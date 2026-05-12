// syscall.c  —  xv6 system call dispatch (modified for CSE323 Project)
//
// New entries:
//   [SYS_getchildren] sys_getchildren   (Foysal Hasan Shifat,  2222347042)
//   [SYS_setpriority] sys_setpriority   (Marzia Hossain,        2222465642)

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "syscall.h"

// ─── forward declarations for all system call implementations ────────────────
extern int sys_fork(void);
extern int sys_exit(void);
extern int sys_wait(void);
extern int sys_pipe(void);
extern int sys_read(void);
extern int sys_kill(void);
extern int sys_exec(void);
extern int sys_fstat(void);
extern int sys_chdir(void);
extern int sys_dup(void);
extern int sys_getpid(void);
extern int sys_sbrk(void);
extern int sys_sleep(void);
extern int sys_uptime(void);
extern int sys_open(void);
extern int sys_write(void);
extern int sys_mknod(void);
extern int sys_unlink(void);
extern int sys_link(void);
extern int sys_mkdir(void);
extern int sys_close(void);
extern int sys_getchildren(void);   // ← project addition
extern int sys_setpriority(void);   // ← project addition

// ─── syscall dispatch table ───────────────────────────────────────────────────
static int (*syscalls[])(void) = {
  [SYS_fork]        sys_fork,
  [SYS_exit]        sys_exit,
  [SYS_wait]        sys_wait,
  [SYS_pipe]        sys_pipe,
  [SYS_read]        sys_read,
  [SYS_kill]        sys_kill,
  [SYS_exec]        sys_exec,
  [SYS_fstat]       sys_fstat,
  [SYS_chdir]       sys_chdir,
  [SYS_dup]         sys_dup,
  [SYS_getpid]      sys_getpid,
  [SYS_sbrk]        sys_sbrk,
  [SYS_sleep]       sys_sleep,
  [SYS_uptime]      sys_uptime,
  [SYS_open]        sys_open,
  [SYS_write]       sys_write,
  [SYS_mknod]       sys_mknod,
  [SYS_unlink]      sys_unlink,
  [SYS_link]        sys_link,
  [SYS_mkdir]       sys_mkdir,
  [SYS_close]       sys_close,
  [SYS_getchildren] sys_getchildren,  // ← project addition
  [SYS_setpriority] sys_setpriority,  // ← project addition
};

// ─────────────────────────────────────────────────────────────────────────────
// Argument helpers (unchanged from stock xv6)
// ─────────────────────────────────────────────────────────────────────────────

int
argint(int n, int *ip)
{
  return fetchint((myproc()->tf->esp) + 4 + 4*n, ip);
}

int
argptr(int n, char **pp, int size)
{
  int i;
  struct proc *curproc = myproc();
  if(argint(n, &i) < 0) return -1;
  if(size < 0 || (uint)i >= curproc->sz || (uint)i+size > curproc->sz)
    return -1;
  *pp = (char*)i;
  return 0;
}

int
argstr(int n, char **pp)
{
  int addr;
  if(argint(n, &addr) < 0) return -1;
  return fetchstr(addr, pp);
}

// ─────────────────────────────────────────────────────────────────────────────
// syscall  –  dispatch on %eax
// ─────────────────────────────────────────────────────────────────────────────
void
syscall(void)
{
  int num;
  struct proc *curproc = myproc();

  num = curproc->tf->eax;
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]){
    curproc->tf->eax = syscalls[num]();
  } else {
    cprintf("%d %s: unknown sys call %d\n",
            curproc->pid, curproc->name, num);
    curproc->tf->eax = -1;
  }
}
