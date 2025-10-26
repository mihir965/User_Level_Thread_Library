// File:	thread-worker.c
// List all group member's name:
// mak575 Mihir Kulkarni
// username of iLab:
// iLab Server:

#include "thread-worker.h"
#include <bits/types/sigset_t.h>
#include <time.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ucontext.h>
#include <ucontext.h>

// Global counter for total context switches and
// average turn around and response time
long tot_cntx_switches = 0;
double avg_turn_time = 0;
double avg_resp_time = 0;

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
/* Defining a t_id counter */
static int t_id = 0;
/* Defining the shced_context and the initializer boolean variable to ensure
 * that the shced_context is only initialized once. Also creating a main_context
 * here to return to */
ucontext_t sched_context;
static ucontext_t main_context;
static int scheduler_initialized = 0;
static int main_context_captured = 0;

/* I will initialize all the data structures needed for the schedulers here */
#if defined (RR)
/* Initializing the head of the linked list */
static q_thread *head = NULL;
#elif defined (PSJF)
// Need a min-heap for PSJF and for CFS
static tcb *run_heap[MAX_THREADS];
static int heap_size = 0;
#endif

/* We also require a global linked list of all the threads ever created, this
* will allow us to join threads that are not necessarily in the ready queue,
* which is the one that we setup above */
static tcb *current_thread = NULL;
static q_thread *global_head = NULL;
static sigset_t signal_mask;
static int timerstarted = 0;

void block_signals() { sigprocmask(SIG_BLOCK, &signal_mask, NULL); }

void unblock_signals() { sigprocmask(SIG_UNBLOCK, &signal_mask, NULL); }

static inline uint64_t now_us(void){
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ull + (uint64_t)ts.tv_nsec/1000ull;
}


/* Forward referencing schedule()*/
static void schedule();
void preempt(int signum);

/* Main context is the kind of thing that should basically run when the main
 * thread is trying to run an API funciton, this way, we have something to
 * return to */
void save_main_context_if_needed() {
  // printf("[DEBUG]: save_main_context_if_needed\n");
  if (!main_context_captured) {
    if (getcontext(&main_context) < 0) {
      perror("Error getting the main_context\n");
      exit(1);
    }
  }
  main_context_captured = 1;
}

/* Auxillary funcitons for the global list */
void enqueue_globally(tcb *new_thread) {
  // printf("[DEBUG]: enqueue_globally\n");
  q_thread *node = malloc(sizeof(q_thread));
  node->thread_tcb = new_thread;
  node->next = NULL;

  if (global_head == NULL) {
    /* No threads yet */
    global_head = node;
    // printf("thread (%d) was enqueued into the global thread list\n",
    // new_thread->tid);
    return;
  }
  q_thread *temp = global_head;
  while (temp->next != NULL)
    temp = temp->next;
  temp->next = node;
  // printf("thread (%d) was enqueued into the global thread list\n",
  // new_thread->tid);
  return;
}

void remove_globally(tcb *new_thread) {
  // printf("[DEBUG]: remove_globally\n");
  if (global_head == NULL)
    return;
  q_thread *curr = global_head;
  q_thread *prev = NULL;
  while (curr != NULL) {
    if (curr->thread_tcb == new_thread) {
      if (prev == NULL) {
        global_head = curr->next;
      } else {
        prev->next = curr->next;
      }
      free(curr);
      return;
    }
    prev = curr;
    curr = curr->next;
  }
}

tcb *find_tcb_by_tid(worker_t tid) {
  // printf("[DEBUG]: find_tcb_by_tid\n");
  q_thread *temp = global_head;
  while (temp != NULL) {
    if (temp->thread_tcb->tid == tid) {
      return temp->thread_tcb;
    }
    temp = temp->next;
  }
  // printf("(%d) could not be found in the global list\n", tid);
  return NULL;
}

/* defining some auxillary functions for the data structure supporting the
 * scheduling policy */
void enqueue(tcb *new_thread) {
  // printf("[DEBUG]: Enquque\n");
#if defined (RR)
  q_thread *node = malloc(sizeof(q_thread));
  node->thread_tcb = new_thread;
  node->next = NULL;

  if (head == NULL) {
    head = node;
    // printf("thread (%d) has been enqueued into the scheduler\n",
    // new_thread->tid);
    return;
  }

  q_thread *temp = head;
  while (temp->next != NULL) {
    temp = temp->next;
  }
  temp->next = node;
  // printf("thread (%d) has been enqueued into the scheduler\n",
  // new_thread->tid);
  return;
#elif defined (PSJF) || defined (CFS)
  heap_push(new_thread);
  return;
#endif

}

/* We will return the head of the queue and advance the queue to the next node
 * (tcb) */
tcb *dequeue() {
  // printf("[DEBUG]: dequeue\n");
#if defined (RR)
  if (head == NULL) {
    // printf("[DEBUG]: There are no threads in the runqueue\n");
    return NULL;
  }
  q_thread *node = head;
  tcb *to_return = node->thread_tcb;
  head = head->next;
  /* We cree the node that is getting returned through dequeue */
  free(node);
  return to_return;
#elif defined(CFS) || defined (PSJF)
  return heap_pop();
#endif
  return NULL;
}

void delete_from_queue(tcb *new_thread) {
  // printf("[DEBUG]: delete_from_queue\n");
#if defined (RR)
  if (head == NULL)
    return;

  /* We create curr and prev since we will want to reorder the list once we
   * delete a node. This is basic linked list logic */
  q_thread *curr = head;
  q_thread *prev = NULL;

  while (curr != NULL) {
    if (curr->thread_tcb == new_thread) {
      if (prev == NULL) {
        /* This means we are deleteing the first tcb only */
        head = curr->next;
      } else {
        prev->next = curr->next;
      }
      free(curr);
      return;
    }
    prev = curr;
    curr = curr->next;
  }
  // printf("Thread not found\n");
  return;
#elif defined (CFS) || defined (PSJF)
  heap_remove(new_thread);
  return;
#endif
}

void create_sched_context() {
  // printf("[DEBUG]: create_sched_context\n");
  /* Since we also want to create the scheduler context that we want to return
   * to after the worker fn is done returning, we can use static keyword to only
   * initialize this context once in the program */
  if (scheduler_initialized)
    return;
  if (getcontext(&sched_context) < 0) {
    perror("Error while initializing the sched_context\n");
    exit(1);
  }

  // printf("Creating scheduler context\n");
  sched_context.uc_stack.ss_sp = malloc(SIGSTKSZ);
  sched_context.uc_stack.ss_size = SIGSTKSZ;
  sched_context.uc_stack.ss_flags = 0;
  sched_context.uc_link = NULL;
  makecontext(&sched_context, schedule, 0);
  // printf("[DEBUG]: Did this run?\n");
  scheduler_initialized = 1;
}

/* create a new thread */
int worker_create(worker_t *thread, pthread_attr_t *attr,
                  void *(*function)(void *), void *arg) {
  // printf("[DEBUG]: Worker_create\n");
  // - create Thread Control Block (TCB)
  // - create and initialize the context of this worker thread
  // - allocate space of stack for this thread to run
  // after everything is set, push this thread into run queue and
  // - make it ready for the execution.

  // YOUR CODE HERE

  /* Another thing that I noticed when degbugging for my basic RR scheduler, is
   * that as mentioned in the write up, the code needs a main_context to return
   * to so that the benchmark code can complete. */
  save_main_context_if_needed();

  /* We are instantiating the scheduler only when the first worker is getting
   * created. More over, the scheduelr context will be created only once since
   * we have the scheduler_initialized variable */
  create_sched_context();

  tcb *new_thread = malloc(sizeof(tcb));

  /* after creating the tcb of each thread on the heap, we define it */
  new_thread->tid = t_id++;
  *thread = new_thread->tid;

  /* allocating enough space for the stack of the thread */
  new_thread->stack_base = malloc(SIGSTKSZ);

  /* Setting up the context of the worker thread */
  if (getcontext(&new_thread->context) < 0) {
    perror("getcontext");
    exit(1);
  }
  new_thread->context.uc_stack.ss_sp = new_thread->stack_base;
  new_thread->context.uc_stack.ss_size = SIGSTKSZ;
  new_thread->context.uc_stack.ss_flags = 0;
  new_thread->retval = NULL;
  new_thread->context.uc_link = &sched_context;

  new_thread->elapsed = 0;
  new_thread->vruntime = 0;
  new_thread->q_level = 0;
  new_thread->slice_used = 0;
  new_thread->last_start_time = 0;

  makecontext(&new_thread->context, (void (*)())function, 1,
              arg); // The void* (*function) (void*) means that the function can
                    // return and pass in any type of data. void* is used for
                    // loose defining the type that is returned or passed in

  new_thread->state = THREAD_READY;
  new_thread->priority = 0;

  /* We then enqueue this tcb into our global linked list that we use for
   * scheuling */
  enqueue(new_thread);
  enqueue_globally(new_thread);
  return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
  // printf("[DEBUG]: Worker_yeild\n");
  // - change worker thread's state from Running to Ready
  // - save context of this thread to its thread control block
  // - switch from thread context to scheduler context
  // YOUR CODE HERE

  /* All this has to do is change the state of the thread, save the context and
   * then swap context with the scheduer context */
  if (!current_thread) {
    // printf("There is no thread running to yeild!\n");
    return -1;
  }
  // printf("[DEBUG]: swapping to sched_context\n");
  current_thread->state = THREAD_READY;
  swapcontext(&current_thread->context, &sched_context);
  return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
  // printf("[DEBUG] Worker_exit\n");
  // - de-allocate any dynamic memory created when starting this thread

  // YOUR CODE HERE
  current_thread->state = THREAD_TERMINATED;
  current_thread->retval =
      value_ptr; // This will be assined to the **value_ptr in worker_join

  /* Go back to the scheduler context  */
  swapcontext(&current_thread->context, &sched_context);
};

/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
  // printf("[DEBUG]: Worker_Join\n");
  // - wait for a specific thread to terminate
  // - de-allocate any dynamic memory created by the joining thread

  // YOUR CODE HERE
  /* We need to save the main_context first because this can be another entry
   * into the thread_library */
  save_main_context_if_needed();

  /* We need to find the thread first */
  /* One problem I noticed is that in benchmarks worker_join is called without
   * explicitly calling schedule(), so if there is no current_thread, we need to
   * go to the scheduelr_context, run the schduler */
  // printf("Joining thread (%d)\n", thread);

  if (!current_thread) {
    // printf("Scheduler hasn't yet run\n");
    swapcontext(&main_context, &sched_context);
  }

  tcb *target_thread = find_tcb_by_tid(thread);

  if (!target_thread) {
    // printf("Could not find the thread\n");
    return -1;
  }

  while (1) {
    if (target_thread->state == THREAD_TERMINATED)
      break;
#if defined (RR)
    if (head == NULL && target_thread->state != THREAD_TERMINATED) {
      // printf("[DEBUG]: No runnable threads and target not terminated —
      // assuming done.\n");
      break;
    }
#elif defined (PSJF) || defined (CFS)
    if(heap_size==0 && target_thread->state != THREAD_TERMINATED){
       printf("[DEBUG]: No runnable threads and target not terminated — assuming done.\n");
       break;
    }
#endif

    swapcontext(&main_context, &sched_context);
  }

  if (value_ptr)
    *value_ptr = target_thread->retval;

  // printf("Removing the target_thread from the global and run queue\n");

  delete_from_queue(target_thread);
  remove_globally(target_thread);

  free(target_thread->stack_base);
  free(target_thread);

  // printf("Done removing the thread\n");
  return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex,
                      const pthread_mutexattr_t *mutexattr) {
  // printf("[DEBUG]: Mutex Init\n");
  //- initialize data structures for this mutex

  // YOUR CODE HERE
  /* The stdatomic.c library provides a way to clear the flag */
  atomic_flag_clear(&mutex->lock_flag);
  mutex->holder_tid = -1;
  mutex->head = NULL;

  return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {
  // printf("[DEBUG]: Mutex Lock\n");
  // - use the built-in test-and-set atomic function to test the mutex
  // - if the mutex is acquired successfully, enter the critical section
  // - if acquiring mutex fails, push current thread into block list and
  // context switch to the scheduler thread

  // YOUR CODE HERE
  for (;;) {
    block_signals();

    /* Trying to acquire the lock */
    if (!atomic_flag_test_and_set(&mutex->lock_flag)) {
      /* This means acquired */
      mutex->holder_tid = current_thread->tid;
      // printf("[DEBUG]: The mutex now blongs to (%d)\n", current_thread->tid);
      unblock_signals();
      return 0;
    }
    /* This means that eh lock is in contention */
    printf("[DEBUG]: Adding thread (%d) to the mutex's blocked queue\n",
           current_thread->tid);
    current_thread->state = THREAD_BLOCKED;

    /* Append to wait queue */
    if (!mutex->head) {
      mutex->head = malloc(sizeof(q_thread));
      mutex->head->thread_tcb = current_thread;
      mutex->head->next = NULL;
    } else {
      q_thread *t = mutex->head;
      while (t->next) {
        t = t->next;
      }
      t->next = malloc(sizeof(q_thread));
      t->next->thread_tcb = current_thread;
      t->next->next = NULL;
    }
    /* Go to scheduler */
    printf("[DEBUG]: Swapping back to scheduler\n");
    unblock_signals();
    delete_from_queue(current_thread);
    swapcontext(&current_thread->context, &sched_context);
  }
  return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
  // printf("[DEBUG]: Mutex Unlock\n");
  // - release mutex and make it available again.
  // - put threads in block list to run queue
  // so that they could compete for mutex later.

  // YOUR CODE HERE
  block_signals();
  if (mutex->holder_tid != current_thread->tid) {
    // printf("Mutex unlock tried by (%d), owner is (%d)", current_thread->tid,
    // mutex->holder_tid);
    exit(1);
  }
  atomic_flag_clear(&mutex->lock_flag);
  mutex->holder_tid = -1;
  if (mutex->head == NULL) {
    /* This means that no threads are contending for the lock at the moment */
    // printf(
    // "[DEBUG]: No one is contending for the lock\nReturning to scheduler\n");
    // worker_yield();
    unblock_signals();
    return 0;
  }
  // printf("[DEBUG]: Waking a thread (%d)\n", mutex->head->thread_tcb->tid);
  mutex->head->thread_tcb->state = THREAD_READY;
  enqueue(mutex->head->thread_tcb);
  q_thread *temp = mutex->head;
  mutex->head = mutex->head->next;
  free(temp);
  unblock_signals();
  swapcontext(&current_thread->context, &sched_context);
  return 0;
};

/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
  // printf("[DEBUG]: Mutex_destroy\n");
  // - de-allocate dynamic memory created in worker_mutex_init
  if (mutex->holder_tid != -1) {
    /* Restore flag state */
    atomic_flag_clear(&mutex->lock_flag);
    // printf("Cannot destroy mutex: it is currently locked by thread (%d)\n",
    // current_thread->tid);
    return -1;
  }

  /* Make sure that no threads are there in the waiting queue */
  if (mutex->head != NULL) {
    // printf("Cannot destroy mutex: there are threads still waiting\n");
    return -1;
  }

  /* Resetting */
  mutex->holder_tid = -1;
  mutex->head = NULL;

  return 0;
};

void preempt(int signum) {
  if (current_thread && current_thread->state == THREAD_RUNNING) {
    // printf("[DEBUG]: Timer interrupt -> yielding thread (%d)\n",
    // current_thread->tid);
#if defined (RR)
    current_thread->state = THREAD_READY;
    swapcontext(&current_thread->context, &sched_context);
#elif defined (PSJF)
    current_thread->elapsed+=1;
    current_thread->state = THREAD_READY;
    swapcontext(&current_thread->context, &sched_context);
#endif
  }
}

static void sched_rr() {
  // printf("[DEBUG]: Entered sched_rr()\n");

  // Use sigaction to register signal handler
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = &preempt;
  sigaction(SIGPROF, &sa, NULL);

  sigemptyset(&signal_mask);
  sigaddset(&signal_mask, SIGPROF);

  // Create timer struct
  struct itimerval timer;

  // Set up what the timer should reset to after the timer goes off
  timer.it_interval.tv_usec = 14000;
  timer.it_interval.tv_sec = 0;

  // Set up the current timer to go off in 1 second
  // Note: if both of the following values are zero
  //       the timer will not be active, and the timer
  //       will never go off even if you set the interval value
  timer.it_value.tv_usec = 14000;
  timer.it_value.tv_sec = 0;

  // Set the timer up (start the timer)
  setitimer(ITIMER_PROF, &timer, NULL);

  while (1) {
    tcb *next = dequeue();

    if (!next) {
      /* There are no more threads on the run_queue */
      // printf(
      // "[DEBUG]: No runnable threads on the run_queue. Returning to main\n");
      struct itimerval stop_timer = {0};
      setitimer(ITIMER_PROF, &stop_timer, NULL);

      /* Return to main only once after all threads are done */
#if defined (RR)
      if (head == NULL) {
        // printf("[DEBUG]: All threads completed. Returning to main once.\n");
        setcontext(&main_context);
      } else {
        // If threads exist but are BLOCKED (e.g. waiting on mutex), keep
        // looping printf("[DEBUG]: Waiting for blocked threads to become
        // runnable.\n");
      }
#endif
      continue;
    }

    current_thread = next;
    current_thread->state = THREAD_RUNNING;
    tot_cntx_switches++;

    // printf("[DEBUG]: Switching to thread (%d)\n", current_thread->tid);
    swapcontext(&sched_context, &current_thread->context);

    // printf("[DEBUG]: Back to scheduler after thread (%d)\n",
    // current_thread->tid);

    if (current_thread->state == THREAD_RUNNING) {
      // printf("[DEBUG]: Thread yeilded from unlock mostly, will be exited, not
      // " "enqueuing\n");
      continue;
    }

    if (current_thread->state == THREAD_TERMINATED) {
      // printf("[DEBUG]: Thread done, removing from run_queue\n");
    }

    if (current_thread->state == THREAD_BLOCKED) {
      // printf("This thread is blocekd (%d)\n", current_thread->tid);
    }

    if (current_thread->state == THREAD_READY) {
      enqueue(current_thread);
    }
  }
}

/* Code for the min_heap implementation */
/* All the functions have been modifed to run differently based on the scheudler chosen */

void heap_swap(int i, int j){
    tcb* temp = run_heap[i];
    run_heap[i] = run_heap[j];
    run_heap[j] = temp;
}

void heapify_down(int i){
    while(1){
        int left = 2*i + 1;
        int right = 2*i + 2;
        int smallest = i;
#if defined (PSJF)
        if(left<heap_size && run_heap[left]->elapsed < run_heap[smallest]->elapsed) smallest = left;
        if(right<heap_size && run_heap[right]->elapsed < run_heap[smallest]->elapsed) smallest = right;

#elif defined (CFS)
        if(left<heap_size && run_heap[left]->vruntime < run_heap[smallest]->vruntime) smallest = left;
        if(right<heap_size && run_heap[right]->vruntime < run_heap[smallest]->vruntime) smallest = right;
#endif
        if(smallest == i) break;
        heap_swap(i, smallest);
        i = smallest;
    }
}

void heapify_up(int i){
    while(i>0){
        int parent = (i-1)/2;
#if defined (PSJF)
        if(run_heap[parent]->elapsed <= run_heap[i]->elapsed) break;
#elif defined(CFS)
        if(run_heap[parent]->vruntime <= run_heap[i]->vruntime) break;
#endif
        heap_swap(i, parent);
        i = parent;
    }
}

void heap_push(tcb* thread){
    run_heap[heap_size] = thread;
    int i = heap_size;
    heap_size++;
    heapify_up(i);
}

tcb* heap_pop(){
    if(heap_size==0) return NULL;
    tcb* min = run_heap[0];
    run_heap[0] = run_heap[--heap_size];
    heapify_down(0);
    return min;
}

void heap_remove(tcb *thread){
    int i;
    for(i=0; i<heap_size; i++){
        if(run_heap[i] == thread) break;
    }
    if(i==heap_size) return;
    run_heap[i] = run_heap[--heap_size];
    heapify_down(i);
    heapify_up(i);
}

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
  // - your own implementation of PSJF
  // (feel free to modify arguments and return types)

  // YOUR CODE HERE
  struct sigaction sa; 
  memset(&sa, 0, sizeof sa);
  sa.sa_handler = &preempt;
  sigaction(SIGPROF, &sa, NULL);

  struct itimerval timer;
  timer.it_interval.tv_sec=0; timer.it_interval.tv_usec = QUANTUM;
  timer.it_value.tv_sec = 0; timer.it_value.tv_usec = QUANTUM;
  setitimer(ITIMER_PROF, &timer, NULL);

  for(;;){
      block_signals();
      tcb* next = dequeue();
      unblock_signals();
      if(!next){
          struct itimerval stop = {0}; setitimer(ITIMER_PROF, &stop, NULL);
          setcontext(&main_context);
      }
      current_thread = next;
      current_thread->state = THREAD_RUNNING;
      current_thread->last_start_time = now_us();
      tot_cntx_switches++;

      /* Run the scheduler */
      swapcontext(&sched_context, &current_thread->context);

      /* This is where a preempt, yeild or exit will come */
      if(current_thread->state == THREAD_TERMINATED)continue;
      if(current_thread->state == THREAD_READY) enqueue(current_thread);
  }
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
  // - your own implementation of MLFQ
  // (feel free to modify arguments and return types)

  // YOUR CODE HERE

  /* Step-by-step guidances */
  // Step1: Calculate the time current thread actually ran
  // Step2.1: If current thread uses up its allotment, demote it to the low
  // priority queue (Rule 4) Step2.2: Otherwise, push the thread back to its
  // origin queue Step3: If time period S passes, promote all threads to the
  // topmost queue (Rule 5) Step4: Apply RR on the topmost queue with entries
  // and run next thread
}

/* Completely fair scheduling algorithm */
static void sched_cfs() {
  // - your own implementation of CFS
  // (feel free to modify arguments and return types)

  // YOUR CODE HERE

  /* Step-by-step guidances */

  // Step1: Update current thread's vruntime by adding the time it actually ran
  // Step2: Insert current thread into the runqueue (min heap)
  // Step3: Pop the runqueue to get the thread with a minimum vruntime
  // Step4: Calculate time slice based on target_latency (TARGET_LATENCY),
  // number of threads within the runqueue Step5: If the ideal time slice is
  // smaller than minimum_granularity (MIN_SCHED_GRN), use MIN_SCHED_GRN instead
  // Step5: Setup next time interrupt based on the time slice
  // Step6: Run the selected thread
}

/* I am forward referencing this function on the top of this script so that I
 * can reference it as the function in the make context function in the
 * sched_context */

/* scheduler */
static void schedule() {
  // printf("[DEBUG]: Schedule called\n");
  // - every time a timer interrupt occurs, your worker thread library
  // should be contexted switched from a thread context to this
  // schedule() function

  // YOUR CODE HERE
  //  - invoke scheduling algorithms according to the policy (PSJF or MLFQ or
  //  CFS)
#if defined(PSJF)
  sched_psjf();
#elif defined(MLFQ)
  sched_mlfq();
#elif defined(CFS)
  sched_cfs();
#elif defined(RR)
  sched_rr();
#else
#error "Define one of PSJF, MLFQ, or CFS when compiling. e.g. make SCHED=MLFQ";
#endif
}

// DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

  fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
  fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
  fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}

// Feel free to add any other functions you need

// YOUR CODE HERE
