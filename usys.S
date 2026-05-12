// usys.S  —  user-space system call stubs (additions for CSE323 Project)
//
// Add these two entries to the existing xv6 usys.S (or usys.pl) file.
// They follow the identical SYSCALL macro pattern used by the rest of xv6.

#include "syscall.h"
#include "traps.h"

// Standard xv6 macro (already present in the original file)
#define SYSCALL(name)  \
  .globl name;         \
  name:                \
    movl $SYS_ ## name, %eax; \
    int $T_SYSCALL;    \
    ret

// ─── New stubs (append to existing usys.S) ───────────────────────────────────
SYSCALL(getchildren)
SYSCALL(setpriority)
