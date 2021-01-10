## Lab: xv6 lazy page allocation

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. lab概述

---

操作系统的一个常见trick就是lazy页分配机制。当应用程序通过sbrk系统调用申请堆空间时，我们需要分配物理地址空间并把它映射到用户的虚拟地址。当用户申请了一片很大的地址空间时，这会花费内核很多时间。而事实上，用户程序通常会申请比它实际需求更多的空间。为了解决这一问题，我们可以在sbrk中只标记一下用户当前的地址空间，那些用户请求了但是并没有被使用的地址在页表中依然是invalid，而当用户实际去访问时就会触发一个缺页异常，我们再在异常处理函数中分配物理地址并映射到用户的虚拟地址。



### 2. 具体实现

---

#### 1. Eliminate allocation from sbrk()

这一部分我们需要去掉sbrk系统调用中原有的分配物理地址的机制，只是单纯地增加进程的sz变量（虚拟地址的大小）。

```c
uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;

  struct proc* p = myproc();
  addr = p->sz;
  if (n < 0){
    uint sz;
    sz = p->sz;
    p->sz = uvmdealloc(p->pagetable, sz, sz + n);
  } else {
    (p->sz) += n;
  }
  return addr;
}
```

注意，这里还得考虑用户要求新增的大小为负的情况（减少虚拟地址空间），此时需要调用uvmdealloc函数释放物理地址。



#### 2. Lazy allocation

---

前面我们修改了sbrk系统调用使得它可以迅速地处理用户的空间申请，但是用户申请的地址在页表中依然是invalid的状态，当用户试图访问这些地址时就会触发缺页异常陷入内核，而我们就需要在usertrap函数中处理缺页。

在RISC-V中，缺页异常号是13和15（分别对应读和写），它们会保存在scause寄存器中，而导致缺页的地址会保存在stval寄存器中。具体的处理代码如下：

```c
void
usertrap(void){
  .......
  .......
if (r_scause() == 8) {
  //系统调用
  .......
  .......
} else if (r_scause() == 13 || r_scause() == 15)
   // page fault
  {
    uint64 va = r_stval();
    if (va >= p->sz || va < p->trapframe->sp) {
      // page-faults on a virtual memory address higher than any allocated with sbrk()
      // or lower than the stack. In xv6, heap is higher than stack
      p->killed = 1;
    } else {
      uint64 ka = (uint64) kalloc();
      if (ka == 0){
        p->killed = 1;
      } else {
        memset((void *)ka, 0, PGSIZE);
        va = PGROUNDDOWN(va);
        if(mappages(p->pagetable, va, PGSIZE, ka, PTE_W|PTE_R|PTE_U) != 0) {
          kfree((void *)ka);
          p->killed = 1;
        }
      }
    }
  } else {
  // 其他异常
  .......
  .......
}
```

可以看到代码的逻辑非常简单，首先检查scause里面存的异常号是不是缺页异常，如果是的话再确认用户访问的地址是否合法（没有超过堆和栈的界限，由于xv6中堆在栈的上面，因此判断语句中用的是或）。

如果确认地址合法，就调用kalloc函数分配一个物理页面，并把它映射到用户的虚拟地址。

此时看起来已经万事大吉了，但如果在xv6中运行用户程序，uvmunmap还是会报错，这是因为如果一个进程需要释放一个自己申请了，但是并没有真正访问过的空间时，这部分空间由于lazy allocation的机制并没有真正分配物理地址，所以uvmunmap函数中会发现这些虚拟地址在页表中是无效的，因此会panic报错。我们要做的修改也很简单，就是用continue替代之前的报错即可。（如下图）

```c
void
uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PGSIZE; a += PGSIZE){
    if((pte = walk(pagetable, a, 0)) == 0){
      //panic("uvmunmap: walk");
      continue;
    }
    if((*pte & PTE_V) == 0){
      //panic("uvmunmap: not mapped");
      //it is ok for pagefault
      continue;
    }

    if(PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      kfree((void*)pa);
    }
    *pte = 0;
  }
}
```



#### 3. Lazytests and Usertests

---

这一部分主要测试了我们lazy allocation机制的鲁棒性，需要能通过多种用户程序的测试。而这里面主要的问题在于write 和 read系统调用，当它们访问了用户申请了但实际未分配的地址空间时，我们需要类似地处理。注意为什么前面usertrap中的机制不能满足呢？因为usertrap中的lazy allocation机制只能满足从用户态触发的缺页，而write 和 read 系统调用最终会调用内核中的copyin和copyout函数，它们都是在内核当中的，显然需要另外的处理机制。其中copyin的修改如下：

```c
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  struct proc *p = myproc();
  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0) {
      // allow for lazy allocation
      if (va0 >= p->sz || va0 < p->trapframe->sp) {
        return -1;
      } else {
        pa0 = (uint64) kalloc();
        if (pa0 == 0) {
          p->killed = 1;
        } else {
          memset((void *)pa0, 0, PGSIZE);
          va0 = PGROUNDDOWN(va0);
          if(mappages(p->pagetable, va0, PGSIZE, pa0, PTE_W|PTE_R|PTE_U) != 0) {
            kfree((void *)pa0);
            p->killed = 1;
          }
        }
      }
    }
    n = PGSIZE - (srcva - va0);
    if(n > len)
      n = len;
    memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PGSIZE;
  }
  return 0;
}
```

copyin函数的功能是从用户地址空间复制数据到内核地址空间（两者页表不同），而它的实现就是通过walkaddr函数手动实现地址翻译的过程，得到用户数据的物理地址，然后将用户物理地址空间的数据复制到内核地址。但如果用户提供的虚拟地址还未分配的话就需要我们先分配物理页，然后重复上面的操作。

copyout函数的修改完全类似。
