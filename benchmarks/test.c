#include "../thread-worker.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <sys/ucontext.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

/* A scratch program template on which to call and
 * test thread-worker library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

void *fun(void *arg) {
  char *name = (char *)arg;
  printf("Hello (%s)\n", name);
  return NULL;
}

/* Basically think of the scheduler as another thread / process that is being
 * context switched into in order to schedule and context switch into the
 * different threads we need two contexts, main and scheduler */

extern ucontext_t sched_context;

int main(int argc, char **argv) {

  /* Implement HERE */
  worker_t w1, w2;
  worker_create(&w1, NULL, fun, "Mihir");
  worker_create(&w2, NULL, fun, "Kulkarni");

  ucontext_t maincontext;
  getcontext(&maincontext);
  swapcontext(&maincontext, &sched_context);
  printf("The program ended\n");
  return 0;
}
