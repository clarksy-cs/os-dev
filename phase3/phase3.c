/* ------------------------------------------------------------------------
   phase3.c
   Applied Technology
   College of Applied Science and Technology
   The University of Arizona
   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <string.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <usyscall.h>
#include <libuser.h>
#include <provided_prototypes.h>
#include "sems.h"

/* ------------------------- Prototypes ----------------------------------- */
int start2(char *arg);
int start3(char *arg);
int get_next_sem_id(void);
static void nullsys3(sysargs *args_ptr);
static void semcreate(sysargs *args_ptr);
static void spawn(sysargs *args_ptr);
static void wait(sysargs *args_ptr);
static void terminate(sysargs *args_ptr);
static void cputime(sysargs *args_ptr);
static void timeofday(sysargs *args_ptr);
static void getPID(sysargs *args_ptr);
static void semp(sysargs *args_ptr);
static void semv(sysargs *args_ptr);
static void semfree(sysargs *args_ptr);
void list_add_node(List *list, void *list_node);
void *list_pop_node(List *list);

/* -------------------------- Globals ------------------------------------- */

/* user process table */
userproc uproc_tbl[MAXPROC];

/* semaphore table */
semaphore sem_tbl[MAXSEMS];

/* global count variables */
int next_sem_id = 0;
int waitingprocs = 0;

/* -------------------------- Functions ----------------------------------- */

int start2(char *arg) {

    int pid;
    int	status;
    int psr;

    /* check kernel mode */
    psr = psr_get();

    if ((psr & PSR_CURRENT_MODE) == 0) {
        console("Kernel mode expected, but function %s called in user mode.\n", arg);
        halt(1);
    }

    /* initialize user process table */
    for (int i = 0; i < MAXPROC; i++) {
        uproc_tbl[i].next = NULL;
        uproc_tbl[i].prev = NULL;
        uproc_tbl[i].entrypoint = NULL;
        uproc_tbl[i].pid = -1;
        uproc_tbl[i].parentPID = -1;
        uproc_tbl[i].startup_mbox = MboxCreate(1, 0);
        uproc_tbl[i].private_mbox = MboxCreate(0, 0);
        strcpy(uproc_tbl[i].name, "");
    }

    /* initialize semaphore table */
    for (int i = 0; i < MAXSEMS; i++) {
        sem_tbl[i].id = -1;
        sem_tbl[i].status = STATUS_FREE;
        sem_tbl[i].value = -1;
    }

    /* intialize system call vector */
    for (int i = 0; i < MAXSYSCALLS; i++) {
        sys_vec[i] = nullsys3;
    }

    sys_vec[SYS_SPAWN] = spawn;
    sys_vec[SYS_WAIT] = wait;
    sys_vec[SYS_TERMINATE] = terminate;
    sys_vec[SYS_SEMCREATE] = semcreate;
    sys_vec[SYS_GETTIMEOFDAY] = timeofday;
    sys_vec[SYS_CPUTIME] = cputime;
    sys_vec[SYS_GETPID] = getPID;
    sys_vec[SYS_SEMP] = semp;
    sys_vec[SYS_SEMV] = semv;
    sys_vec[SYS_SEMFREE] = semfree;

    pid = spawn_real("start3", start3, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

} /* start2 */

static void nullsys3(sysargs *args_ptr) {

   console("nullsys3(): Invalid syscall %d\n", args_ptr->number);
   console("nullsys3(): process %d terminating\n", getpid());
   terminate_real(1);

} /* nullsys3 */

int get_next_sem_id(void) {

    int new_sem_id = -1;
    int next_pos = next_sem_id % MAXSEMS;
    int num_full = 0;

    /* iterate sem table for empty position */
    while ((num_full < MAXSEMS) && sem_tbl[next_pos].status != STATUS_FREE) {
        next_sem_id++;
        next_pos = next_sem_id % MAXSEMS;
        num_full++;
    }

    /* empty position found in sem table */
    if (num_full < MAXSEMS) {
        new_sem_id = next_sem_id++;
        return new_sem_id;
    }

    return new_sem_id;
} /* get_next_sem_id */

static void semcreate(sysargs *args_ptr) {

    int value = (int)args_ptr->arg1;
    int sem_id = semcreate_real(value);
    args_ptr->arg1 = (void *)sem_id;

    if (sem_id == -1) {
        args_ptr->arg4 = (void *)-1;
    } else {
        args_ptr->arg4 = (void *) 0;
    }

} /* semcreate */

int semcreate_real(int init_value) {

   /* return -1 if parameter is out of bounds */
   if (init_value < 0) {
      return -1;
   }

   int sem_pos;

   /* get next sem id */
   next_sem_id = get_next_sem_id();

   /* return -1 if sem table is full */
   if (next_sem_id == -1) {
      return next_sem_id;
   }

   /* set sem position in table */
   sem_pos = next_sem_id % MAXSEMS;

   /* pointer to new sem in sem table */
   sem_ptr new_sem = &sem_tbl[sem_pos];

   /* initialize new sem */
   new_sem->id = next_sem_id;
   new_sem->status  = STATUS_USED;
   new_sem->value = init_value;

   return new_sem->id;

} /* semcreate_real */

int launch_usermode(char *arg) {

    int pid;
    int proc_slot;
    int result;
    int psr;

    pid = getpid();
    proc_slot = pid % MAXPROC;

    MboxReceive(uproc_tbl[proc_slot].startup_mbox, NULL, 0);

    if (is_zapped()) {
        quit(0);
    }

    /* set the user mode */
    psr = psr_get();
    psr &= ~PSR_CURRENT_MODE;
    psr_set(psr);

    /* run the entry point */
    result = uproc_tbl[proc_slot].entrypoint(arg);

    Terminate(result);

    return result;

} /* launch_usermode */

static void spawn(sysargs *args_ptr) {
    
    int (*func)(char *);
    char *arg;
    char *name;
    int stack_size;
    int priority;
    int kidpid;

    func = args_ptr->arg1;
    arg = args_ptr->arg2;
    stack_size = (int)args_ptr->arg3;
    priority = (int)args_ptr->arg4;
    name = args_ptr->arg5;

    kidpid = spawn_real(name, func, arg, stack_size, priority);
    args_ptr->arg1 = (void *)kidpid;
    args_ptr->arg4 = (void *)0;

    return;

} /* spawn */

int spawn_real(char *name, int (*func)(char *), char *arg, int stack_size, int priority) {

    int parent;
    int kidpid;
    int proc_slot;
    uproc_ptr kidptr;

    parent = getpid() % MAXPROC;

    kidpid = fork1(name, launch_usermode, arg, stack_size, priority);

    if (kidpid < 0) {
        return -1;
    }

    proc_slot = kidpid % MAXPROC;
    kidptr = &uproc_tbl[proc_slot];

    kidptr->pid = kidpid;            // get child pid
    kidptr->parentPID = getpid();    // get parent pid
    kidptr->entrypoint = func;       // pass launch_usermode function to call

    list_add_node(&uproc_tbl[parent].children, kidptr);

    /* tell process to start */
    MboxCondSend(uproc_tbl[proc_slot].startup_mbox, NULL, 0);

    return kidpid;

} /* spawn_real */

static void wait(sysargs *args_ptr) {

    int *ptr = (int *)malloc(sizeof(int));
    int pid;
    int status;

    pid = wait_real(ptr);
    status = *ptr;

    args_ptr->arg1 = (void *)pid;
    args_ptr->arg2 = (void *)status;

    if (status < 0) {
        args_ptr->arg4 = (void *)-1;
    }

    args_ptr->arg4 = (void *)0;

    free(ptr);
    
    return;

} /* wait */

int wait_real(int *status) {

    int pid = join(status);
    return pid;

} /* wait_real */

static void terminate(sysargs *args_ptr) {

    int exit_code = (int)args_ptr->arg1;
    terminate_real(exit_code);

} /* terminate */

void terminate_real(int exit_code) {

    int proc_slot = getpid() % MAXPROC;
    uproc_ptr current = &uproc_tbl[proc_slot];
    uproc_ptr child, parent;

    while (current->children.count > 0) {
        child = list_pop_node(&current->children);
        zap(child->pid);
    }

    if (current->parentPID != -1) {
        int parentslot = current->parentPID % MAXPROC;
        parent = &uproc_tbl[parentslot];
        if (parent->children.count > 0) {
            child = list_pop_node(&parent->children);
        }
    }

    quit(exit_code);

} /* terminate_real */

static void cputime(sysargs *args_ptr){ 

    int *ptr = (int *)malloc(sizeof(int));
    int cpu;

    cpu = cputime_real(ptr);
    args_ptr->arg1 = (void *)cpu;

    free(ptr);

} /* cputime */

int cputime_real(int *time) {

    *time = readtime();

} /* cputime_real */


static void timeofday(sysargs *args_ptr) {

    int *ptr = (int *)malloc(sizeof(int));
    int tod;

    tod = gettimeofday_real(ptr);
    args_ptr->arg1 = (void *)tod;

    free(ptr);

} /* timeofday */

int gettimeofday_real(int *time) {

    return sys_clock();

} /* gettimeofday_real */

static void getPID(sysargs *arg_ptr) {

    int *ptr = (int *)malloc(sizeof(int));
    int pid;

    pid = getPID_real(ptr);
    arg_ptr->arg1 = (void *)pid;

    free(ptr);

} /* getPID */

int getPID_real(int *pid) {
    
    int cpid = getpid();

    if (cpid < 0 || cpid >= MAXPROC) {
        return -1;
    }

    return cpid;

} /* getPID_real */

static void semp(sysargs *args_ptr) {

    int result;

    result = semp_real((int)args_ptr->arg1);
    args_ptr->arg4 = (void *)result;

} /* semp */

int semp_real(int semaphore) {

    sem_ptr sem = &sem_tbl[semaphore];

    if (sem->value == 0) {
        int wproc_slot = getpid() % MAXPROC;
        uproc_ptr wproc = &uproc_tbl[wproc_slot];
        list_add_node(&sem->waitingprocs, wproc);
        MboxReceive(wproc->private_mbox, NULL, 0);
    }

    sem->value -= 1;

    return 0;

} /* semp_real */

static void semv(sysargs *args_ptr) {

    int result;

    result = semv_real((int)args_ptr->arg1);
    args_ptr->arg4 = (void *)result;

} /* semv */

int semv_real(int semaphore) {

    sem_ptr sem = &sem_tbl[semaphore];

    sem->value += 1;

    if (sem->waitingprocs.count > 0) {
        uproc_ptr wproc = list_pop_node(&sem->waitingprocs);
        MboxCondSend(wproc->private_mbox, NULL, 0);
    }

    return 0;

} /* semv_real */

static void semfree(sysargs *args_ptr) {

    int result;

    result = semfree_real((int)args_ptr->arg1);
    args_ptr->arg4 = (void *)result;

} /* semfree */

int semfree_real(int semaphore) {

    sem_ptr sem = &sem_tbl[semaphore];

    while (sem->waitingprocs.count > 0) {
        uproc_ptr wproc = list_pop_node(&sem->waitingprocs);
        zap(wproc->pid);
    }

    sem->value = -1;
    sem->id = -1;
    sem->status = STATUS_FREE;

    return 0;

} /* semfree_real */

void list_add_node(List *list, void *list_node) {

   node *new_node = (node *)list_node;

   if (list->head == NULL) {
      /* list is empty */
      list->head = new_node;
      list->tail = new_node;
   } else if (((node *)list->head)->next == NULL) {
      /* list has only 1 node - add to end */
      ((node *)list->head)->next = new_node;
      new_node->prev = list->head;
      list->tail = new_node;
   } else {
      /* list has more than 1 node - add to end */
      ((node *)list->tail)->next = new_node;
      new_node->prev = list->tail;
      list->tail = new_node;
   }

   list->count++;

} /* list_add_node */

void *list_pop_node(List *list) {

   node *rm_node = (node *)list->head;

   if (list->head == list->tail) {
      /* list has only 1 node - restore pointers */
      list->head = NULL;
      list->tail = NULL;
   } else {
      /* list has more than 1 node - adjust pointers */
      list->head = ((node *)list->head)->next;
      ((node *)list->head)->prev = NULL;
   }

   list->count--;

   /* ensure count is not out of bounds */
   if (list->count < 0) {
      list->count = 0;
   }

   return rm_node;

} /* list_pop_node */