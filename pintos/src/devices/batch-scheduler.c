/*
 * Exercise on thread synchronization.
 *
 * Assume a half-duplex communication bus with limited capacity, measured in
 * tasks, and 2 priority levels:
 *
 * - tasks: A task signifies a unit of data communication over the bus
 *
 * - half-duplex: All tasks using the bus should have the same direction
 *
 * - limited capacity: There can be only 3 tasks using the bus at the same time.
 *                     In other words, the bus has only 3 slots.
 *
 *  - 2 priority levels: Priority tasks take precedence over non-priority tasks
 *
 *  Fill-in your code after the TODO comments
 */

#include <stdio.h>
#include <string.h>

#include "tests/threads/tests.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "timer.h"

/* This is where the API for the condition variables is defined */
#include "threads/synch.h"

/* This is the API for random number generation.
 * Random numbers are used to simulate a task's transfer duration
 */
#include "lib/random.h"

#define MAX_NUM_OF_TASKS 200

#define BUS_CAPACITY 3

typedef enum {
  SEND,
  RECEIVE,

  NUM_OF_DIRECTIONS
} direction_t;

typedef enum {
  NORMAL,
  PRIORITY,

  NUM_OF_PRIORITIES
} priority_t;

typedef struct {
  direction_t direction;
  priority_t priority;
  unsigned long transfer_duration;
} task_t;

/* Locks */
static struct lock bus_lock;
/* Conditions */
static struct condition c_prio[2];
static struct condition c_norm[2];
/* other global variable*/
static int on_bus;
static int current_direction;
static int waiters_normal[2];
static int waiters_priority[2];

void init_bus (void);
void batch_scheduler (unsigned int num_priority_send,
                      unsigned int num_priority_receive,
                      unsigned int num_tasks_send,
                      unsigned int num_tasks_receive);

/* Thread function for running a task: Gets a slot, transfers data and finally
 * releases slot */
static void run_task (void *task_);

/* WARNING: This function may suspend the calling thread, depending on slot
 * availability */
static void get_slot (const task_t *task);

/* Simulates transfering of data */
static void transfer_data (const task_t *task);

/* Releases the slot */
static void release_slot (const task_t *task);

void init_bus (void) {

  random_init ((unsigned int)123456789);

  lock_init(&bus_lock);

  cond_init(&c_prio[0]);
  cond_init(&c_prio[1]);
  cond_init(&c_norm[0]);
  cond_init(&c_norm[1]);

  on_bus = 0;
  current_direction = -1; /* Bus yet to be used */
  waiters_normal[0] = waiters_normal[1] = 0; /* waiters[] counts PRIORITY waiters per direction */
  waiters_priority[0] = waiters_priority[1] = 0;
}

void batch_scheduler (unsigned int num_priority_send,
                      unsigned int num_priority_receive,
                      unsigned int num_tasks_send,
                      unsigned int num_tasks_receive) {
  ASSERT (num_tasks_send + num_tasks_receive + num_priority_send +
             num_priority_receive <= MAX_NUM_OF_TASKS);

  static task_t tasks[MAX_NUM_OF_TASKS] = {0};

  char thread_name[32] = {0};

  unsigned long total_transfer_dur = 0;

  int j = 0;

  /* create priority sender threads */
  for (unsigned i = 0; i < num_priority_send; i++) {
    tasks[j].direction = SEND;
    tasks[j].priority = PRIORITY;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "sender-prio");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create priority receiver threads */
  for (unsigned i = 0; i < num_priority_receive; i++) {
    tasks[j].direction = RECEIVE;
    tasks[j].priority = PRIORITY;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "receiver-prio");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create normal sender threads */
  for (unsigned i = 0; i < num_tasks_send; i++) {
    tasks[j].direction = SEND;
    tasks[j].priority = NORMAL;
    tasks[j].transfer_duration = random_ulong () % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "sender");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* create normal receiver threads */
  for (unsigned i = 0; i < num_tasks_receive; i++) {
    tasks[j].direction = RECEIVE;
    tasks[j].priority = NORMAL;
    tasks[j].transfer_duration = random_ulong() % 244;

    total_transfer_dur += tasks[j].transfer_duration;

    snprintf (thread_name, sizeof thread_name, "receiver");
    thread_create (thread_name, PRI_DEFAULT, run_task, (void *)&tasks[j]);

    j++;
  }

  /* Sleep until all tasks are complete */
  timer_sleep (2 * total_transfer_dur);
}

/* Thread function for the communication tasks */
void run_task(void *task_) {
  task_t *task = (task_t *)task_;

  get_slot (task);

  msg ("%s acquired slot", thread_name());
  transfer_data (task);

  release_slot (task);
}

static direction_t other_direction(direction_t this_direction) {
  return this_direction == SEND ? RECEIVE : SEND;
}


void get_slot (const task_t *task) {
  lock_acquire(&bus_lock);
  /* If the task has PRIORITY. */
  if (task->priority == PRIORITY){
    waiters_priority[task->direction]++;
    while ((on_bus == 3) || /* The bus is full (on_bus == 3). */
           (on_bus > 0 && current_direction != task->direction) ) /* The bus is in use by the opposite direction. */
      cond_wait(&c_prio[task->direction],&bus_lock);
    waiters_priority[task->direction]--;
  }
  /* Else, the task has NORMAL priority */
  else{
    waiters_normal[task->direction]++;
    while ((on_bus == 3) || /* The bus is full. */
           (on_bus > 0 && current_direction != task->direction) || /* The bus is in use by the opposite direction. */
           ((waiters_priority[0] > 0) || waiters_priority[1] > 0)) /* There are ANY priority tasks waiting (in either direction). */
      cond_wait(&c_norm[task->direction], &bus_lock);
    waiters_normal[task->direction]--;
  }

  on_bus++; /* Get on the bridge. */
  current_direction = task->direction;
  lock_release(&bus_lock);
}

void transfer_data (const task_t *task) {
  /* Simulate bus send/receive */
  timer_sleep (task->transfer_duration);
}

void release_slot (const task_t *task) {
  lock_acquire(&bus_lock);

  /* Decrease the number on the bus */
  on_bus--; 
  
  /* Store the directions of the task. */
  int d = task->direction; 
  
  /* If there are still tasks on the bus after this one leaves,
     we can only allow more tasks of the same direction to enter.
  */
  if(on_bus > 0){
    // If there are PRIORITY tasks waiting in the same direction, wake one
    if (waiters_priority[d] > 0){
      cond_signal(&c_prio[d], &bus_lock);
    }  
  }
  /* If the bus is empty (on_bus == 0), we get to choose
     which direction (and priority) to serve next. */
    else
  {
    /* PRIORITY tasks in the opposite direction. */
    if (waiters_priority[1-d] > 0){
      current_direction = 1-d;
      cond_broadcast(&c_prio[1-d],&bus_lock);
    }
    /* NORMAL tasks in the same  direction. */
    else if (waiters_normal[d] > 0){
      cond_signal(&c_norm[d], &bus_lock);
    }  
    /* NORMAL tasks in the oppisite direction. */
    else if (waiters_normal[1-d] > 0){
      current_direction = 1-d;
      cond_broadcast(&c_norm[1-d],&bus_lock);
    }
  }

  lock_release(&bus_lock);
}