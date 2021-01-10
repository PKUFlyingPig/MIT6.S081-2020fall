## Lab: Copy-On-Write Fork for xv6

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概览

---

xv6操作系统中原来对于fork()的实现是将父进程的用户空间全部复制到子进程的用户空间。但如果父进程地址空间太大，那这个复制过程将非常耗时。另外，现实中经常出现fork() + exec() 的调用组合，这种情况下fork()中进行的复制操作完全是浪费。基于此，我们可以利用页表实现写时复制机制。



### 2. 具体实现

---

#### 2.1 改写fork()

在xv6的fork函数中，会调用uvmcopy函数给子进程分配页面，并将父进程的地址空间里的内容拷贝给子进程。改写uvmcopy函数，不再给子进程分配页面，而是将父进程的物理页映射进子进程的页表，并将两个进程的PTE_W都清零。

```c
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;

  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    reference_count[pa >> 12] += 1;	// reference count ++;
    *pte &= ~PTE_W;   // both child and parent can not write into this page
    *pte |= PTE_COW;  // flag the page as copy on write
    flags = PTE_FLAGS(*pte);
    if(mappages(new, i, PGSIZE, (uint64)pa, flags) != 0){
      goto err;
    }
  }
  return 0;

 err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}
```

上面函数的实现逻辑也很简单，首先利用walk函数在父进程的页表中找到对应虚拟地址的物理地址，然后将该物理地址映射进子进程的页表，同时注意设置PTE_COW位以及清除PTE_W位。

#### 2.2 编写COW handler

此时父子进程对所有的COW页都没有写权限，如果某个进程试图对某个页进行写，就会触发page fault(scause = 15)，因此需要在trap.c/usertrap中处理这个异常。

```c
if (r_scause() == 15) // write page fault
  {
    if (cowhandler(p->pagetable, r_stval()) < 0)
      p->killed = 1;
  } 
```

我们会检查scause寄存器的值是否是15，如果是的话就调用cowhandler函数。

```c
// allocate a new page for the COW
// return -1 if the va is invalid or illegal.
int cowhandler(pagetable_t pagetable, uint64 va)
{
  if (va >= MAXVA) 
    return -1;
  pte_t *pte;
  pte = walk(pagetable, va, 0);
  if (pte == 0) return -1;
  if ((*pte & PTE_U) == 0 || (*pte & PTE_V) == 0 || (*pte & PTE_COW) == 0)
    return -1;

  // allocate a new page
  uint64 pa = PTE2PA(*pte); // original physical address
  uint64 ka = (uint64) kalloc(); // newly allocated physical address

  if (ka == 0){
    return -1;
  } 
  memmove((char*)ka, (char*)pa, PGSIZE); // copy the old page to the new page
  kfree((void*)pa);
  uint flags = PTE_FLAGS(*pte);
  *pte = PA2PTE(ka) | flags | PTE_W;
  *pte &= ~PTE_COW;
  return 0;
}
```

cowhandler做的事情也很简单，它首先会检查一系列权限位，然后分配一个新的物理页，并将它映射到产生缺页异常的进程的页表中，同时设置写权限位。

#### 2.3 增加物理页计数器

由于现在可能有多个进程拥有同一个物理页，如果某个进程退出时free掉了这个物理页，那么其他进程就会出错。所以我们得设置一个全局数组，记录每个物理页被几个进程所拥有。同时注意这个数组可能会被多个进程同时访问，因此需要用一个锁来保护。

```c
int reference_count[PHYSTOP >> 12];
struct spinlock ref_cnt_lock;
```

每个物理页所对应的计数器将在下面几个函数内被修改：

首先在kalloc分配物理页函数中将对应计数器置为1

```c
// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    acquire(&ref_cnt_lock);
    reference_count[(uint64)r>>12] = 1; // first allocate, reference = 1
    release(&ref_cnt_lock);
  }
  release(&kmem.lock);

  if(r)  memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

进程在fork时会调用uvmcopy函数，我们要在其中将COW 页对应的计数器加1。

另外在某个进程想free掉某个物理页时，我们要将其计数器减1。

```c
void
kfree(void *pa)
{
  struct run *r;
  int tmp, pn;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&ref_cnt_lock);
  pn = (uint64) pa >> 12;
  if (reference_count[pn] < 1)
    panic("kfree ref");
  reference_count[pn] -= 1;
  tmp = reference_count[pn];
  release(&ref_cnt_lock);

  if (tmp > 0) return;
  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}
```

#### 2.4 修改copyout

最后，如果内核调用copyout函数试图修改一个进程的COW页，也需要进行cowhandler类似的操作来处理。

```c
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0) {
    	return -1;
    }
	pte = walk(pagetable, va0, 0);
    if (*pte & PTE_COW)
    {
      // allocate a new page
      uint64 ka = (uint64) kalloc(); // newly allocated physical address

      if (ka == 0){
      	struct proc *p = myproc();
        p->killed = 1; // there's no free memory
      } else {
        memmove((char*)ka, (char*)pa0, PGSIZE); // copy the old page to the new page
        uint flags = PTE_FLAGS(*pte);
        uvmunmap(pagetable, va0, 1, 1);
        *pte = PA2PTE(ka) | flags | PTE_W;
        *pte &= ~PTE_COW;
        pa0 = ka;
      }
    } 
    n = PGSIZE - (dstva - va0);
    if(n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

```
