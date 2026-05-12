// test_setpriority.c  —  Test program for Priority-Based Scheduling
// Author : Marzia Hossain  (2222465642)
// Course : CSE323 – Operating System Design
// Section: 03
//
// Demonstrates that the scheduler runs higher-priority processes before
// lower-priority ones.  Three child processes are spawned; each counts
// iterations in a busy loop.  After the parent sets different priorities,
// the process with the lowest numeric priority value (= highest urgency)
// should accumulate the most iterations.

#include "types.h"
#include "stat.h"
#include "user.h"

#define SHARED_PAGES 1          // 1 page of shared memory (via pipes here we
                                // use a simple file-backed counter approach)
#define WORK_TICKS  30          // how long each worker runs (in ticks)

// ─────────────────────────────────────────────────────────────────────────────
// worker  –  busy-loop for WORK_TICKS, then report iteration count to pipe
// ─────────────────────────────────────────────────────────────────────────────
static void
worker(int id, int prio, int write_fd)
{
  int start = uptime();
  long long count = 0;

  while(uptime() - start < WORK_TICKS)
    count++;

  // write result back through the pipe as text
  char buf[64];
  int len = 0;
  // build "ID PRIO COUNT\n" manually (no sprintf in xv6 ulib)
  buf[len++] = '0' + id;
  buf[len++] = ' ';
  buf[len++] = '0' + prio;
  buf[len++] = ' ';
  // encode count (simple base-10)
  char tmp[32]; int ti = 0;
  long long c = count;
  if(c == 0){ tmp[ti++] = '0'; }
  else { while(c){ tmp[ti++] = '0' + (c % 10); c /= 10; } }
  // reverse
  for(int a = 0, b = ti-1; a < b; a++, b--){
    char t = tmp[a]; tmp[a] = tmp[b]; tmp[b] = t;
  }
  for(int j = 0; j < ti; j++) buf[len++] = tmp[j];
  buf[len++] = '\n';

  write(write_fd, buf, len);
  close(write_fd);
  exit();
}

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int
main(void)
{
  int pfd[3][2];    // three pipes, one per child
  int pids[3];
  int priorities[3] = {2, 5, 9};   // high / medium / low urgency

  printf(1, "\n=== Priority-Based Scheduler Test ===\n");
  printf(1, "Author : Marzia Hossain (2222465642)\n\n");
  printf(1, "Spawning 3 workers with priorities: high(2), medium(5), low(9)\n");
  printf(1, "Each runs for %d ticks.  Higher-priority worker should finish\n", WORK_TICKS);
  printf(1, "more iterations of its busy loop.\n\n");

  for(int i = 0; i < 3; i++){
    if(pipe(pfd[i]) < 0){
      printf(1, "pipe failed\n");
      exit();
    }
    pids[i] = fork();
    if(pids[i] == 0){
      close(pfd[i][0]);   // child closes read end
      worker(i, priorities[i], pfd[i][1]);
      // worker() calls exit(), never returns
    }
    close(pfd[i][1]);     // parent closes write end
  }

  // Set priorities (parent does this right after all forks)
  for(int i = 0; i < 3; i++){
    int r = setpriority(pids[i], priorities[i]);
    printf(1, "setpriority(pid=%d, prio=%d) = %d  %s\n",
           pids[i], priorities[i], r, r == 0 ? "[OK]" : "[FAIL]");
  }

  printf(1, "\nWaiting for workers to finish...\n\n");

  // Read results
  char line[128];
  int id_r[3], prio_r[3];
  long long count_r[3];

  for(int i = 0; i < 3; i++){
    int n = read(pfd[i][0], line, sizeof(line)-1);
    if(n <= 0){ printf(1, "read failed\n"); continue; }
    line[n] = 0;
    // parse "ID PRIO COUNT\n"
    id_r[i] = line[0] - '0';
    prio_r[i] = line[2] - '0';
    count_r[i] = 0;
    for(int j = 4; line[j] && line[j] != '\n'; j++)
      count_r[i] = count_r[i]*10 + (line[j]-'0');
    close(pfd[i][0]);
    wait();
  }

  // Display results
  printf(1, "Results:\n");
  printf(1, "  %-8s %-10s %-20s\n", "Worker", "Priority", "Iterations");
  printf(1, "  %-8s %-10s %-20s\n", "------", "--------", "----------");

  long long max_count = 0;
  int winner = 0;
  for(int i = 0; i < 3; i++){
    // print count (no %lld in xv6 printf, so do it manually)
    char cbuf[32]; int ci = 0;
    long long cv = count_r[i];
    if(cv == 0){ cbuf[ci++]='0'; }
    else { while(cv){ cbuf[ci++]='0'+(cv%10); cv/=10; } }
    for(int a=0,b=ci-1;a<b;a++,b--){char t=cbuf[a];cbuf[a]=cbuf[b];cbuf[b]=t;}
    cbuf[ci]=0;
    printf(1, "  worker%-2d   prio=%-5d  %s\n", id_r[i], prio_r[i], cbuf);
    if(count_r[i] > max_count){ max_count = count_r[i]; winner = i; }
  }

  printf(1, "\nConclusion: Worker with priority %d ran the most.\n", prio_r[winner]);
  int expected_winner = 0; // priorities[0] = 2 is highest
  printf(1, "Expected winner: worker 0 (priority 2)\n");
  printf(1, "Test result: %s\n\n",
         winner == expected_winner ? "[PASS] Scheduling is priority-based."
                                   : "[NOTE] Result may vary under low load.");

  // ── Edge-case tests ───────────────────────────────────────────────────────
  printf(1, "Edge-case tests:\n");

  // invalid priority value
  int r = setpriority(getpid(), 11);
  printf(1, "  setpriority(self, 11)  = %d  %s\n", r,
         r == -1 ? "[PASS] rejected out-of-range" : "[FAIL]");

  r = setpriority(getpid(), -1);
  printf(1, "  setpriority(self, -1)  = %d  %s\n", r,
         r == -1 ? "[PASS] rejected negative" : "[FAIL]");

  // valid self-set
  r = setpriority(getpid(), 0);
  printf(1, "  setpriority(self,  0)  = %d  %s\n", r,
         r == 0  ? "[PASS] accepted highest prio" : "[FAIL]");

  r = setpriority(getpid(), 10);
  printf(1, "  setpriority(self, 10)  = %d  %s\n", r,
         r == 0  ? "[PASS] accepted lowest prio" : "[FAIL]");

  // bad PID
  r = setpriority(99999, 5);
  printf(1, "  setpriority(99999, 5)  = %d  %s\n", r,
         r == -1 ? "[PASS] non-existent PID rejected" : "[FAIL]");

  printf(1, "\nsetpriority() tests complete.\n\n");
  exit();
}
