## Lab Mmap 

### 1. 概述

---

```c
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
int munmap(void *addr, size_t length);
```

mmap和munmap系统调用允许UNIX程序对其地址空间进行详细的控制。它们可用于在进程间共享内存，将文件映射到进程地址空间，并作为用户级页面故障方案的一部分。在这个lab中，需要给xv6添加mmap和munmap，重点是内存映射普通文件，并不考虑匿名文件。

mmap可以以多种方式调用，但这个lab只需要考虑映射普通文件。你可以假设addr永远是0，这意味着内核应该决定映射文件的虚拟地址。mmap返回这个地址，如果失败的话，返回0xffffffffffffffffff。prot 表示内存是否应该被映射为可读、可写和/或可执行；你可以假设 prot 是 PROT_READ 或 PROT_WRITE 或两者都是。标志要么是 MAP_SHARED，意味着对映射内存的修改应该被写回文件，要么是 MAP_PRIVATE，意味着不应该。fd是要映射的文件的打开文件描述符。你可以假设偏移量为零（它是文件中映射的起始点）。

### 2. 代码实现

---

首先创建virtual memory area的数据结构，并给进程proc结构添加一个vmas的数组：

```c
struct vma{
  int valid;
  uint64 addr;
  int length;
  int prot;
  int flags;
  struct file *mapfile;
};

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
  // mmap lab 
  struct vma vmas[NVMA];               // Process virtual memory area
};
```

然后在sysfile.c中添加mmap的系统调用代码（添加系统调用的详细流程详见syscall lab）

```c
uint64
sys_mmap(void)
{
  uint64 addr;
  int length, prot, flags, fd, i;
  struct proc* p;
  if (argint(1, &length) < 0 || argint(2, &prot) < 0 ||
      argint(3, &flags) < 0 || argint(4, &fd) < 0)
    return -1;
  p = myproc();
  struct file *mapfile = p->ofile[fd];
  if ((!mapfile->writable)&&(prot&PROT_WRITE)&&(!(flags&MAP_PRIVATE)))
    return -1;
  for (i = 0; i < NVMA; i++) {
    // allocate vma struct
    if (p->vmas[i].valid == 0) {
      p->vmas[i].valid = 1;
      p->vmas[i].addr = addr = p->sz;
      p->vmas[i].length = length;
      p->vmas[i].prot = prot;
      p->vmas[i].flags = flags;
      p->vmas[i].mapfile = mapfile;
      filedup(p->ofile[fd]);
      break;
    }
  }
  // have no vma slot
  if (i == NVMA) {
    return -1;
  }
  p->sz += length; // lazy mapping
  return addr;
}
```

但此时mmap只是将mapped的文件记录到了进程的vmas数据结构中，并没有真正映射进进程的页表，所以当用户第一次访问映射的虚存地址时，会触发page fault异常，因此需要在usertrap中添加代码：

```c
if (r_scause() == 13 || r_scause() == 15) // page fault
  {
    uint64 va = r_stval();

    if (va >= p->sz || va < p->trapframe->sp) {
      // page-faults on a virtual memory address higher than any allocated with sbrk()
      // or lower than the stack. In xv6, heap is higher than stack
      p->killed = 1;
    } else {
      int i;
      // check if the pagefault page is in one virtual memory area
      for (i = 0; i < NVMA; i++) {
        if (p->vmas[i].valid) {
          if (p->vmas[i].addr <= va && (p->vmas[i].addr + p->vmas[i].length) > va)
            break;
        }
      }
      if (i == NVMA) {
        // not in any vma
        p->killed = 1;
      } else {
        // allocate page
        uint64 ka = (uint64) kalloc();
        if (ka == 0){
          p->killed = 1;
        } else {
          // printf("access va %d\n", va);
          // printf("sz : %d\n", p->sz);
          // printf("addr : %d\n", p->vmas[i].addr);
          memset((void *)ka, 0, PGSIZE);
          va = PGROUNDDOWN(va);
          ilock(p->vmas[i].mapfile->ip);
          readi(p->vmas[i].mapfile->ip, 0, ka, va - p->vmas[i].addr, PGSIZE);
          iunlock(p->vmas[i].mapfile->ip);
          uint64 pm = PTE_U;
          if (p->vmas[i].prot & PROT_READ)
            pm |= PTE_R;
          if (p->vmas[i].prot & PROT_WRITE)
            pm |= PTE_W;
          if(mappages(p->pagetable, va, PGSIZE, ka, pm) != 0) {
            kfree((void *)ka);
            p->killed = 1;
          }
        }
      }
    }
  }
```

至此mmap就已经完成了，还需要实现munmap：

```c
uint64
sys_munmap(void)
{
  uint64 addr;
  int length, i;
  struct proc* p;
  if (argaddr(0, &addr) < 0 || argint(1, &length) < 0)
    return -1;
  p = myproc();
  for (i = 0; i < NVMA; i++) {
    if (p->vmas[i].valid == 1) {
      if (p->vmas[i].addr <= addr && (p->vmas[i].addr + p->vmas[i].length) > addr)
            break;
    }
  }

  // have no vma slot
  if (i == NVMA) {
    return -1;
  }

  struct vma *vmap = &p->vmas[i];
  if (vmap->flags & MAP_SHARED) { 
    filewrite(vmap->mapfile, addr, length);
  }

  // printf("addr, length = %d, %d\n", addr, length);
  // printf("vmap->addr, vmap->length = %d, %d\n", vmap->addr, vmap->length);
  if (vmap->addr == addr && vmap->length == length) {
    // unmap the whole vma
    uvmunmap(p->pagetable, addr, length/PGSIZE, 1);
    fileclose(vmap->mapfile);
    vmap->valid = 0;
  } else if (vmap->addr == addr) {
    // unmap from the beginning
    uvmunmap(p->pagetable, addr, length/PGSIZE, 1);
    vmap->addr += length;
    vmap->length -= length;
  } else if (vmap->addr + vmap->length == addr + length){
    // unmap from the end
    uvmunmap(p->pagetable, addr, length/PGSIZE, 1);
    vmap->length -= length;
  }

  return 0;
}

```

此外还需要修改exit代码使得进程在退出时unmap掉所有的映射的文件，以及修改fork代码使得子进程拥有相同的内存映射。

### 3. 代码测试

---

<img src="/Users/apple/Library/Application Support/typora-user-images/image-20210113105539383.png" alt="image-20210113105539383" style="zoom:50%;" />