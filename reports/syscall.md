## Lab : Syscalls

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概述

---

这个lab的目的是给xv6增加两个系统调用，以此熟悉整个系统调用的过程。

### 2. 代码实现

---

#### 2.1 System call tracing

---

这个lab中需要增加一个叫做trace的系统调用，它会让进程在调用被打上mask的系统调用时，输出对应的进程号、系统调用名和返回值。

在sysproc.c中新增一个sys_trace的实现，当进程调用trace这个系统调用时，最终内核会执行这段代码，把进程对应的mask变量进行赋值。

```c
uint64
sys_trace(void)
{
  int mask;
  if(argint(0, &mask) < 0)
    return -1;
  myproc()->mask = mask;
  return 0;
}
```

另外修改所有系统调用的入口函数，让它根据进程的mask输出对应的值。

```c
void
syscall(void)
{
  char const *syscall_names[] = {"fork", "exit", "wait", "pipe", "read",
  "kill", "exec", "fstat", "chdir", "dup", "getpid", "sbrk", "sleep",
  "uptime", "open", "write", "mknod", "unlink", "link", "mkdir","close","trace","sysinfo"};

  int num;
  struct proc *p = myproc();

  num = p->trapframe->a7;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    p->trapframe->a0 = syscalls[num]();
    if ((p->mask) & (1 << num)){
      printf("%d: syscall %s -> %d\n", p->pid, syscall_names[num - 1], p->trapframe->a0);
    }
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}
```

#### 2.2 SysInfo

---

这个lab中需要增加一个新的SysInfo的系统调用，获取内存当前剩余的空闲页大小以及UNUSED的进程。

首先在kalloc.c中增加一个统计当前剩余空闲页大小的helper function。

```c
uint64 
freemem(void)
{
  struct run *r;
  uint64 freepage = 0;
  acquire(&kmem.lock);
  r = kmem.freelist;
  while (r)
  {
    freepage += 1;
    r = r->next;
  }
  release(&kmem.lock);
  return (freepage << 12);
}
```

然后在proc.c中实现一个统计UNUSED进程数的helperfunction。

```c
uint64
unusedproc(void)
{
  struct proc *p;
  uint64 unused = 0;

  for(p = proc; p < &proc[NPROC]; p++)
  {
    if(p->state != UNUSED) {
      unused++;
    }
  }

  return unused;
}
```

最后新增sysinfo系统调用

```c
uint64
sys_sysinfo(void)
{
  uint64 addr;
  if(argaddr(0, &addr) < 0)
    return -1;
  struct proc *p = myproc();
  struct sysinfo info;
  info.freemem = freemem();
  info.nproc = unusedproc();    
  if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
      return -1;
  return 0;
}
```
