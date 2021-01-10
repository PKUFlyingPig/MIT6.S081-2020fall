## Lab : traps

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概述

---

xv6通过trap机制实现了系统调用，这个lab会让学生先熟悉用户栈然后实现一个trap handler。



### 2. 代码实现

---

#### 2.1 Backtrace

---

在debug的时候我们希望能知道在代码发生错误时，它的栈上方调用了哪些函数。

![image-20210110212219277](/Users/apple/Library/Application Support/typora-user-images/image-20210110212219277.png)

上面是一张课堂slides中截图，很好地展示了RISC-V的栈帧的结构，由此可以很容易地实现backtrace

```c
void 
backtrace(void)
{
  uint64 cur_fp = r_fp();
  while(cur_fp != PGROUNDDOWN(cur_fp))
  {
    printf("%p\n", *(uint64 *)(cur_fp - 8));
    cur_fp = *(uint64 *)(cur_fp - 16);
  }
}
```

需要注意的是，xv6只给每个用户进程分配一个页（4KB）作为用户栈，因此可以通过检查fp是否到达页的开头判断是否到达栈头。

#### 2.2 Alarm

在这个练习中，将为xv6添加一个功能，当一个进程使用CPU时间时，xv6会周期性地对其发出alarm。这个特性可以用于那些想限制它们占用多少 CPU 时间的进程，或者对那些想采取一些周期性行动的进程有用。更一般地说，你将实现用户级中断/故障处理程序的原始形式；例如，你可以使用类似的东西来处理应用程序中的页面故障。

为了达到题干中的目的，需要新增一个sigalarm(interval, handler)的系统调用，它会使得调用这个系统调用的进程在CPU每走过interval个ticks时自动地调用handler函数。

首先需要在进程proc数据结构中新增相应的变量：

```c
/ ======== alarm solution =========
  uint64 handler;
  int alarm_interval;
  int passed_ticks;
  int allow_entrance_handler;
  uint64 saved_epc;           // saved user program counter
  uint64 saved_ra;
  uint64 saved_sp;
  uint64 saved_gp;
  uint64 saved_tp;
  uint64 saved_t0;
  uint64 saved_t1;
  uint64 saved_t2;
  uint64 saved_t3;
  uint64 saved_t4;
  uint64 saved_t5;
  uint64 saved_t6;
  uint64 saved_a0;
  uint64 saved_a1;
  uint64 saved_a2;
  uint64 saved_a3;
  uint64 saved_a4;
  uint64 saved_a5;
  uint64 saved_a6;
  uint64 saved_a7;
  uint64 saved_s0;
  uint64 saved_s1;
  uint64 saved_s2;
  uint64 saved_s3;
  uint64 saved_s4;
  uint64 saved_s5;
  uint64 saved_s6;
  uint64 saved_s7;
  uint64 saved_s8;
  uint64 saved_s9;
  uint64 saved_s10;
  uint64 saved_s11;
  // =================================
```

然后实现sigalarm系统调用，它会将对应进程的属性赋值。

```c
uint64
sys_sigalarm(void)
{
  int interval;
  uint64 handler;
  if (argint(0, &interval) < 0)
    return -1;
  if (argaddr(1, &handler) < 0)
    return -1;
  myproc()->alarm_interval = interval;
  myproc()->handler = handler;
  return 0;
}
```

接下来就需要在xv6每次发生时钟中断时，trap的代码能够处理：

```c
// give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
  {
    // =======  alarm solution ========
    p->passed_ticks += 1;
    if (p->passed_ticks == p->alarm_interval)
    {
      if (p->allow_entrance_handler)
      {
        // avoid re-entrant
        p->allow_entrance_handler = 0;

        // save all the needed registers
        p->saved_epc = p->trapframe->epc; // saved user program counter
        p->saved_ra = p->trapframe->ra;
        p->saved_sp = p->trapframe->sp;
        p->saved_gp = p->trapframe->gp;
        p->saved_tp = p->trapframe->tp;
        p->saved_t0 = p->trapframe->t0;
        p->saved_t1 = p->trapframe->t1;
        p->saved_t2 = p->trapframe->t2;
        p->saved_t3 = p->trapframe->t3;
        p->saved_t4 = p->trapframe->t4;
        p->saved_t5 = p->trapframe->t5;
        p->saved_t6 = p->trapframe->t6;
        p->saved_a0 = p->trapframe->a0;
        p->saved_a1 = p->trapframe->a1;
        p->saved_a2 = p->trapframe->a2;
        p->saved_a3 = p->trapframe->a3;
        p->saved_a4 = p->trapframe->a4;
        p->saved_a5 = p->trapframe->a5;
        p->saved_a6 = p->trapframe->a6;
        p->saved_a7 = p->trapframe->a7;
        p->saved_s0 = p->trapframe->s0;
        p->saved_s1 = p->trapframe->s1;
        p->saved_s2 = p->trapframe->s2;
        p->saved_s3 = p->trapframe->s3;
        p->saved_s4 = p->trapframe->s4;
        p->saved_s5 = p->trapframe->s5;
        p->saved_s6 = p->trapframe->s6;
        p->saved_s7 = p->trapframe->s7;
        p->saved_s8 = p->trapframe->s8;
        p->saved_s9 = p->trapframe->s9;
        p->saved_s10 = p->trapframe->s10;
        p->saved_s11 = p->trapframe->s11;
        // when the process returns to the user code,
        // jump to the handler code first
        p->trapframe->epc = p->handler;

        // re-arm the alarm
        p->passed_ticks = 0;
      } else {
        // can not enter handler code
        p->passed_ticks -= 1;
      }
    }
    // ==================================================
    yield();
  }
```

最后，由于handler函数默认在最后都会调用sigreturn系统调用，我们只需要在这个系统调用中恢复原来寄存器中的值。

```c

uint64
sys_sigreturn(void)
{
  // restore all the saved registers
  struct proc *p = myproc();
  p->trapframe->epc = p->saved_epc; 
  p->trapframe->ra = p->saved_ra; 
  p->trapframe->sp = p->saved_sp; 
  p->trapframe->gp = p->saved_gp; 
  p->trapframe->tp = p->saved_tp; 
  p->trapframe->a0 = p->saved_a0; 
  p->trapframe->a1 = p->saved_a1; 
  p->trapframe->a2 = p->saved_a2; 
  p->trapframe->a3 = p->saved_a3; 
  p->trapframe->a4 = p->saved_a4; 
  p->trapframe->a5 = p->saved_a5; 
  p->trapframe->a6 = p->saved_a6; 
  p->trapframe->a7 = p->saved_a7; 
  p->trapframe->t0 = p->saved_t0; 
  p->trapframe->t1 = p->saved_t1; 
  p->trapframe->t2 = p->saved_t2; 
  p->trapframe->t3 = p->saved_t3; 
  p->trapframe->t4 = p->saved_t4; 
  p->trapframe->t5 = p->saved_t5; 
  p->trapframe->t6 = p->saved_t6;
  p->trapframe->s0 = p->saved_s0;
  p->trapframe->s1 = p->saved_s1;
  p->trapframe->s2 = p->saved_s2;
  p->trapframe->s3 = p->saved_s3;
  p->trapframe->s4 = p->saved_s4;
  p->trapframe->s5 = p->saved_s5;
  p->trapframe->s6 = p->saved_s6;
  p->trapframe->s7 = p->saved_s7;
  p->trapframe->s8 = p->saved_s8;
  p->trapframe->s9 = p->saved_s9;
  p->trapframe->s10 = p->saved_s10;
  p->trapframe->s11 = p->saved_s11;

  myproc()->allow_entrance_handler = 1;
  return 0;
}
```
