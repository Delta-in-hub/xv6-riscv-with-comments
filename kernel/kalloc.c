// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct free_list
{
  struct spinlock lock;
  struct run *freelist;
} kmem, *per_hart_free_list;

void *per_hart_alloc_init();

void kinit()
{
  void *free_memory_start = per_hart_alloc_init();
  initlock(&kmem.lock, "kmem");
  freerange(free_memory_start, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

int pkfree(void *pa);

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct run *r;

  // physics address must be rounded to 4k, and  `end`<= pa <= PHYSTOP
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  if (pkfree(pa))
    return;
  r = (struct run *)pa;

  acquire(&kmem.lock);
  // insert to the front of the free-list
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

void *pkalloc();

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  void *ret = pkalloc();
  if (ret)
  {
    memset((char *)ret, 5, PGSIZE); // fill with junk
    return ret;
  }
  struct run *r;

  acquire(&kmem.lock);
  // get the front one of the free-list
  r = kmem.freelist;
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}

// Modified by Delta [START]
#define ROUND_UP(N, S) ((((N) + (S)-1) / (S)) * (S))
#define ROUND_DOWN(N, S) ((uint64)(N / S) * S)

#define PER_HART_ALLOC_SIZE (1 * 1024 * 1024) // per hart has 1MB space

#define assert(x)             \
  {                           \
    if (!(x))                 \
      panic("assert failed"); \
  }

/**
 * @brief per hart(cpu) alloc 1MB space for itself.
 *
 * @return the start address of remain memory
 */
void *per_hart_alloc_init()
{
  per_hart_free_list = (struct free_list *)end;
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&per_hart_free_list[i].lock, "kmem-per-hart-freelist");
    per_hart_free_list[i].freelist = (void *)0;
  }

  uint64 _s = (uint64)per_hart_free_list + NCPU * sizeof(*per_hart_free_list);
  _s = PGROUNDUP(_s);

  for (int i = 0; i < NCPU; i++)
  {
    for (int j = 0; j < PER_HART_ALLOC_SIZE / PGSIZE; j++)
    {
      struct run *r = (struct run *)_s;
      r->next = per_hart_free_list[i].freelist;
      per_hart_free_list[i].freelist = r;

      _s += PGSIZE;
    }
    assert(_s <= PHYSTOP);
  }
  return (void *)_s;
}

/**
 * @brief alloca 4KB space on hart's self-memroy. No need to acquire lock.
 *
 * @return NULL for failed
 */
void *pkalloc()
{
  int id = cpuid();
  // The requirement: the hartid must be continuous and started from 0.
  // This is Ok for qemu-risc
  struct free_list *free_list = &per_hart_free_list[id];
  struct run *ret;
  acquire(&free_list->lock);
  ret = free_list->freelist;
  if (ret)
    free_list->freelist = ret->next;
  release(&free_list->lock);
  return (void *)ret;
}

/**
 * @brief check if pa belongs to a hart's self-memory, if true give it back to that hart's free-list,else do nothing
 *
 * @return 1 for ture, 0 for else
 */
int pkfree(void *pa)
{
  uint64 _s = (uint64)per_hart_free_list + NCPU * sizeof(*per_hart_free_list);
  _s = PGROUNDUP(_s);

  uint64 _id = ((uint64)pa - _s) / PER_HART_ALLOC_SIZE;
  if (_id >= NCPU)
    return 0;
  struct free_list *fl = &per_hart_free_list[_id];
  struct run *r = (struct run *)pa;
  acquire(&fl->lock);
  r->next = fl->freelist;
  fl->freelist = r;
  release(&fl->lock);
  return 1;
}
// Modified by Delta [END]
