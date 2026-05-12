// test_getchildren.c  —  Test program for the getchildren() system call
// Author : Foysal Hasan Shifat  (2222347042)
// Course : CSE323 – Operating System Design
// Section: 03

#include "types.h"
#include "stat.h"
#include "user.h"

#define MAX_CHILDREN 64

// Helper: print a horizontal separator
static void
separator(void)
{
  printf(1, "------------------------------------------------------------\n");
}

int
main(void)
{
  int i, n, pid, ret;
  int children[MAX_CHILDREN];

  printf(1, "\n=== getchildren() System Call Test ===\n");
  printf(1, "Author : Foysal Hasan Shifat (2222347042)\n\n");

  // ── Test 1: single child ────────────────────────────────────────────────
  separator();
  printf(1, "Test 1: spawn 1 child, read children of parent\n");
  separator();

  pid = fork();
  if(pid == 0){
    // child just sleeps a bit then exits
    sleep(5);
    exit();
  }

  // parent: give child time to appear in RUNNABLE state
  sleep(2);

  ret = getchildren(getpid(), children, MAX_CHILDREN);
  printf(1, "getchildren(%d) returned count=%d\n", getpid(), ret);
  if(ret < 0){
    printf(1, "  ERROR: returned -1\n");
  } else {
    for(i = 0; i < ret && i < MAX_CHILDREN; i++)
      printf(1, "  child[%d] = PID %d\n", i, children[i]);
  }
  wait();

  // ── Test 2: multiple children ───────────────────────────────────────────
  separator();
  printf(1, "Test 2: spawn 4 children, list them all\n");
  separator();

  int spawned[4];
  for(n = 0; n < 4; n++){
    spawned[n] = fork();
    if(spawned[n] == 0){
      sleep(10);
      exit();
    }
  }

  sleep(2);   // let children become RUNNABLE

  ret = getchildren(getpid(), children, MAX_CHILDREN);
  printf(1, "getchildren(%d) returned count=%d\n", getpid(), ret);
  for(i = 0; i < ret && i < MAX_CHILDREN; i++)
    printf(1, "  child[%d] = PID %d\n", i, children[i]);

  for(n = 0; n < 4; n++) wait();

  // ── Test 3: invalid PID ──────────────────────────────────────────────────
  separator();
  printf(1, "Test 3: call with invalid PID (should return -1)\n");
  separator();

  ret = getchildren(-1, children, MAX_CHILDREN);
  printf(1, "getchildren(-1) = %d  %s\n", ret,
         ret == -1 ? "[PASS]" : "[FAIL]");

  // ── Test 4: maxlen overflow ──────────────────────────────────────────────
  separator();
  printf(1, "Test 4: maxlen=1 with 3 children (should return 3, copy 1)\n");
  separator();

  int p1 = fork(); if(p1 == 0){ sleep(8); exit(); }
  int p2 = fork(); if(p2 == 0){ sleep(8); exit(); }
  int p3 = fork(); if(p3 == 0){ sleep(8); exit(); }

  sleep(2);
  memset(children, 0, sizeof(children));
  ret = getchildren(getpid(), children, 1);
  printf(1, "getchildren(pid, buf, 1) returned total=%d, copied PID=%d  %s\n",
         ret, children[0],
         (ret == 3 && children[0] != 0) ? "[PASS]" : "[FAIL]");

  wait(); wait(); wait();

  separator();
  printf(1, "getchildren() tests complete.\n\n");
  exit();
}
