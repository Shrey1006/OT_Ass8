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

struct run {
  struct run *next;
};

#define PA2INDEX(pa) (((uint64)(pa) - KERNBASE) / PGSIZE)
#define MAX_PHYPAGES ((PHYSTOP - KERNBASE) / PGSIZE)

struct {
  struct spinlock lock;
  int count[MAX_PHYPAGES];
} refcnt;

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt");

  // Initialize reference counts to 1 so that the initial kfree
  // calls in freerange decrement them to 0 and push them to kmem.freelist.
  for (int i = 0; i < MAX_PHYPAGES; i++) {
    refcnt.count[i] = 1;
  }

  freerange(end, (void *)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&refcnt.lock);
  int idx = PA2INDEX(pa);
  if (refcnt.count[idx] <= 0)
    panic("kfree: refcount <= 0");
  refcnt.count[idx]--;
  int c = refcnt.count[idx];
  release(&refcnt.lock);

  // If other page tables still reference this physical page, do not free yet
  if (c > 0)
    return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

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
  if (r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if (r) {
    acquire(&refcnt.lock);
    refcnt.count[PA2INDEX(r)] = 1;
    release(&refcnt.lock);
    memset((char *)r, 5, PGSIZE); // fill with junk
  }
  return (void *)r;
}

// Increment reference count of a physical page (e.g. during fork/COW sharing)
void
kref_inc(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    return;
  acquire(&refcnt.lock);
  refcnt.count[PA2INDEX(pa)]++;
  release(&refcnt.lock);
}

// Decrement reference count of a physical page
void
kref_dec(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    return;
  acquire(&refcnt.lock);
  refcnt.count[PA2INDEX(pa)]--;
  release(&refcnt.lock);
}

// Query reference count of a physical page
int
kref_get(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    return 0;
  acquire(&refcnt.lock);
  int c = refcnt.count[PA2INDEX(pa)];
  release(&refcnt.lock);
  return c;
}

// Perform COW physical allocation or in-place upgrade.
// If refcount > 1: allocates a new page, copies pa to it, decrements refcount(pa), returns new page.
// If refcount == 1: retains pa in-place and returns pa.
// Returns 0 on allocation failure.
void *
kcow_page(void *pa)
{
  if ((uint64)pa < KERNBASE || (uint64)pa >= PHYSTOP)
    return 0;

  acquire(&refcnt.lock);
  int idx = PA2INDEX(pa);
  if (refcnt.count[idx] == 1) {
    // Single owner: no copy needed
    release(&refcnt.lock);
    return pa;
  }
  release(&refcnt.lock);

  // Multiple references: allocate private page
  char *mem = kalloc();
  if (mem == 0)
    return 0;
  memmove(mem, (char *)pa, PGSIZE);

  acquire(&refcnt.lock);
  if (refcnt.count[idx] == 1) {
    // Another core dropped refcount to 1 in the meantime
    release(&refcnt.lock);
    kfree(mem);
    return pa;
  }
  refcnt.count[idx]--;
  release(&refcnt.lock);
  return mem;
}
