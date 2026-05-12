// proc.c  —  xv6 process management (modified for CSE323 Project)
//
// Changes made:
//   1. allocproc()       → initialise priority = 5 (mid-range default)
//   2. fork()            → child inherits parent priority; record ctime
//   3. scheduler()       → replaced round-robin with priority-based selection
//                          with aging to prevent starvation
//   4. sys_getchildren() → new system call (Foysal Hasan Shifat, 2222347042)
//   5. sys_setpriority() → new system call (Marzia Hossain,       2222465642)

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "syscall.h"

// ─── Priority constants ───────────────────────────────────────────────────────
#define DEFAULT_PRIORITY   5    // assigned to every new process
#define MIN_PRIORITY       0    // highest urgency
#define MAX_PRIORITY      10    // lowest urgency
#define AGING_INTERVAL   100    // ticks between aging passes
#define AGING_BOOST        1    // priority improvement per aging pass

// ─── Process table ────────────────────────────────────────────────────────────
struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;
static uint aging_clock = 0;    // tracks when to run the aging pass

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void *chan);

// ─────────────────────────────────────────────────────────────────────────────
// allocproc  – find an UNUSED slot and initialise it
// ─────────────────────────────────────────────────────────────────────────────
static struct proc *
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state   = EMBRYO;
  p->pid     = nextpid++;
  p->priority = DEFAULT_PRIORITY;   // ← project addition
  p->ctime    = ticks;              // ← project addition (creation time)

  release(&ptable.lock);

  // Allocate kernel stack
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// fork  – create a child process
// ─────────────────────────────────────────────────────────────────────────────
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  if((np = allocproc()) == 0)
    return -1;

  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state  = UNUSED;
    return -1;
  }
  np->sz     = curproc->sz;
  np->parent = curproc;
  *np->tf    = *curproc->tf;

  np->tf->eax = 0;           // fork returns 0 in child

  // ── project addition: child inherits parent priority ──────────────────────
  np->priority = curproc->priority;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}

// ─────────────────────────────────────────────────────────────────────────────
// scheduler  – priority-based CPU scheduler with aging
//
// Algorithm:
//   Each iteration scans the RUNNABLE set, picks the process with the
//   numerically-smallest priority value (= highest urgency).  Ties are
//   broken by arrival order (ctime).  After AGING_INTERVAL ticks, every
//   waiting RUNNABLE process has its priority decremented by AGING_BOOST
//   (clamped at MIN_PRIORITY) to prevent starvation.
// ─────────────────────────────────────────────────────────────────────────────
void
scheduler(void)
{
  struct proc *p, *best;
  struct cpu  *c = mycpu();
  c->proc = 0;

  for(;;){
    sti();  // enable interrupts on this processor

    acquire(&ptable.lock);

    // ── optional aging pass ──────────────────────────────────────────────────
    if(ticks - aging_clock >= AGING_INTERVAL){
      aging_clock = ticks;
      for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
        if(p->state == RUNNABLE && p->priority > MIN_PRIORITY){
          p->priority -= AGING_BOOST;
          if(p->priority < MIN_PRIORITY)
            p->priority = MIN_PRIORITY;
        }
      }
    }

    // ── find highest-priority RUNNABLE process ───────────────────────────────
    best = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;
      if(best == 0
         || p->priority < best->priority
         || (p->priority == best->priority && p->ctime < best->ctime))
      {
        best = p;
      }
    }

    if(best != 0){
      // ── context-switch into best ─────────────────────────────────────────
      c->proc   = best;
      switchuvm(best);
      best->state = RUNNING;
      swtch(&(c->scheduler), best->context);
      switchkvm();
      c->proc = 0;
    }

    release(&ptable.lock);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// sys_getchildren  –  system call implementation
//   (Foysal Hasan Shifat, 2222347042)
//
//   int getchildren(int pid, int *children, int maxlen)
//
//   Returns the number of direct children whose PIDs were written into the
//   user-space array *children (at most maxlen entries).
//   Returns -1 on error (bad pid, bad pointer).
// ─────────────────────────────────────────────────────────────────────────────
int
sys_getchildren(void)
{
  int pid, maxlen;
  int *children;            // user-space pointer

  // Fetch arguments
  if(argint(0, &pid)     < 0) return -1;
  if(argptr(1, (char**)&children, sizeof(int)) < 0) return -1;
  if(argint(2, &maxlen)  < 0) return -1;

  if(pid <= 0 || maxlen <= 0 || children == 0)
    return -1;

  struct proc *p;
  int count = 0;
  int tmp_children[NPROC];   // temporary buffer in kernel space

  acquire(&ptable.lock);

  // Verify the target PID actually exists
  int parent_found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->state != UNUSED){
      parent_found = 1;
      break;
    }
  }

  if(!parent_found){
    release(&ptable.lock);
    return -1;
  }

  // Collect child PIDs
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED) continue;
    if(p->parent && p->parent->pid == pid){
      if(count < NPROC)
        tmp_children[count++] = p->pid;
    }
  }

  release(&ptable.lock);

  // Copy up to maxlen entries to user space
  int to_copy = count < maxlen ? count : maxlen;
  if(copyout(myproc()->pgdir,
             (uint)children,
             (void*)tmp_children,
             to_copy * sizeof(int)) < 0)
    return -1;

  return count;   // total number of children (may exceed maxlen)
}

// ─────────────────────────────────────────────────────────────────────────────
// sys_setpriority  –  system call implementation
//   (Marzia Hossain, 2222465642)
//
//   int setpriority(int pid, int priority)
//
//   Sets the priority of process <pid> to <priority>.
//   Returns 0 on success, -1 on error.
// ─────────────────────────────────────────────────────────────────────────────
int
sys_setpriority(void)
{
  int pid, priority;
  struct proc *p;

  if(argint(0, &pid)      < 0) return -1;
  if(argint(1, &priority) < 0) return -1;

  if(pid <= 0)                                     return -1;
  if(priority < MIN_PRIORITY || priority > MAX_PRIORITY) return -1;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid && p->state != UNUSED){
      p->priority = priority;
      release(&ptable.lock);
      return 0;
    }
  }

  release(&ptable.lock);
  return -1;   // PID not found
}

// ─────────────────────────────────────────────────────────────────────────────
// Standard xv6 routines (unchanged — included for completeness)
// ─────────────────────────────────────────────────────────────────────────────

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

int
cpuid() {
  return mycpu()-cpus;
}

struct cpu*
mycpu(void)
{
  int apicid, i;
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  apicid = lapicid();
  for(i = 0; i < ncpu; ++i)
    if(cpus[i].apicid == apicid)
      return &cpus[i];
  panic("unknown apicid\n");
}

struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  wakeup1(curproc->parent);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

int
wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc) continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid     = 0;
        p->parent  = 0;
        p->name[0] = 0;
        p->killed  = 0;
        p->state   = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }
    sleep(curproc, &ptable.lock);
  }
}

void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  if(p == 0) panic("sleep");
  if(lk == 0) panic("sleep without lk");
  if(lk != &ptable.lock){
    acquire(&ptable.lock);
    release(lk);
  }
  p->chan  = chan;
  p->state = SLEEPING;
  sched();
  p->chan = 0;
  if(lk != &ptable.lock){
    release(&ptable.lock);
    acquire(lk);
  }
}

static void
wakeup1(void *chan)
{
  struct proc *p;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}

void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

int
kill(int pid)
{
  struct proc *p;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

void
procdump(void)
{
  static char *states[] = {
    [UNUSED]   "unused",
    [EMBRYO]   "embryo",
    [SLEEPING] "sleep ",
    [RUNNABLE] "runble",
    [RUNNING]  "run   ",
    [ZOMBIE]   "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED) continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s [prio=%d]", p->pid, state, p->name, p->priority);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}
