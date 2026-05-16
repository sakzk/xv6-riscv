// Benchmark for kalloc lock contention.
// Forks multiple child processes that each perform
// many sbrk()/sbrk(-) cycles to stress kalloc/kfree,
// then prints lock contention stats.

#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

#define PGSIZE  4096
#define NCHILD  3       // Number of child processes (match CPUS)
#define NPAGES  500     // Pages to allocate per round
#define NROUND  100     // Number of alloc/free rounds per child

void
stressalloc(void)
{
  for(int i = 0; i < NROUND; i++){
    // Allocate NPAGES pages (each calls kalloc)
    char *p = sbrk(NPAGES * PGSIZE);
    if(p == (char*)-1){
      printf("sbrk alloc failed at round %d\n", i);
      exit(1);
    }

    // Free them (each calls kfree)
    if(sbrk(-(NPAGES * PGSIZE)) == (char*)-1){
      printf("sbrk free failed at round %d\n", i);
      exit(1);
    }
  }
}

int
main(int argc, char *argv[])
{
  printf("kalloctest: start\n");

  // Reset lock counters by calling lockstat once
  lockstat();

  int start = uptime();

  for(int i = 0; i < NCHILD; i++){
    int pid = fork();
    if(pid < 0){
      printf("fork failed\n");
      exit(1);
    }
    if(pid == 0){
      // Child: run the stress test
      stressalloc();
      exit(0);
    }
  }

  // Wait for all children to finish
  for(int i = 0; i < NCHILD; i++){
    wait(0);
  }

  int end = uptime();

  printf("kalloctest: %d ticks elapsed\n", end - start);
  lockstat();   // Print and reset lock stats
  printf("kalloctest: done\n");
  exit(0);
}
