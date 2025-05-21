/* Recursively forks until the child fails to fork.
   We expect that at least 28 copies can run.
   
   We count how many children your kernel was able to execute
   before it fails to start a new process.  We require that,
   if a process doesn't actually get to start, exec() must
   return -1, not a valid PID.

   We repeat this process 10 times, checking that your kernel
   allows for the same level of depth every time.

   In addition, some processes will spawn children that terminate
   abnormally after allocating some resources.

   We set EXPECTED_DEPTH_TO_PASS heuristically by
   giving *large* margin on the value from our implementation.
   If you seriously think there is no memory leak in your code
   but it fails with EXPECTED_DEPTH_TO_PASS,
   please manipulate it and report us the actual output.
   
   Orignally written by Godmar Back <godmar@gmail.com>
   Modified by Minkyu Jung, Jinyoung Oh <cs330_ta@casys.kaist.ac.kr>
*/

#include <debug.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <syscall.h>
#include <random.h>
#include "tests/lib.h"

static const int EXPECTED_DEPTH_TO_PASS = 10;
static const int EXPECTED_REPETITIONS = 10;

const char *test_name = "multi-oom";

int make_children (void);

/* Open a number of files (and fail to close them).
   The kernel must free any kernel resources associated
   with these file descriptors. */
static void
consume_some_resources (void)
{
  msg("consume_some_resources에 들어옴");
  int fd, fdmax = 126;

  /* Open as many files as we can, up to fdmax.
	 Depending on how file descriptors are allocated inside
	 the kernel, open() may fail if the kernel is low on memory.
	 A low-memory condition in open() should not lead to the
	 termination of the process.  */
  for (fd = 0; fd < fdmax; fd++) {
#ifdef EXTRA2
	  if (fd != 0 && (random_ulong () & 1)) {
		if (dup2(random_ulong () % fd, fd+fdmax) == -1)
			break;
		else
			if (open (test_name) == -1)
			  break;
	  }
#else
		if (open (test_name) == -1)
		  break;
#endif
  }
}

/* Consume some resources, then terminate this process
   in some abnormal way.  */
static int NO_INLINE
consume_some_resources_and_die (void)
{
  msg("consume_some_resouces_and_die에 들어옴");
  consume_some_resources ();
  int *KERN_BASE = (int *)0x8004000000;
  msg("KERN_BASE는 %d임", *KERN_BASE);

  switch (random_ulong () % 5) {
	case 0:
    msg("0에 들어옴");
	  *(int *) NULL = 42;
    break;

	case 1:
      msg("1에 들어옴");
	  return *(int *) NULL;

	case 2:
      msg("2에 들어옴");
	  return *KERN_BASE;

	case 3:
      msg("3에 들어옴");
	  *KERN_BASE = 42;
    break;

	case 4:
      msg("4에 들어옴");
	  open ((char *)KERN_BASE);
	  exit (-1);
    break;

	default:
	  NOT_REACHED ();
  }
  return 0;
}

int
make_children (void) {
  msg("make_children에 들어옴");
  int i = 0;
  int pid;
  char child_name[128];
  for (; ; random_init (i), i++) {
    msg("make_children에서의 현재 i : %i", i);
    if (i > EXPECTED_DEPTH_TO_PASS/2) {
      msg("%d는 비정상 종료", i);
      // 비정상 종료
      snprintf (child_name, sizeof child_name, "%s_%d_%s", "child", i, "X");
      msg("child_name은 %s", child_name);
      pid = fork(child_name);
      msg("fork함 pid는 %d", pid);
      if (pid > 0 && wait (pid) != -1) {
        msg("pid > 0 && wait (pid) != -1임");
        fail ("crashed child should return -1.");
      } else if (pid == 0) {
        msg("else if에 들어옴");
        consume_some_resources_and_die();
        fail ("Unreachable");
      }
    }

    // 정상 종료
    msg("%d는 정상 종료", i);
    snprintf (child_name, sizeof child_name, "%s_%d_%s", "child", i, "O");
    pid = fork(child_name);
    if (pid < 0) {
      exit (i);
    } else if (pid == 0) {
      consume_some_resources();
    } else {
      break;
    }
  }

  int depth = wait (pid);
  msg("depth는 %d", depth);
  if (depth < 0) {
    msg("depth < 0임");
	  fail ("Should return > 0.");
  }
  
  if (i == 0)
	  return depth;
  else
	  exit (depth);
}

int
main (int argc UNUSED, char *argv[] UNUSED) {
  msg ("begin");

  int first_run_depth = make_children ();
  msg("처음 만든 자식 개수: %d", first_run_depth);
  CHECK (first_run_depth >= EXPECTED_DEPTH_TO_PASS, "Spawned at least %d children.", EXPECTED_DEPTH_TO_PASS);

  for (int i = 0; i < EXPECTED_REPETITIONS; i++) {
    msg("main의 i : %d", i);
    int current_run_depth = make_children();
    msg("새로 만든 자식 개수: %d", current_run_depth);
    if (current_run_depth < first_run_depth) {
      msg("main에서 실패햇어욤");
      fail ("should have forked at least %d times, but %d times forked", 
              first_run_depth, current_run_depth);
    }
  }

  msg ("success. Program forked %d iterations.", EXPECTED_REPETITIONS);
  msg ("end");
}
