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

if you want to have a clean copy of the original lab handout, just go to the [course website](https://pdos.csail.mit.edu/6.828/2020/schedule.html) and follow the lab guidance. Also, you can watch the course videos on the course website. Hope you enjoy your OS journey.



## Lab Reports

For each lab, I wrote a brief report on my implementation, explaining the important part of my code and how to complete the lab step by step. (I wrote these reports in Chinese)

- [Lab Util](reports/Utils.md)
- [Lab syscall](reports/syscall.md)
- [Lab pgtbl](reports/pgtbl.md)
- [Lab traps](reports/traps.md)
- [Lab lazy](reports/lazy.md)
- [Lab COW](reports/COW.md)
- [Lab thread](reports/thread.md)
- [Lab lock](reports/lock.md)
- [Lab fs](reports/fs.md)
- [Lab Mmap](reports/Mmap.md)

## Reading materials

I highly recommend you read the [xv6 book](book-riscv-rev1.pdf) before watching the course videos and doing the labs.

## Wanna Learn More ?

Check out [this repository](https://github.com/PKUFlyingPig/Self-learning-Computer-Science) which contains all my self-learning materials : )

## Hints for each lab

You should do the labs on your own at first, but if you encounter some bugs or want to get some high-level hints, this part may help you.

### lab1 : util

**every program must end with exit(0)!!**

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

### lab4 : traps

- RISCV - assembly (easy) :
  - a0-7 contains arguments to functions. a2 holds 13 in main's call to printf.
  - at address 0x26, we can see the complier directly loads 12 into a1 register, which is just the return value of f(8) + 1.
  - printf lies at address 0x630
  - just after the jalr to printf, ra saves the return address, which is 0x38
  - the output is "He110 world". If the RISC-V were instead big-endian, we need to set i to 0x726c6400 in order to yield the same output. We don't need to change 57616 to a different value.
  - the value in register a2 will be printed after 'y=', which is quite random.

- Backtrace (moderate) :
  - use r_fp() in backtrace to get current frame pointer
  - using `PGROUNDDOWN(fp)` and `PGROUNDUP(fp)` (see `kernel/riscv.h`. These number are helpful for `backtrace` to terminate its loop). 
- Alarm (hard) :
  - this  lab is not as hard as the pgtbl, so do not think too complicatedly, the answer is quite straight-forward and easy to get.
  - follow the guidance step by step, all the details are all in the guidance
  - I add a new field (allow_entrance_handler) in proc structure to avoid handler re-entrant. 
  - the bugs that I encountered :
    - At first I add one more trapframe to save all the registers, but this implementation will not pass the usertests
    - You need to save s-registers, but I am a little confused about it. Since if the handler function follows the RISC-V calling convention, it needs to save the s-register before using it. So why the original process's s-registers get corrupted ?

### lab5 : lazy

- eliminate allocation from sbrk() (easy)
  - so easy
- lazy allocation (moderate)
  - You need to modify trap.c/usertrap() to handle the page fault. You can check whether a fault is a page fault by seeing if r_scause() is 13 or 15. If so, use kalloc() to allocate physical memory and then use mappage() to add this physical memory into the user pagetable.
  - Modify uvmunmap() function. Because when the OS want to unmap the address which is requested by the user program but have not yet been allocated, it orginally will call panic() to abort. But since it may because of lazy allocation, we will allow this to happen.(you can simply change the "panic" into "continue")
- Lazytests and Usertests (moderate)
  - handle negative sbrk() arguments : remember to deallocate.
  - you need to kill the process if the address is illegal (below the stack or above the heap). the p->sz and p->trapframe-sp may help you. Remeber that in xv6, user address space's heap is above its stack, which may be not consistent with your common knowledge.
  - If you see the error "incomplete type proc", include "spinlock.h" then "proc.h".
  - Handle the parent-to-child memory copy in fork() correctly : modify uvmcopy(), which is similar to the modification you made to the uvmunmap() funcation.
  - Handle the case in which a process passes a valid address from sbrk() to a system call such as read or write, but the memory for that address has not yet been allocated : you only need to modify copyin() and copyout() function. Why we need to do that? because usertrap() only handle the page fault in the user program, but copyin() and copyout() are called in the kernel, which can not be handled by usertrap(). So you can just copy the code from usertrap() to handle this case. 

### Lab6 : COW

MIT's Q&A session will go through this lab step by step, you can check this [video](https://www.youtube.com/watch?v=S8ZTJKzhQao&feature=youtu.be]) for help.

- You need a lock to protect your ref_count array
- decrement the ref_count in the kfree function, I used to decrement it in the uvmunmap function but failed to pass the usertests.
- you can write a specific function e.g. cowhandler to handle the COW page fault, then call it in both usertrap and copyout function (I didn't do that, so my code may look a little messy).

### lab7 : multithreading

- Uthread (moderate) :
  - your code can be quite similar to the code in switch.S and proc.h (almost the same)
  - think twice when you assign value to the thread's stack pointer in thread_create ! ( remember that the stack is growing down in the address space)
- Using threads :
  - when you call insert() in put(), you already give the current head of the bucket as a variable to the insert() function, so if one thread has called insert but has not updated the head of the bucket, and now another thread calls insert() with an old head of the bucket, then the wrong occurs.
- Barrier :
  - nothing special, remember that when one thread wake up, it will immediately acquire the lock.

### Lab8 : locking

- Memory allocator (moderate) :
  - You can choose to allocate free pages evenly to each CPU at the beginning, although I didn't do that.
  - I choose to steal 1000 pages at a time.
- Buffer cache (hard) :
  - I don't recommend my solution, which took me only 10 minutes to pass the hard lab.
  - I just copy the bcache 13 times to hack the lab.

### Lab9 : file system

I know "Chapter 8: File system" in xv6 book is quite long and hard to understand, but read it patiently and thoroughly will save you a lot of time ! I personally find read the code first will help you understand (fs.c, file.c, sysfile.c, fs.h, file.h) .

- Large files (moderate) :
  - easy and straightforward
- Symbolic links (moderate) :
  - if you encounter "exec symlinktest failed", you may forget to add _symlinktest into the Makefile
  - the function readi and writei may be your friend, you don't bother to write your own code to write data into inode's data block.
  - if you encounter "panic: iget: no inodes", you may forget to iput(ip). I recommend you use iunlockput(ip) for convenience.

### Lab10 : Mmap

mmap is a very important feature in today's OS, if you run strace for any program, you will see many mmap system call. This lab will let you have a taste for the memory mapped file.

- use filedup to increment file's reference, use fileclose to decrement file's reference.
- you can write the pagefault handler code in usertrap based on your Lazy lab implementation.

- the mmaptest only tests files whose size is PGSIZE-aligned, so I didn't handle the common case. You may make use of this to simplify your code.