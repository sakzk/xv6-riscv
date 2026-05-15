// Mutual exclusion lock.
struct spinlock {
  uint locked;       // Is the lock held?

  // For debugging:
  char *name;        // Name of lock.
  struct cpu *cpu;   // The cpu holding the lock.

  // For contention measurement:
  uint64 nacquire;   // Total number of acquire calls.
  uint64 ncontend;   // Number of times acquire had to spin.
};

