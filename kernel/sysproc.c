#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  backtrace();
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// ========== alarm solution ===============
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
// =========================================