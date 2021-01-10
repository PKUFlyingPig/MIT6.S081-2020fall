## Lab: Multithreading

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概览：

---

这个lab主要由三部分组成：

- 实现一个用户级线程的创建和切换
- 使用UNIX pthread线程库实现一个线程安全的Hash表
- 利用UNIX的锁和条件变量实现一个barrier

### 2. 实现思路与代码

----

#### 2.1  Uthread: switching between threads

----

这个lab希望我们补充完成一个用户级线程的创建和切换上下文的代码。

每个线程被定义成一个结构体：

```c
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct uthread_context context;      /* thread context */
};
```

创建线程时从当前线程池中找到状态为FREE的线程，然后初始化它的返回值为参数中的函数指针。初始化sp的值时需要注意，由于栈是向下生长的，因此sp的值应当赋成thread的stack数组的末尾元素。

```c
void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;
  // YOUR CODE HERE
  t->context.ra = (uint64)func;
  t->context.sp = (uint64)(t->stack + STACK_SIZE);
}
```

上下文切换部分完全仿照内核进程切换的switch.S 的代码，保存被调用者保存寄存器，ra（返回值）， sp（栈指针）

```asm
thread_switch:
	sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s2, 32(a0)
    sd s3, 40(a0)
    sd s4, 48(a0)
    sd s5, 56(a0)
    sd s6, 64(a0)
    sd s7, 72(a0)
    sd s8, 80(a0)
    sd s9, 88(a0)
    sd s10, 96(a0)
    sd s11, 104(a0)

    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s2, 32(a1)
    ld s3, 40(a1)
    ld s4, 48(a1)
    ld s5, 56(a1)
    ld s6, 64(a1)
    ld s7, 72(a1)
    ld s8, 80(a1)
    ld s9, 88(a1)
    ld s10, 96(a1)
    ld s11, 104(a1)
	ret    
```



#### 2.2  Using threads :

----

这个lab需要我们通过加锁来实现一个线程安全的哈希表。

下面是这个哈希表数据结构的组成：

```c
#define NBUCKET 5
#define NKEYS 100000

struct entry {
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];
int keys[NKEYS];
```

可以看到哈希表共有5个bucket，每个bucket都是一个由entry组成的链表，而哈希表的插入操作也很直观：

```c
static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;
}
```

但上面这段简单的代码也是多线程操作哈希表时出错的根源，假设某个线程调用了insert但没有返回，此时另一个线程调用insert，它们的第四个参数n（bucket的链表头）如果值相同，就会发生漏插入键值对的现象。

为了避免上面的错误，只需要在input调用insert的部分加锁即可。

```c
static 
void put(int key, int value)
{
  int i = key % NBUCKET;

  // is the key already present?
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;
  } else {
    // the key is new.
    pthread_mutex_lock(&locks[i]);
    insert(key, value, &table[i], table[i]);
    pthread_mutex_unlock(&lock[i]);
  }
}
```



#### 2.3 Barrier :

----

这个lab需要实现一个barrier，即每个线程都要在barrier处等待所有线程到达barrier之后才能继续运行。

为了实现barrier，需要用到UNIX提供的条件变量以及wait/broadcast机制。

```c
static void 
barrier()
{
  // YOUR CODE HERE
  //
  // Block until all threads have called barrier() and
  // then increment bstate.round.
  //
  pthread_mutex_lock(&bstate.barrier_mutex);
  bstate.nthread++;
  if (bstate.nthread < nthread) {
    pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex);
  } else {
    bstate.round++;
    bstate.nthread = 0;
    pthread_cond_broadcast(&bstate.barrier_cond);
  }
  pthread_mutex_unlock(&bstate.barrier_mutex);
}
```
