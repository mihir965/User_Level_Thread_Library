// File:	worker_t.h

// List all group member's name:
// username of iLab:
// iLab Server:

#ifndef WORKER_T_H
#define WORKER_T_H

#include <stdatomic.h>
#include <stdint.h>
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

#define MLFQ_LEVELS 5
#define BASE_Q_MS 10 // For the highest-priority quantum
#define BOOST_MS 200 // rule 5: periodic boost interval
#define TICK_MS 10   // SIGPROF tick

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
  THREAD_BLOCKED,
} thread_state;

typedef struct QueueThread q_thread;

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
  int priority; // This will be the nice value.
  void *retval; // This is needed for join since, if worker_exit passes a value
                // ptr, it means that the return value needs to be saved. This
                // attribute/element of the tcb will return this value

  long elapsed;  // This is for knowing how long the thread executed for PSJF
  long vruntime; // For CFS
  long last_start_time; // We can measure how long the thread ran with this
  int q_level;          // For MLFQ
  int slice_used;       // How much of the current level's slice it has used
  uint64_t create_time;
  uint64_t start_time;
  uint64_t end_time;
  int has_started;

} tcb;

/* mutex struct definition */
typedef struct worker_mutex_t {
  /* add something here */

  // YOUR CODE HERE
  int holder_tid;        // The tid of the tcb that is holding this mutex lock
  atomic_flag lock_flag; // The flag that we will be calling the test-and-set
                         // function on
  struct QueueThread *head;
} worker_mutex_t;

/* define your data structures here: */
// Feel free to add your own auxiliary data structures (linked list or queue
// etc...)

// YOUR CODE HERE

// Gonna use a Linked List for runqueue
typedef struct QueueThread {
  struct QueueThread *next;
  tcb *thread_tcb;
} q_thread;

/* Function Declarations: */

void enqueue(tcb *new_thread);

tcb *dequeue();

void delete_from_queue(tcb *new_thread);

/* fnctions for global list */
void enqueue_globally(tcb *new_thread);

void remove_globally(tcb *new_thread);

tcb *find_tcb_by_tid(worker_t tid);

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

/* Functions for preemption etc */
void preempt(int signum);

// Need auxiliary functions for the min-heap
void heap_swap(int i, int j);

void heapify_up(int);

void heapify_down(int);

void heap_push(tcb *);

tcb *heap_pop();

void heap_remove(tcb *);

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
