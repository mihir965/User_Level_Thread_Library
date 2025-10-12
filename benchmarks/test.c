#include "../thread-worker.h"
#include <pthread.h>
#include <stdio.h>
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

int f1_ran = 0;

void *fun(void *arg) {
  char *name = (char *)arg;
  printf("Hello (%s)\n", name);
  f1_ran = 1;
  return NULL;
}

int main(int argc, char **argv) {

  /* Implement HERE */
  worker_t w1, w2;
  int ret = worker_create(&w1, NULL, fun, "Mihir");
  if (ret == 0) {
    printf("Thread created successfully (id=%d)\n", w1);
  } else {
    printf("Failed to create thread\n");
  }

  int ret2 = worker_create(&w2, NULL, fun, "Kulkarni");
  if (ret2 == 0) {
    printf("Thread created successfully (id=%d)\n", w2);
  } else {
    printf("Failed to create thread\n");
  }

  tcb *fn1 = dequeue();
  if (fn1 == NULL) {
    printf("Error no thread available in the ready queue\n");
    exit(1);
  }

  printf("Switching to thread id (%d) now\n", fn1->tid);

  ucontext_t main_context;

  if (getcontext(&main_context) < 0) {
    printf("Error creating the main context\n");
    exit(1);
  }

  fn1->context.uc_link = &main_context;

  fflush(stdout);
  
  if(!f1_ran){
      setcontext(&fn1->context);
  }

  tcb *fn2 = dequeue();

  printf("Switching to thread id (%d) now\n", fn2->tid);

  setcontext(&fn2->context);

  printf("This should not run\n");

  return 0;
}
