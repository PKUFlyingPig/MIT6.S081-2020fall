# MIT 6.S081 2020fall

This repository contains my solution to the labs for MIT's 6.S081 operating system.

## How to use it ?

First, you need to clone this repository by

```c
git clone https://github.com/PKUFlyingPig/MIT6.S081-2020fall.git
```

Then cd into the file, you are now in the master branch of the projects, to see my answer to a specific lab, e.g. util, syscall ...... , you just checkout into that branch by for example 

```
git checkout util
```

and now you are in the utils branch which contains my solution to the specific lab. 

if you want to have a clean copy of the original lab handout, just go to the [course website](https://pdos.csail.mit.edu/6.828/2020/schedule.html) and follow the lab guidance. Hope you enjoy your OS journey.

## Answer for each lab

### lab1: util

**every program must end with exit(0) !!! !!! **

- sleep (easy) : just follow the guidance
- pingpong (easy) : You only need to transfer any byte you want, but not transfer "pingpong" the string itself. When the parent send the byte, remember to explicitly wait for the child to reply.
- Primes (moderate) : Remeber that when you create a pipe and then call fork(). There are 4 file descriptors open. So be careful to turn off the file descriptor that you don't need.
- Find (moderate) : nothing special. Don't recurse into "." and "..".
- xargs (moderate) : A little tricky to read in the string because you need to read one byte at a time. You can refer to my solution. I read in one line at a time.  Also remember to wait for all the children to complete.



### lab2 : syscall

**read the chapter 2 and 3.5 4.4 4.5 carefully and make sure to understand what you are going to do**

- system call tracing (moderate) : all the modification you need to do are listed in the lab guidance except for the entry you need to add for the syscall declaration and the syscall function pointer array  in the kernel/syscall.c (about line 90 to 120).
- sysinfo (moderate) : 
  - remember to add the definition of your newly added two helper functions in kernel/defs.h, otherwise you will receive compile error.
  - read the lab guid carefully, especially that `nproc` field should be set to the number of processes whose `state` is **not** `UNUSED`. I spent one hour to find this stupid bug.
  - use argaddr to get the sysinfo structure address provided by user in sys_sysinfo function implementation.
  - nproc : allocproc function in proc.c may give you some hints.
  - freemem : read chapter 3.5 in xv6 book may make you understand what happens in the kalloc.c. (kmem is a list of free pages and each page is 4096 bytes)

### lab3 : pgtbl 

**warning : this lab is extremly time-consuming**

**read the chapter 3 and the code in proc.c/ vm.c carefully, understand the difference between the user address space and kernel address space**

**because there are so many details in this lab, I wrapper my solution code with the title "solution for pgtbl ---- part x", I only modified the following files(vm.c, exec.c, proc.c, proc.h, defs.h), so you can search the title in these files to see my modification.**

- Print a page table (easy) :
  - just follow the guidance
  - see freewalk function for inspiration
- kernel page table per process(hard) : 
  - to understand my solution(especially the kvmcreate function), you need to first read the part3 problem, and notice that the user will not use virtual address over PLIC(0xC000000) ---- I think this condition is just for simplification. After you understand this, you should find that the user will only modify the address lies in the first entry of the pagetable. That is why I just copy the 1 to 511 entry of kernel_pagetable into per process's pagetable. Then the kvmfree function is also easy to understand.
  - the second step is to call the two function above to create a new kernel pagetable for a new process that is allocated (in allocproc()) and free the kernel pagetable when the process is killed (in freeproc()).
  - the last step is to modify the scheduler()  to switch the kernel pagetable for each process. Remember that the scheduler will use the original kernel page table !
- simplify copyin / copyinstr (hard) :
  - add a new function to copy PTEs from the user page table into process's kernel pagetable. (see kvmmapuser() in vm.c for more details)
  - At each point where the kernel changes a process's user mappings, change the process's kernel page table in the same way. Such points include `fork()`, `exec()`, and `growproc()`. The guidance says that you need to modify the sbrk(). I instead modified growproc(). Both are ok.
  - Don't forget to modify the copyin() and copyinstr() methods to call copyin_new and copyinstr_new. 
  - You also need to add the newly added functions into defs.h.

### lab4 traps

- RISCV - assembly :

  - a0-7 contains arguments to functions. a2 holds 13 in main's call to printf.

  - at address 0x26, we can see the complier directly loads 12 into a1 register, which is just the return value of f(8) + 1.

  - printf lies at address 0x630

  - just after the jalr to printf, ra saves the return address, which is 0x38

  - the output is "He110 world". If the RISC-V were instead big-endian, we need to set i to 0x726c6400 in order to yield the same output. We don't need to change 57616 to a different value.
  
  - the value in register a2 will be printed after 'y=', which is quite random.
  
    



