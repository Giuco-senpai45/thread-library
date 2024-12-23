#include "ult.h"
#include "utils.h"
#include "mutex.h"

#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <stdio.h>
#include "errno.h"

#define STOPSIG SIGALRM

struct itimerval schedule_clock;
struct sigaction alarm;

static size_t thread_count = 0;
static ult_t *running = NULL;
static ucontext_t *main_context;
static ult_t threads_list[MAX_THREADS_COUNT];

ult_t *init_next_ult(state_t state)
{
  ult_t *t = &threads_list[thread_count];
  t->tid = thread_count;
  t->state = state;
  t->waiting_for = -1;
  t->has_joiner = false;
  t->joiner = -1;

  if (getcontext(&t->context) == -1)
  {
    perror("Failed to get context");
    exit(EXIT_FAILURE);
  }
  thread_count++;

  return t;
}

static ult_t *get_next_ready_thread()
{
  tid_t next_tid;
  bool found = false;
  size_t checked = 0;
  while (checked < thread_count && !found)
  {
    next_tid = (running->tid + checked + 1) % thread_count;
    if (ULT_READY == threads_list[next_tid].state)
    {
      found = true;
      break;
    }
    checked++;
  }

  if (!found)
  {
    bool all_blocked = true;
    for (size_t i = 0; i < thread_count; i++)
    {
      if (threads_list[i].state != ULT_BLOCKED &&
          threads_list[i].state != ULT_TERMINATED)
      {
        all_blocked = false;
        break;
      }
    }

    if (all_blocked)
    {
      printf("\n=== DEADLOCK DETECTED ===\n");
      printf("All threads are blocked. Current state:\n");

      for (size_t i = 0; i < thread_count; i++)
      {
        ult_t *t = &threads_list[i];
        printf("Thread %ld: %s\n", t->tid,
               t->state == ULT_BLOCKED ? "BLOCKED" : "TERMINATED");
      }

      display_deadlocks();
      printf("=== Program stopped due to deadlock ===\n");
      exit(EXIT_FAILURE);
    }
  }

  return &threads_list[next_tid];
}

void ult_schedule(int signum)
{
  if (0 == thread_count)
  {
    exit(EXIT_SUCCESS);
  }

  // when we switch context, block incoming termination signals to the scheduler so it doesnt tell itself to stop
  block_signals();
  ult_t *current_t = running;
  running = get_next_ready_thread();
  unblock_signals();
  swapcontext(&current_t->context, &running->context);
}

static int create_scheduler(long quota)
{
  memset(&alarm, 0, sizeof(struct sigaction));
  alarm.sa_handler = &ult_schedule; // scheduler decides who is the next to get CPU

  sigemptyset(&alarm.sa_mask); // set what signals we react to (in our case STOPSIG)
  sigaddset(&alarm.sa_mask, STOPSIG);
  alarm.sa_flags |= SA_RESTART; // restart system calls if interrupted by handler

  struct sigaction old;
  if (sigaction(STOPSIG, &alarm, &old) == -1)
  {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  schedule_clock.it_interval.tv_sec = 0;
  schedule_clock.it_interval.tv_usec = quota;
  schedule_clock.it_value.tv_sec = 0;
  schedule_clock.it_value.tv_usec = quota;

  if (setitimer(ITIMER_REAL, &schedule_clock, NULL) == -1)
  {
    if (sigaction(STOPSIG, &old, NULL) == -1)
    {
      perror("sigaction");
    }
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

static inline char *get_state_name(state_t s)
{
  static char *names[] = {"blocked", "ready", "terminated"};

  return names[s];
}

void deadlock_graphs(int signo)
{
  block_signals();

  printf("\n#########Threads#########\n");
  for (size_t i = 0; i < thread_count; i++)
  {
    printf("Thread %lu %s\n", threads_list[i].tid,
           get_state_name(threads_list[i].state));
  }
  printf("##################\n");

  display_deadlocks();

  unblock_signals();
}

static int ult_display_handler()
{
  struct sigaction sa;

  sa.sa_handler = deadlock_graphs;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;

  // SIGTSTP is CTRL+Z
  if (sigaction(SIGTSTP, &sa, NULL) == -1)
  {
    perror("sigaction");
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

int ult_init(long quota)
{
  running = init_next_ult(ULT_READY); // register main as an ult
  main_context = &running->context;

  // create the scheduler, setup internal timer for each thread based on quota
  int status = create_scheduler(quota);
  if (EXIT_SUCCESS != status)
  {
    return status;
  }

  status = ult_display_handler();
  if (EXIT_SUCCESS != status)
  {
    return status;
  }

  return EXIT_SUCCESS;
}

static void ult_wrapper()
{
  block_signals();
  ult_t *t = running;
  unblock_signals();

  void *result = t->start_routine(t->arg);
  ult_exit(result);
}

ult_t *create_thread(state_t state, void *(*start_routine)(void *), void *arg)
{
  ult_t *t = init_next_ult(state);

  t->context.uc_stack.ss_sp = malloc(SIGSTKSZ);
  t->context.uc_stack.ss_size = SIGSTKSZ;
  t->context.uc_stack.ss_flags = 0;
  t->context.uc_link = main_context;
  makecontext(&t->context, (void (*)(void))ult_wrapper, 0); // set ult_wrapper function as entrypoint

  t->start_routine = start_routine;
  t->arg = arg;

  return t;
}

int ult_create(tid_t *tid, void *(*start_routine)(void *), void *arg)
{
  if (MAX_THREADS_COUNT - 1 == thread_count)
  {
    return EXIT_FAILURE;
  }

  block_signals();
  *tid = create_thread(ULT_READY, start_routine, arg)->tid;
  unblock_signals();

  return 0;
}

int ult_join(tid_t tid, void **retval)
{
  block_signals();

  ult_t *target = &threads_list[tid];
  ult_t *current = running;

  // check if target thread is terminated
  if (target->state == ULT_TERMINATED)
  {
    if (retval != NULL)
    {
      *retval = target->retval;
    }
    unblock_signals();
    return EXIT_SUCCESS;
  }

  // check if target thread has a joiner already
  if (target->has_joiner)
  {
    unblock_signals();
    errno = EINVAL;
    return EXIT_FAILURE;
  }

  current->state = ULT_BLOCKED;
  current->waiting_for = tid;
  target->has_joiner = true;
  target->joiner = current->tid;

  unblock_signals();

  ult_yield();

  // will resume here after target thread exits (means the thread he is joining terminated)
  block_signals();
  current->state = ULT_READY;
  current->waiting_for = -1;
  target->has_joiner = false;
  target->joiner = -1;

  if (retval != NULL)
  {
    *retval = target->retval;
  }
  unblock_signals();

  return EXIT_SUCCESS;
}

void ult_yield() { raise(STOPSIG); }

void ult_exit(void *retval)
{
  block_signals();

  running->retval = retval;
  running->state = ULT_TERMINATED;

  if (running->has_joiner)
  {
    tid_t joiner = running->joiner;
    if (joiner >= 0 && joiner < thread_count)
    {
      ult_t *joiner_thread = &threads_list[joiner];
      if (joiner_thread->state == ULT_BLOCKED &&
          joiner_thread->waiting_for == running->tid)
      {
        joiner_thread->state = ULT_READY;
      }
    }
  }

  unblock_signals();
  ult_yield();
}

tid_t ult_self() { return running->tid; }

bool ult_is_thread_waiting_for(tid_t waiter, tid_t target)
{
  if (waiter >= thread_count || target >= thread_count)
  {
    return false;
  }
  return threads_list[waiter].waiting_for == target;
}

bool ult_is_thread_terminated(tid_t tid)
{
  if (tid >= thread_count)
  {
    return false;
  }
  return threads_list[tid].state == ULT_TERMINATED;
}

ult_t *get_current_thread()
{
  if (running == NULL)
  {
    return NULL;
  }
  return running;
}

ult_t *get_thread_by_id(tid_t tid)
{
  if (tid >= thread_count)
    return NULL;
  return &threads_list[tid];
}

size_t ult_get_thread_count()
{
  if (thread_count > MAX_THREADS_COUNT)
  {
    return 0;
  }
  return thread_count;
}
