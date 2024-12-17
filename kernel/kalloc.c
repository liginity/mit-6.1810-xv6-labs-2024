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
  // NOTE when a page is not a cow page, the matching flags here is -1.
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
  kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)] = -1;
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

// for a physical page in the copy-on-write feature.
// 1. increase the reference count
// 2. record the page-table-entry flag
void
kcow_inc_rc(void *pa, int flags)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcow_inc_rc");

  // reference count should be at least 1.
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kcow_inc_rc: reference count error");
  }

  acquire(&kmem.lock);
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] == 1 &&
      kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)] == -1) {
    // record the flags
    // acquire(&kmem.lock);
    kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)] = flags;
    // release(&kmem.lock);
  } else {
    // check the flags
    // NOTE there is no clear rule for this check
    // check 1: verify the stored flags and current flags have the same
    //          PTE_W bit.
    if ((PTE_W & kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)]) !=
        (PTE_W & flags)) {
      // panic("kcow_inc_rc: flags changed");
      // printf("old flag = 0x%x\n", kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)]);
      // printf("new flag = 0x%x\n", flags);
      // NOTE one case of reaching here:
      //      when a page with PTE_W is marked as without PTE_W in cow,
      //      and the page is passed into kcow_inc_rc() for the second time.
    }
  }
  // acquire(&kmem.lock);
  kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] += 1;
  release(&kmem.lock);
}

void
kcow_dec_rc(void *pa)
{
  // NOTE kcow_dec_rc() is used when do the writing in copy-on-write
  //      in usertrap().
  // maybe the reference count should be at least 2.
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcow_dec_rc");

  // reference count should be at least 1.
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kcow_dec_rc: reference count error");
  }
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] == 1) {
    printf("rc = 1, and kcow_dec_rc() in called\n");
    panic("kcow_get_rc(): there is race condition");
  }
  acquire(&kmem.lock);
  kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] -= 1;
  release(&kmem.lock);
}

int
kcow_get_rc(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcow_get_rc");

  // reference count should be at least 1.
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kcow_get_rc: reference count error");
  }
  return kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)];
}

int
kcow_get_flags(void *pa)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcow_get_rc");

  // reference count should be at least 1.
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kcow_get_rc: reference count error");
  }
  return kmem.pg_flags[PA_TO_RC_ARRAY_INDEX((uint64)pa)];
}

// return a writable page from a cow page.
// THE LESSON: getting the value of reference count
//     and changing its value after doing some check,
//     the two things should be done in the same critical region.
void *
kcow_get_page(void *pa, int do_copy)
{
  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kcow_get_rc");

  void *new_pa = 0;
  acquire(&kmem.lock);
  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] <= 0) {
    panic("kcow_inc_rc: reference count error");
  }

  if (kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] == 1) {
    new_pa = pa;
    release(&kmem.lock);
  } else {
    kmem.phpgrcs[PA_TO_RC_ARRAY_INDEX((uint64)pa)] -= 1;
    release(&kmem.lock);
    new_pa = kalloc();
    if (new_pa == 0) {
      printf("failed to allocate new page for a cow page\n");
    }
  }

  if (do_copy && new_pa && new_pa != pa) {
    memmove(new_pa, pa, PGSIZE);
  }
  return new_pa;
}
