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

**read the chapter 2 and 4.4 4.5 carefully and make sure to understand what you are going to do**

- system call tracing (moderate) : all the modification you need to do are listed in the lab guidance except for the entry you need to add for the syscall declaration and the syscall function pointer array  in the kernel/syscall.c (about line 90 to 120).
- TBA.



## TBA



