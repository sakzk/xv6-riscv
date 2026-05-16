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

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

// Static storage for lock names ("kmem0", "kmem1", ...).
static char locknames[NCPU][8];

void
printlockstats(void)
{
  for(int i = 0; i < NCPU; i++){
    if(kmem[i].lock.nacquire > 0 || kmem[i].lock.ncontend > 0){
      printf("lock: %s: #acquire %ld #contend %ld\n",
             kmem[i].lock.name, kmem[i].lock.nacquire, kmem[i].lock.ncontend);
      // Reset counters after printing.
      kmem[i].lock.nacquire = 0;
      kmem[i].lock.ncontend = 0;
    }
  }
}

void
kinit()
{
  for(int i = 0; i < NCPU; i++){
    locknames[i][0] = 'k';
    locknames[i][1] = 'm';
    locknames[i][2] = 'e';
    locknames[i][3] = 'm';
    locknames[i][4] = '0' + i;
    locknames[i][5] = '\0';
    initlock(&kmem[i].lock, locknames[i]);
  }
  // All free pages initially go to CPU 0's freelist.
  // Other CPUs will steal from it as needed.
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
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

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();  // disable interrupts so cpuid() is stable
  int id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  push_off();  // disable interrupts so cpuid() is stable
  int id = cpuid();

  // Try our own freelist first.
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  if(!r){
    // Our freelist is empty. Steal a page from another CPU.
    for(int i = 0; i < NCPU; i++){
      if(i == id)
        continue;
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r)
        kmem[i].freelist = r->next;
      release(&kmem[i].lock);
      if(r)
        break;
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
