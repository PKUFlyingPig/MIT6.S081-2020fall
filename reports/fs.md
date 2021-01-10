## Lab : file system

@author : FlyingPig

@email : zhongyinmin@pku.edu.cn

### 1. 概述

---

这个lab中会给xv6的文件系统添加大文件和符号链接。

### 2. 代码实现

---

#### 2.1 Large files

---

<img src="/Users/apple/Library/Application Support/typora-user-images/image-20210109231702097.png" alt="image-20210109231702097" style="zoom:50%;" />

原先xv6的inode包含12个直接数据块号和1个间接数据块号，其中间接数据块包含256个块号（BSIZE/4），因此一个xv6的文件最多只能含有268个数据块的数据。

这个lab的目的就是将一个直接数据块号替换成一个两层间接数据块号，即指向一个包含间接数据块号的数据块。

主要的代码就是修改bmap函数，它会将指定inode的第bn个数据块（相对位置）映射到磁盘的数据块号。

```c
// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  if(bn < NDIRECT){
    if((addr = ip->addrs[bn]) == 0)
      ip->addrs[bn] = addr = balloc(ip->dev);
    return addr;
  }
  bn -= NDIRECT;

  if(bn < NINDIRECT){
    // Load indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT]) == 0)
      ip->addrs[NDIRECT] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn]) == 0){
      a[bn] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  bn -= NINDIRECT;

  uint indirect_idx, final_offset;
  struct buf *bp2;
  if (bn < NDOUBLEINDIRECT) {
    // Load double-indirect block, allocating if necessary
    if((addr = ip->addrs[NDIRECT+1]) == 0)
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    indirect_idx = bn / NINDIRECT; 
    final_offset = bn % NINDIRECT;
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    // Load indirect block, allocating if necessary
    if ((addr = a[indirect_idx]) == 0) {
      a[indirect_idx] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);

    bp2 = bread(ip->dev, addr);
    a = (uint*)bp2->data;
    if ((addr = a[final_offset]) == 0) {
      a[final_offset] = addr = balloc(ip->dev);
      log_write(bp2);
    }
    brelse(bp2);
    return addr;
  }

  panic("bmap: out of range");
}
```

#### 2.2 Symbolic links

---

这个lab的目标是给xv6新增一个符号链接的系统调用。常见的硬链接（例如A链接到B），会将A的inode号设置成和B文件一样，并且将对应inode的ref加一。而所谓符号链接（软链接），并不会影响B的inode，而是将A标记成特殊的软链接文件，之后对A进行的所有文件操作，操作系统都会转换到对B的操作，类似于一个快捷方式。

首先新增syslink这个系统调用：

```c
uint64
sys_symlink(void)
{
  char target[MAXPATH];
  char path[MAXPATH];
  struct inode *ip;
  int n;

  if((n = argstr(0, target, MAXPATH)) < 0 || argstr(1, path, MAXPATH) < 0)
    return -1;

  begin_op();

  // create a new symlink, return with a locked inode
  ip = create(path, T_SYMLINK, 0, 0);
  if(ip == 0){
      end_op();
      return -1;
  }

  // write the target into the symlink's data block
  if(writei(ip, 0, (uint64)target, 0, n) != n) {
    end_op();
    return -1;
  }

  iunlockput(ip);
  end_op();
  return 0;
}
```

这个系统调用会创建一个类型为T_SYMLINK的的文件，并把target的文件路径写到它的datablock中。

另外需要修改open这个系统调用，使得操作系统能够正确处理symlink类型的文件：

```c
if (ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)) {
    int tolerate = 10;
    while (ip->type == T_SYMLINK && tolerate > 0) {
      if(readi(ip, 0, (uint64)path, 0, ip->size) != ip->size) {
        iunlockput(ip);
        end_op();
        return -1;
      }
      iunlockput(ip);
      if((ip = namei(path)) == 0) {
        end_op();
        return -1;
      }
      ilock(ip);
      tolerate--;
    }
    // cycle symlink is not allowed
    if (tolerate == 0) {
      iunlockput(ip);
      end_op();
      return -1;
    }
  }
```

注意如果软链接文件链接到了另一个软链接文件，系统需要递归地查找直到找到第一个非软链接的文件（对于成环的情况，需要返回-1）。

另外如果用户在打开文件时添加了O_NOFOLLOW模式，那么系统需要像处理正常文件一样将软链接文件打开。
