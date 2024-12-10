// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define PA_TO_RC_ARRAY_INDEX(pa) ((PGROUNDDOWN(pa) - KERNBASE) / PGSIZE)
// #define PA_TO_RC_ARRAY_INDEX(pa) ((PGROUNDDOWN(pa) - KERNBASE) << 12)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
  // reference count array for physical pages.
  int phpgrcs[(PHYSTOP - KERNBASE) / PGSIZE];
  // page-table-entry flags for the physical pages.
  // using this "extra" array is a little wasteful. but it is easy to write code.
  int pg_flags[(PHYSTOP - KERNBASE) / PGSIZE];
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)p)] = 1;
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // for physical page reference count
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kfree: reference count error");
  }
  // use the mem lock
  acquire(&kmem.lock);
  kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] -= 1;
  int rc = kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)];
  release(&kmem.lock);

  if (rc > 0) {
    // this page has reference count > 0. it should not be freed.
    return;
  }
  // assert(kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] == 0);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    // initialize the reference count for this page.
    kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)r)] = 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Use copy-on-write to copy a physical page.
// 1. increase the reference count
// 2. record the page-table-entry flag
void
kcow_copy(void *pa, int flags)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcow_copy");

  // reference count should be at least 1.
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kcow_copy: reference count error");
  }

  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] == 1) {
    // record the flags
    kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)] = flags;
  } else {
    // check the flags
    // NOTE there is no clear rule for this check
    // check 1: verify the stored flags and current flags have the same
    //          PTE_W bit.
    if ((PTE_W & kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)]) !=
        (PTE_W & flags)) {
      panic("kcow_copy: flags changed");
    }

    kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] += 1;
  }
}
