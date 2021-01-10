## Lab : Page Table

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概述

---

这个lab主要目的是让学生熟悉页表并通过修改xv6的源码简化从用户空间到内核空间的数据复制过程。



### 2. 代码实现

---

#### 2.1 print a page table

---

这个部分需要实现一个helper函数来打印某个进程的页表

```c
void
vmprinthelper(pagetable_t pagetable, int level)
{
  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      for(int i = 0; i < level; i++)printf(".. ");
      uint64 child = PTE2PA(pte);
      printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
      if (level == 3) 
       continue;
      else 
      // this PTE points to a lower-level page table.
       vmprinthelper((pagetable_t)child, level + 1);
    } 
  }
}

void 
vmprint(pagetable_t pagetable)
{
  printf("page table %p\n", pagetable);
  vmprinthelper(pagetable, 1);
```

由于xv6采用了三级页表翻译机制，因此print函数递归到第三层就要返回。



#### 2.2 A kernel page table per process

---

Xv6只有一个单一的内核页表，只要在内核中执行就会用到。Xv6的每个进程都还有一个单独的页表，用于每个进程的用户地址空间映射，只包含该进程用户内存的映射，从虚拟地址0开始。由于内核页表不包含这些映射，用户地址在内核中是无效的。因此，当内核需要使用系统调用中传递的用户指针时（例如，传递给write()的缓冲区指针），内核必须首先将该指针翻译成物理地址。以下两个lab的目标是允许内核直接去引用用户指针。

在进程proc的定义中增加一个内核页表：

```c
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  pagetable_t kpagetable;
};
```

由于之前xv6只有一个内核页表，因此vm.c中的函数都只限于内核页表，需要为用户的内核页表定义若干函数来创建、删除以及映射页表。

```c
// kvmmap is only set for the original kernel page table, so we need to use a new
// kvmmap function to map page for all the kernel page tables (each proc has one page table)
void 
kvmmapkern(pagetable_t pagetable, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(pagetable, va, sz, pa, perm) != 0) 
    panic("kvmmap");
}

// proc's version of kvminit
pagetable_t
kvmcreate() 
{
  pagetable_t pagetable;
  int i;

  pagetable = uvmcreate();
  for(i = 1; i < 512; i++) {
    pagetable[i] = kernel_pagetable[i];
  }

  // uart registers
  kvmmapkern(pagetable, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmapkern(pagetable, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmapkern(pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmapkern(pagetable, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  return pagetable;
}


void 
kvmfree(pagetable_t kpagetale, uint64 sz) 
{
  pte_t pte = kpagetale[0];
  pagetable_t level1 = (pagetable_t) PTE2PA(pte);
  for (int i = 0; i < 512; i++) {
    pte_t pte = level1[i];
    if (pte & PTE_V) {
      uint64 level2 = PTE2PA(pte);
      kfree((void *) level2);
      level1[i] = 0;
    }
  }
  kfree((void *) level1);
  kfree((void *) kpagetale);
}
```

之后需要修改scheduler的内容，使得切换进程时将切换到对应进程的内核页表

```c
// switch kernel page table
w_satp(MAKE_SATP(p->kpagetable));
sfence_vma();
```

同时在进程下CPU时要切换回原来的全局内核页表

```c
// switch to scheduler's kernel page table
kvminithart();
```

最后还需要在创建/退出进程时相应地创建和删除进程的内核页表。

#### 2.3 Simplify `copyin/copyinstr`

---

内核的copyin函数读取用户指针指向的内存。它通过将它们转换为物理地址，内核可以直接引用它们。此前xv6通过在内核代码中调用walk函数通过软件实现地址翻译。在这部分实验中，需要将用户映射添加到每个进程的内核页表中（在上一节中创建），使copyin（以及相关的字符串函数copyinstr）能够直接访问用户指针指向的物理地址。

首先修改现有的copyin（以及copyinstr），让它们返回copyin_new (以及copyinstr_new)的值。

```c
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  // ==== specialized for pgtbl =======
  return copyin_new(pagetable, dst, srcva, len);
  // ==================================
}
```

然后在每个内核改变用户地址映射的地方对用户的内核页表做同样的操作（fork(), exec(), sbrk()）

最后在userinit函数（第一个用户进程）中也要做同样的事情：

```c
// Set up first user process.
void
userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;
  
  // allocate one user page and copy init's instructions
  // and data into it.
  uvminit(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;      // user program counter
  p->trapframe->sp = PGSIZE;  // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  // =========== solution for pgtbl ---- part 3 =============
  kvmmapuser(p->pid, p->kpagetable, p->pagetable, p->sz, 0);
  // ========================================================

  release(&p->lock);
}
```
