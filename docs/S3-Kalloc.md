# S3 - Kernel Memory Allocation

> http://xv6.dgs.zone/tranlate_books/book-riscv-rev1/c3/s4.html
>
> https://jyywiki.cn/OS/2022/slides/14.slides#/5/1
>
> https://www.geeksforgeeks.org/operating-system-allocating-kernel-memory-buddy-system-slab-system/
>
> https://jyywiki.cn/OS/2022/labs/L1
>
> [pmm.c](../../os-workbench-2022/kernel/src/pmm.c)



## XV6的物理内存分配

内核必须在运行时为页表、用户内存、内核栈和管道缓冲区分配和释放物理内存。

xv6使用内核末尾到`PHYSTOP`之间的物理内存进行运行时分配。它一次分配和释放整个4096字节的页面。

它使用链表的数据结构将空闲页面记录下来。分配时需要从链表中删除页面；释放时需要将释放的页面添加到链表中。

XV6共拥有128MB的内存，内存地址从`0x80000000`开始，到`0x88000000`结束。

- `0x80000000`-`0x80021d60` ，内核程序本身占用这段内存
- `0x80021d60`-`0x88000000`，Free memory 由内核管理（kalloc,kfree），以按需分配
  - `0x80021d60` - `0x80022000`，没有使用，原因如下
  - `0x80022000`  - `0x88000000`，真正被管理的内存空间，32734个4KB的页面。



XV6分配，释放内存都以4KB为单位，`kalloc(void)`是不要参数的。因此内存都要对4K对齐。代码如下。

```c
#define PGSIZE 4096 // bytes per page

#define PGROUNDUP(sz)  (((sz)+PGSIZE-1) & ~(PGSIZE-1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE-1))
```

使用`uint16_t`举例说明

```C
PGSIZE      0b0001000000000000
PGSIZE-1    0b0000111111111111
~(PGSIZE-1) 0b1111000000000000
------------------------------
a           0b0010101001010101
a+PGSIZE-1  0b0011101001010100
RU(a)       0b0011000000000000
RD(a)       0b0010000000000000
```





