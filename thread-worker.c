// File:	thread-worker.c
// List all group member's name:
// mak575 Mihir Kulkarni
// username of iLab:
// iLab Server:

#include "thread-worker.h"
#include <stdio.h>
#include <sys/ucontext.h>
#include <ucontext.h>

//Global counter for total context switches and 
//average turn around and response time
long tot_cntx_switches=0;
double avg_turn_time=0;
double avg_resp_time=0;

// INITAILIZE ALL YOUR OTHER VARIABLES HERE
// YOUR CODE HERE
/* Defining a t_id counter */
static int t_id = 0;
/* Defining the shced_context and the initializer boolean variable to ensure that the shced_context is only initialized once */
static ucontext_t sched_context;
static int scheduler_initialized = 0;

/* Initializing the head of the linked list */
static q_thread* head = NULL;


/* Forward referencing schedule()*/
static void schedule();

/* defining some auxillary functions for the data structure supporting the scheduling policy */
void enqueue(tcb* new_thread){
    q_thread* node = malloc(sizeof (q_thread));
    node->thread_tcb = new_thread;
    node->next = NULL;

    if(head==NULL){
        head = node;
        return;
    }

    q_thread* temp = head;
    while(temp->next != NULL){
        temp = temp->next;
    }
    temp->next = node;
    printf("thread (%d) has been enqueued into the scheduler\n", new_thread->tid);
    return;
}

/* We will return the head of the queue and advance the queue to the next node (tcb) */
tcb* dequeue(){
    if(head==NULL)
        return NULL;
    q_thread* node = head;
    tcb* to_return = node->thread_tcb;
    head = head->next;
    free(node);
    return to_return;
}

void delete_from_queue(tcb *new_thread){
    if(head==NULL) return;

    /* We create curr and prev since we will want to reorder the list once we delete a node. This is basic linked list logic */
    q_thread* curr = head;
    q_thread* prev = NULL;

    while(curr!=NULL){
        if(curr->thread_tcb == new_thread){
            if(prev==NULL){
                /* This means we are deleteing the first tcb only */
                head = curr->next;
            }else{
                prev->next = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
    return;
}

void create_sched_context(){
    /* Since we also want to create the scheduler context that we want to return to after the worker fn is done returning, we can use static keyword to only initialize this context once in the program */
    if(scheduler_initialized) return;
    if(getcontext(&sched_context)<0){
        perror("Error while initializing the sched_context\n");
        exit(1);
    }
    sched_context.uc_stack.ss_sp = malloc(SIGSTKSZ);
    sched_context.uc_stack.ss_size = SIGSTKSZ;
    sched_context.uc_stack.ss_flags = 0;
    sched_context.uc_link = NULL;
    makecontext(&sched_context, schedule, 0);
    scheduler_initialized = 1;
}

/* create a new thread */
int worker_create(worker_t * thread, pthread_attr_t * attr, 
                      void *(*function)(void*), void * arg) {

       // - create Thread Control Block (TCB)
       // - create and initialize the context of this worker thread
       // - allocate space of stack for this thread to run
       // after everything is set, push this thread into run queue and 
       // - make it ready for the execution.

       // YOUR CODE HERE

    create_sched_context();

    tcb* new_thread = malloc(sizeof(tcb));

    /* after creating the tcb of each thread on the heap, we define it */
    new_thread->tid = t_id++;
    *thread = new_thread->tid;

    /* allocating enough space for the stack of the thread */
    new_thread->stack_base = malloc(SIGSTKSZ);



    /* Setting up the context of the worker thread */
    if(getcontext(&new_thread->context) < 0){
        perror("getcontext");
        exit(1);
    }
    new_thread->context.uc_stack.ss_sp = new_thread->stack_base;
    new_thread->context.uc_stack.ss_size = SIGSTKSZ;
    new_thread->context.uc_stack.ss_flags = 0;
    new_thread->context.uc_link = &sched_context;
    makecontext(&new_thread->context, (void(*)()) function, 1, arg); // The void* (*function) (void*) means that the function can return and pass in any type of data. void* is used for loose defining the type that is returned or passed in

    new_thread->state = THREAD_READY;
    new_thread->priority = 0;

    /* We then enqueue this tcb into our global linked list that we use for scheuling */
    enqueue(new_thread);

    return 0;
};

/* give CPU possession to other user-level worker threads voluntarily */
int worker_yield() {
	
	// - change worker thread's state from Running to Ready
	// - save context of this thread to its thread control block
	// - switch from thread context to scheduler context

	// YOUR CODE HERE
	
	return 0;
};

/* terminate a thread */
void worker_exit(void *value_ptr) {
	// - de-allocate any dynamic memory created when starting this thread

	// YOUR CODE HERE
};


/* Wait for thread termination */
int worker_join(worker_t thread, void **value_ptr) {
	
	// - wait for a specific thread to terminate
	// - de-allocate any dynamic memory created by the joining thread
  
	// YOUR CODE HERE
	return 0;
};

/* initialize the mutex lock */
int worker_mutex_init(worker_mutex_t *mutex, 
                          const pthread_mutexattr_t *mutexattr) {
	//- initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int worker_mutex_lock(worker_mutex_t *mutex) {

        // - use the built-in test-and-set atomic function to test the mutex
        // - if the mutex is acquired successfully, enter the critical section
        // - if acquiring mutex fails, push current thread into block list and
        // context switch to the scheduler thread

        // YOUR CODE HERE
        return 0;
};

/* release the mutex lock */
int worker_mutex_unlock(worker_mutex_t *mutex) {
	// - release mutex and make it available again. 
	// - put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int worker_mutex_destroy(worker_mutex_t *mutex) {
	// - de-allocate dynamic memory created in worker_mutex_init

	return 0;
};

/* Pre-emptive Shortest Job First (POLICY_PSJF) scheduling algorithm */
static void sched_psjf() {
	// - your own implementation of PSJF
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}


/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// - your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

	/* Step-by-step guidances */
	// Step1: Calculate the time current thread actually ran
	// Step2.1: If current thread uses up its allotment, demote it to the low priority queue (Rule 4)
	// Step2.2: Otherwise, push the thread back to its origin queue
	// Step3: If time period S passes, promote all threads to the topmost queue (Rule 5)
	// Step4: Apply RR on the topmost queue with entries and run next thread
}

/* Completely fair scheduling algorithm */
static void sched_cfs(){
	// - your own implementation of CFS
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

	/* Step-by-step guidances */

	// Step1: Update current thread's vruntime by adding the time it actually ran
	// Step2: Insert current thread into the runqueue (min heap)
	// Step3: Pop the runqueue to get the thread with a minimum vruntime
	// Step4: Calculate time slice based on target_latency (TARGET_LATENCY), number of threads within the runqueue
	// Step5: If the ideal time slice is smaller than minimum_granularity (MIN_SCHED_GRN), use MIN_SCHED_GRN instead
	// Step5: Setup next time interrupt based on the time slice
	// Step6: Run the selected thread
}


/* I am forward referencing this function on the top of this script so that I can reference it as the function in the make context function in the sched_context */

/* scheduler */
static void schedule() {
	// - every time a timer interrupt occurs, your worker thread library 
	// should be contexted switched from a thread context to this 
	// schedule() function
	
	//YOUR CODE HERE
// 	// - invoke scheduling algorithms according to the policy (PSJF or MLFQ or CFS)
// #if defined(PSJF)
//     	sched_psjf();
// #elif defined(MLFQ)
// 	sched_mlfq();
// #elif defined(CFS)
//     	sched_cfs();  
// #else
// 	# error "Define one of PSJF, MLFQ, or CFS when compiling. e.g. make SCHED=MLFQ"
// #endif
}



//DO NOT MODIFY THIS FUNCTION
/* Function to print global statistics. Do not modify this function.*/
void print_app_stats(void) {

       fprintf(stderr, "Total context switches %ld \n", tot_cntx_switches);
       fprintf(stderr, "Average turnaround time %lf \n", avg_turn_time);
       fprintf(stderr, "Average response time  %lf \n", avg_resp_time);
}


// Feel free to add any other functions you need

// YOUR CODE HERE

