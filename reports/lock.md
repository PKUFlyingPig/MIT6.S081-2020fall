## Lab: Locking

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

#### 1. 概述

---

在并发编程中我们经常用到锁来解决同步互斥问题，但是一个多核机器上对锁的使用不当会带来很多的所谓 “lock contention” 问题。这个lab的目标就是对涉及到锁的数据结构进行修改已降低对锁的竞争。



#### 2. Memory allocator

---

第一部分涉及到内存分配的代码，xv6将空闲的物理内存kmem组织成一个空闲链表kmem.freelist，同时用一个锁kmem.lock保护freelist，所有对kmem.freelist的访问都需要先取得锁，所以会产生很多竞争。解决方案也很直观，给每个CPU单独开一个freelist和对应的lock，这样只有同一个CPU上的进程同时获取对应锁才会产生竞争。

```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];
```

同时得修改对应的kinit和kfree的代码以适应数据结构的修改

```c
void
kinit()
{
  char buf[10];
  for (int i = 0; i < NCPU; i++)
  {
    snprintf(buf, 10, "kmem_CPU%d", i);
    initlock(&kmem[i].lock, buf);
  }
  freerange(end, (void*)PHYSTOP);
}
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  int cpu = cpuid();
  pop_off();
  acquire(&kmem[cpu].lock);
  r->next = kmem[cpu].freelist;
  kmem[cpu].freelist = r;
  release(&kmem[cpu].lock);
}
```

另外，一个相对麻烦的问题是当一个CPU的freelist为空时，需要向其他 CPU的freelist“借”空闲块。

```c
void *
kalloc(void)
{
  struct run *r;

  push_off();
  int cpu = cpuid();
  pop_off();

  acquire(&kmem[cpu].lock);
  r = kmem[cpu].freelist;
  if(r)
    kmem[cpu].freelist = r->next;
  else // steal page from other CPU
  {
    struct run* tmp;
    for (int i = 0; i < NCPU; ++i)
    {
      if (i == cpu) continue;
      acquire(&kmem[i].lock);
      tmp = kmem[i].freelist;
      if (tmp == 0) {
        release(&kmem[i].lock);
        continue;
      } else {
        for (int j = 0; j < 1024; j++) {
          // steal 1024 pages
          if (tmp->next)
            tmp = tmp->next;
          else 
            break;
        }
        kmem[cpu].freelist = kmem[i].freelist;
        kmem[i].freelist = tmp->next;
        tmp->next = 0;
        release(&kmem[i].lock);
        break;
      }
    }
    r = kmem[cpu].freelist;
    if (r) 
      kmem[cpu].freelist = r->next;
  }
  release(&kmem[cpu].lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```



#### 3. Buffer cache

---

Buffer cache 是xv6的文件系统中的数据结构，用来缓存部分磁盘的数据块，以减少耗时的磁盘读写操作。但这也意味着buffer cache的数据结构是所有进程共享的（不同CPU上的也是如此），如果只用一个锁 bcache.lock保证对其修改的原子性的话，势必会造成很多的竞争。

我的解决策略是根据数据块的blocknumber将其保存进一个哈希表，而哈希表的每个bucket都有一个相应的锁来保护，这样竞争只会发生在两个进程同时访问同一个bucket内的block。

bcache的数据结构如下：

```c
struct {
  struct spinlock lock;
  struct buf head[NBUCKET];
  struct buf hash[NBUCKET][NBUF];
  struct spinlock hashlock[NBUCKET]; // lock per bucket
} bcache;
```

相应地，需要修改binit, bget和breles函数：

```c
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  for (int i = 0; i < NBUCKET; i++) {
    initlock(&bcache.hashlock[i], "bcache");

    // Create linked list of buffers
    bcache.head[i].prev = &bcache.head[i];
    bcache.head[i].next = &bcache.head[i];
    for(b = bcache.hash[i]; b < bcache.hash[i]+NBUF; b++){
      b->next = bcache.head[i].next;
      b->prev = &bcache.head[i];
      initsleeplock(&b->lock, "buffer");
      bcache.head[i].next->prev = b;
      bcache.head[i].next = b;
    }
  }
}
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint hashcode = blockno % NBUCKET;
  acquire(&bcache.hashlock[hashcode]);

  // Is the block already cached?
  for(b = bcache.head[hashcode].next; b != &bcache.head[hashcode]; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashlock[hashcode]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  for(b = bcache.head[hashcode].prev; b != &bcache.head[hashcode]; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.hashlock[hashcode]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  panic("bget: no buffers");
}

void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint hashcode = b->blockno % NBUCKET;
  acquire(&bcache.hashlock[hashcode]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.head[hashcode].next;
    b->prev = &bcache.head[hashcode];
    bcache.head[hashcode].next->prev = b;
    bcache.head[hashcode].next = b;
  }
  
  release(&bcache.hashlock[hashcode]);
}
```
