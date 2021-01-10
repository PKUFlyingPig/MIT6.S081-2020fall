## Lab : Utils

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概述

---

第一个lab相对容易，主要是让大家熟悉xv6和系统调用。



### 2. 代码实现

---

#### 2.1 boot xv6

---

从仓库clone代码，配置好环境后，在命令行运行

```shell
make qemu
```

即可启动xv6操作系统。

#### 2.2 sleep

---

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char const *argv[])
{
	if (argc != 2)
	{
		fprintf(2, "Usage: sleep seconds\n");
		exit(1);
	}
	int time = atoi(argv[1]);
	sleep(time);
	
	exit(0);
}
```

直接调用sleep系统调用即可。

#### 2.3 pingpong

---

这个lab主要是利用pipe系统调用来实现两个进程间的通信，需要注意的是记得close不需要的文件描述符。

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char const *argv[])
{
	int pid;
	int p[2];
	pipe(p);

	if (fork() == 0) // child (receive -> send)
	{
		pid = getpid();
		char buf[2];
		if (read(p[0], buf, 1) != 1)
		{
			fprintf(2, "failed to read in child\n");
			exit(1);
		}
		close(p[0]);
		printf("%d: received ping\n", pid);
		if(write(p[1], buf, 1) != 1)
		{
			fprintf(2, "failed to write in child\n");
			exit(1);
		}
		close(p[1]);
		exit(0);
	}else{			// parent (send -> receive)
		pid = getpid();
		char info[2] = "a";
		char buf[2];
		buf[1] = 0;
		if (write(p[1], info, 1) != 1)
		{
			fprintf(2, "failed to write in parent\n");
			exit(1);
		}
		// wait for child to receive ping
		close(p[1]);
		wait(0);
		if(read(p[0], buf, 1) != 1){
			fprintf(2, "failed to read in parent\n");
			exit(1);
		}
		printf("%d: received pong\n", pid);
		close(p[0]);
		exit(0);
	}
}
```

#### 2.4 primes

---

这个lab主要利用了pipe和fork系统调用实现了一个并行化的素数筛。

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void new_proc(int p[2]){
	int prime;
	int flag;
	int n;
	close(p[1]);
	if(read(p[0], &prime, 4) != 4){
		fprintf(2, "child process failed to read\n");
		exit(1);
	}
	printf("prime %d\n", prime);

	flag = read(p[0], &n, 4);
	if(flag){
		int newp[2];
		pipe(newp);
		if (fork() == 0)
		{
			new_proc(newp);
		}else
		{
			close(newp[0]);
			if(n%prime)write(newp[1], &n, 4);
			
			while(read(p[0], &n, 4)){
				if(n%prime)write(newp[1], &n, 4);
			}
			close(p[0]);
			close(newp[1]);
			wait(0);
		}
	}
	exit(0);
}

int main(int argc, char const *argv[])
{
	int p[2];
	pipe(p);
	if (fork() == 0)
	{
		new_proc(p);
	}else
	{
		close(p[0]);
		for(int i = 2; i <= 35; i++)
		{
			if (write(p[1], &i, 4) != 4)
			{
				fprintf(2, "first process failed to write %d into the pipe\n", i);
				exit(1);
			}
		}
		close(p[1]);
		wait(0);
		exit(0);
	}
	return 0;
}
```

#### 2.5 find

---

利用文件系统调用实现一个简单的find函数。

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

void find_helper(char const *path, char const *target)
{
	char buf[512], *p;
	int fd;
	struct dirent de;
	struct stat st;
	if((fd = open(path, 0)) < 0){
		fprintf(2, "find: cannot open %s\n", path);
		exit(1);
	}

	if(fstat(fd, &st) < 0){
		fprintf(2, "find: cannot stat %s\n", path);
		exit(1);
	}

	switch(st.type){
		case T_FILE:
			fprintf(2, "Usage: find dir file\n");
			exit(1);
		case T_DIR:
			if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
				printf("find: path too long\n");
				break;
			}
			strcpy(buf, path);
			p = buf + strlen(buf);
			*p++ = '/';
			while(read(fd, &de, sizeof(de)) == sizeof(de)){
				if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
					continue;
				memmove(p, de.name, DIRSIZ);
				p[DIRSIZ] = 0;
				if(stat(buf, &st) < 0){
        			printf("find: cannot stat %s\n", buf);
        			continue;
      			}
      			if(st.type == T_DIR){
      				find_helper(buf, target);
      			}else if (st.type == T_FILE){
      				if (strcmp(de.name, target) == 0)
      				{
      					printf("%s\n", buf);
      				}
      			}
			}
			break;
	}
	close(fd);
}
int main(int argc, char const *argv[])
{
	if (argc != 3)
	{
		fprintf(2, "Usage: find dir file\n");
		exit(1);
	}

	char const *path = argv[1];
	char const *target = argv[2];
	find_helper(path, target);
	exit(0);
}
```

#### 2.6 xargs

---

简易版的xargs实现

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

int readline(char *new_argv[32], int curr_argc){
	char buf[1024];
	int n = 0;
	while(read(0, buf+n, 1)){
		if (n == 1023)
		{
			fprintf(2, "argument is too long\n");
			exit(1);
		}
		if (buf[n] == '\n')
		{
			break;
		}
		n++;
	}
	buf[n] = 0;
	if (n == 0)return 0;
	int offset = 0;
	while(offset < n){
		new_argv[curr_argc++] = buf + offset;
		while(buf[offset] != ' ' && offset < n){
			offset++;
		}
		while(buf[offset] == ' ' && offset < n){
			buf[offset++] = 0;
		}
	}
	return curr_argc;
}

int main(int argc, char const *argv[])
{
	if (argc <= 1)
	{
		fprintf(2, "Usage: xargs command (arg ...)\n");
		exit(1);
	}
	char *command = malloc(strlen(argv[1]) + 1);
	char *new_argv[MAXARG];
	strcpy(command, argv[1]);
	for (int i = 1; i < argc; ++i)
	{
		new_argv[i - 1] = malloc(strlen(argv[i]) + 1);
		strcpy(new_argv[i - 1], argv[i]);
	}

	int curr_argc;
	while((curr_argc = readline(new_argv, argc - 1)) != 0)
	{
		new_argv[curr_argc] = 0;
		if(fork() == 0){
			exec(command, new_argv);
			fprintf(2, "exec failed\n");
			exit(1);
		}
		wait(0);
	}
	exit(0);
}
```

