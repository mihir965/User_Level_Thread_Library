// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#include <sys/ucontext.h>
#define _GNU_SOURCE

/* To use Linux pthread Library in Benchmark, you have to comment the
 * USE_WORKERS macro */
#define USE_WORKERS 1

/* Targeted latency in milliseconds */
#define TARGET_LATENCY 20

/* Minimum scheduling granularity in milliseconds */
#define MIN_SCHED_GRN 1

/* Time slice quantum in milliseconds */
#define QUANTUM 10

/* Defining max number of threads 1024, since I will be using a queue at the
 * beginning*/
#define MAX_THREADS 1024

/* include lib header files that you need here: */
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

typedef uint worker_t;

/* Creating this enums so that we can use this in the TCB as the  */
typedef enum {
  THREAD_RUNNING,
  THREAD_READY,
  THREAD_TERMINATED,
} thread_state;

typedef struct TCB {
  /* add important states in a thread control block */
  // thread Id
  // thread status
  // thread context
  // thread stack
  // thread priority
  // And more ...

  // YOUR CODE HERE
  uint tid;           // This is the uuid of the thread
  thread_state state; // The thread state that I defined
  ucontext_t context; // The context that will get initialized in worker_create
  void *
      stack_base; // We malloc the stack for each thread. What we're
                  // implementing is user-level threads. Kernel threads are
                  // allocated stacks by the kernel or os, therefore do not need
                  // malloc, but since we are abstacting the behaviour of
                  // threads, we need to allocate seperate space for each thread
  uint stack_size;
  int priority;   // This will be the nice value.

} tcb;

/* mutex struct definition */
typedef struct worker_mutex_t {
  /* add something here */

  // YOUR CODE HERE
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue
// etc...)

// YOUR CODE HERE

/* Function Declarations: */

/* fucntion to create the scheduler context only once */
void create_sched_context();

/* create a new thread */
int worker_create(worker_t *thread, pthread_attr_t *attr,
                  void *(*function)(void *), void *arg);

/* give CPU pocession to other user level worker threads voluntarily */
int worker_yield();

/* terminate a thread */
void worker_exit(void *value_ptr);

/* wait for thread termination */
int worker_join(worker_t thread, void **value_ptr);

/* initial the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,
                      const pthread_mutexattr_t *mutexattr);

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex);

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex);

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex);

/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void);

#ifdef USE_WORKERS
#define pthread_t worker_t
#define pthread_mutex_t worker_mutex_t
#define pthread_create worker_create
#define pthread_exit worker_exit
#define pthread_join worker_join
#define pthread_mutex_init worker_mutex_init
#define pthread_mutex_lock worker_mutex_lock
#define pthread_mutex_unlock worker_mutex_unlock
#define pthread_mutex_destroy worker_mutex_destroy
#endif

#endif
