# xv6 OS Enhancement – Kernel Explorers

**Course:** CSE323 – Operating System Design  
**Section:** 03  
**Instructor:** Farhan Tanvir Khan  
**Team:** Kernel Explorers

| Member | ID | Feature |
|--------|----|---------|
| Foysal Hasan Shifat | 2222347042 | `getchildren` system call |
| Marzia Hossain | 2222465642 | Priority-Based Process Scheduling |

---

## Overview

This project extends the [MIT xv6](https://github.com/mit-pdos/xv6-public) teaching operating system with two new features:

1. **`getchildren` system call** – returns the PIDs of all direct child processes of a given process.
2. **Priority-Based Process Scheduler** – replaces xv6's default round-robin scheduler with a priority-aware scheduler that supports aging to prevent starvation.

---

## Repository Structure

```
xv6-kernel-explorers/
├── kernel/
│   ├── proc.c          # Core: scheduler + both syscall implementations
│   ├── syscall.c       # Dispatch table (new entries for SYS_getchildren & SYS_setpriority)
│   └── usys.S          # User-space syscall stubs
├── include/
│   ├── proc.h          # struct proc (with priority & ctime fields)
│   ├── syscall.h       # Syscall numbers (SYS_getchildren=22, SYS_setpriority=23)
│   └── user.h          # User-space prototypes
├── user/
│   ├── test_getchildren.c   # Test suite for getchildren()
│   └── test_setpriority.c   # Test suite for setpriority() + scheduler demo
├── Makefile.patch      # How to integrate into a full xv6 tree
└── README.md           # This file
```

---

## Feature 1 – `getchildren` System Call

**Implemented by:** Foysal Hasan Shifat (2222347042)

### API

```c
int getchildren(int pid, int *children, int maxlen);
```

| Parameter | Description |
|-----------|-------------|
| `pid` | PID of the parent process to inspect |
| `children` | User-space array to receive child PIDs |
| `maxlen` | Maximum number of PIDs to write into `children` |
| **Returns** | Total number of direct children found, or `-1` on error |

### How It Works

1. Validates `pid` and `maxlen`; returns `-1` immediately for invalid inputs.
2. Scans the kernel `ptable` looking for processes whose `parent->pid == pid`.
3. Collects matching PIDs into a temporary kernel-space buffer.
4. Uses `copyout()` to transfer up to `maxlen` PIDs safely from kernel space to the user-supplied pointer.
5. Returns the **total** child count (which may exceed `maxlen` — the caller can detect overflow).

### Error Conditions

| Condition | Return value |
|-----------|-------------|
| `pid <= 0` | `-1` |
| `pid` refers to a non-existent process | `-1` |
| `maxlen <= 0` | `-1` |
| `children` pointer fails bounds check | `-1` |

### Files Modified / Added

| File | Change |
|------|--------|
| `include/syscall.h` | Added `#define SYS_getchildren 22` |
| `kernel/proc.c` | Added `sys_getchildren()` implementation |
| `kernel/syscall.c` | Added dispatch entry `[SYS_getchildren] sys_getchildren` |
| `kernel/usys.S` | Added `SYSCALL(getchildren)` stub |
| `include/user.h` | Added prototype `int getchildren(int, int*, int)` |
| `user/test_getchildren.c` | New user-level test program |

---

## Feature 2 – Priority-Based Process Scheduling

**Implemented by:** Marzia Hossain (2222465642)

### Priority Model

- Every process has an integer `priority` field in `struct proc`.
- Range: `0` (highest urgency) to `10` (lowest urgency).
- Default value at process creation: `5` (mid-range).
- Children **inherit** their parent's priority at `fork()` time.

### Scheduler Algorithm

The modified `scheduler()` in `proc.c`:

1. On each scheduling opportunity, scans all `RUNNABLE` processes.
2. Selects the process with the **smallest** `priority` value.
3. **Tie-breaking:** if two processes share the same priority, the one with the earlier creation time (`ctime`) runs first (FCFS within the same priority level).
4. Context-switches into the selected process.

### Starvation Prevention (Aging)

Every `AGING_INTERVAL` ticks (default: 100), the scheduler decrements the `priority` of every `RUNNABLE` process that is waiting by `AGING_BOOST` (default: 1), clamped at `MIN_PRIORITY (0)`.  
This ensures no process waits indefinitely.

### `setpriority` System Call

```c
int setpriority(int pid, int priority);
```

| Parameter | Description |
|-----------|-------------|
| `pid` | Target process PID |
| `priority` | New priority value (must be in `[0, 10]`) |
| **Returns** | `0` on success, `-1` on error |

### Error Conditions

| Condition | Return value |
|-----------|-------------|
| `pid <= 0` | `-1` |
| `priority < 0` or `priority > 10` | `-1` |
| PID not found in process table | `-1` |

### Files Modified / Added

| File | Change |
|------|--------|
| `include/proc.h` | Added `priority` and `ctime` fields to `struct proc` |
| `include/syscall.h` | Added `#define SYS_setpriority 23` |
| `kernel/proc.c` | Modified `allocproc()`, `fork()`, `scheduler()`; added `sys_setpriority()` |
| `kernel/syscall.c` | Added dispatch entry `[SYS_setpriority] sys_setpriority` |
| `kernel/usys.S` | Added `SYSCALL(setpriority)` stub |
| `include/user.h` | Added prototype `int setpriority(int, int)` |
| `user/test_setpriority.c` | New user-level test program |

---

## Integration Guide (applying to a full xv6 tree)

### Step 1 – Copy source files

```bash
# Assume stock xv6 is in ./xv6-public/
cp kernel/proc.c      xv6-public/proc.c
cp kernel/syscall.c   xv6-public/syscall.c
cp kernel/usys.S      xv6-public/usys.S       # append only the two new SYSCALL() lines
cp include/proc.h     xv6-public/proc.h
cp include/syscall.h  xv6-public/syscall.h
cp include/user.h     xv6-public/user.h
cp user/test_getchildren.c  xv6-public/test_getchildren.c
cp user/test_setpriority.c  xv6-public/test_setpriority.c
```

### Step 2 – Edit the Makefile

Open `xv6-public/Makefile` and make two additions:

**a) Add user programs to `UPROGS`:**
```makefile
UPROGS=\
    ...existing entries...\
    _test_getchildren\
    _test_setpriority\
```

**b) Add source files to `EXTRA`:**
```makefile
EXTRA=\
    ...existing entries...\
    test_getchildren.c\
    test_setpriority.c\
```

### Step 3 – Build and run

```bash
cd xv6-public
make qemu-nox
```

### Step 4 – Run tests inside xv6

Once xv6 boots, at the shell prompt:

```
$ test_getchildren
$ test_setpriority
```

---

## Expected Test Output

### `test_getchildren`

```
=== getchildren() System Call Test ===
Author : Foysal Hasan Shifat (2222347042)

------------------------------------------------------------
Test 1: spawn 1 child, read children of parent
------------------------------------------------------------
getchildren(3) returned count=1
  child[0] = PID 4
...
getchildren(-1) = -1  [PASS]
...
getchildren(pid, buf, 1) returned total=3, copied PID=7  [PASS]
getchildren() tests complete.
```

### `test_setpriority`

```
=== Priority-Based Scheduler Test ===
Author : Marzia Hossain (2222465642)

Spawning 3 workers with priorities: high(2), medium(5), low(9)
...
setpriority(pid=4, prio=2) = 0  [OK]
setpriority(pid=5, prio=5) = 0  [OK]
setpriority(pid=6, prio=9) = 0  [OK]

Results:
  Worker   Priority   Iterations
  ------   --------   ----------
  worker0  prio=2     18234567
  worker1  prio=5     9123456
  worker2  prio=9     3045678

Conclusion: Worker with priority 2 ran the most.
Test result: [PASS] Scheduling is priority-based.

  setpriority(self, 11)  = -1  [PASS] rejected out-of-range
  setpriority(self, -1)  = -1  [PASS] rejected negative
  setpriority(self,  0)  = 0   [PASS] accepted highest prio
  setpriority(self, 10)  = 0   [PASS] accepted lowest prio
  setpriority(99999, 5)  = -1  [PASS] non-existent PID rejected
```

---

## Design Decisions

### Why `copyout()` instead of direct pointer dereference?
User pointers cannot be dereferenced directly from kernel code on x86 without explicit validation and mapping. `copyout()` safely copies data from a kernel buffer to a user virtual address, checking bounds against the process's page table.

### Why aging?
A pure priority scheduler can starve low-priority processes if higher-priority processes continuously arrive. Aging periodically boosts the effective priority of waiting processes, guaranteeing eventual execution for every process.

### Why inherit priority on `fork()`?
Parent and child are logically related tasks. Inheriting priority preserves the scheduling intent set by the parent (or system administrator) and avoids unexpected de-prioritisation of newly spawned helpers.

### Why `ctime` as a tiebreaker?
When two processes share the same priority, FCFS (First-Come-First-Served) is the fairest and most predictable tiebreaker, avoiding arbitrary ordering that could cause short-term starvation within the same priority band.

---

## Known Limitations

- The aging pass runs inside `scheduler()` while holding `ptable.lock`, which adds a small overhead every `AGING_INTERVAL` ticks. This is acceptable for a teaching OS but would require a separate kernel thread in a production system.
- `getchildren()` returns a snapshot; by the time the caller reads the array, some child PIDs may have already exited.
- Priority is not enforced across CPUs in the SMP build — each CPU runs its own scheduler loop independently. Full SMP-aware scheduling would require a shared run queue with additional locking.

---

## References

- Cox, R., Kaashoek, F., Morris, R. *xv6: a simple, Unix-like teaching operating system*. MIT PDOS. https://pdos.csail.mit.edu/6.828/2018/xv6/book-rev11.pdf
- Silberschatz, A., Galvin, P., Gagne, G. *Operating System Concepts*, 10th ed. Wiley, 2018. (Chapters 5 & 6 — CPU Scheduling)
- xv6 source code: https://github.com/mit-pdos/xv6-public
