/*
* Create first user-level process and wait for it to finish.
* These are lower-case because they are not system calls;
* system calls cannot be invoked from kernel mode.
* Assumes kernel-mode versions of the system calls
* with lower-case names.  I.e., Spawn is the user-mode function
* called by the test cases; spawn is the kernel-mode function that
* is called by the syscall_handler; spawn_real is the function that
* contains the implementation and is called by spawn.
*
* Spawn() is in libuser.c.  It invokes usyscall()
* The system call handler calls a function named spawn() -- note lower
* case -- that extracts the arguments from the sysargs pointer, and
* checks them for possible errors.  This function then calls spawn_real().
*
* Here, we only call spawn_real(), since we are already in kernel mode.
*
* spawn_real() will create the process by using a call to fork1 to
* create a process executing the code in spawn_launch().  spawn_real()
* and spawn_launch() then coordinate the completion of the phase 3
* process table entries needed for the new process.  spawn_real() will
* return to the original caller of Spawn, while spawn_launch() will
* begin executing the function passed to Spawn. spawn_launch() will
* need to switch to user-mode before allowing user code to execute.
* spawn_real() will return to spawn(), which will put the return
* values back into the sysargs pointer, switch to user-mode, and 
* return to the user code that called Spawn.
*/

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

/* -------------------------- Globals ------------------------------------- */

/* user process table */
userproc uproc_tbl[MAXPROC];

/* semaphore table */
semaphore sem_tbl[MAXSEMS];

/* global count variables */
static int next_sem_id = 0;

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

   /* set mailbox position in table */
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
    // more variables

    if (is_zapped) {
        // terminate process
    }

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

    int pid;
    int proc_slot;

    pid = fork1(name, launch_usermode, arg, stack_size, priority);

    if (pid < 0) {
        console("PID invalid.\n");
        halt(1);
    }

    proc_slot = pid % MAXPROC;

    uproc_tbl[proc_slot].pid = pid;             // get child pid
    uproc_tbl[proc_slot].parentPID = getpid();  // get parent pid
    uproc_tbl[proc_slot].entrypoint = func;     // pass launch_usermode function to call

    /* tell process to start */
    MboxCondSend(uproc_tbl[proc_slot].startup_mbox, NULL, 0);

    return pid;

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

    quit(exit_code);

} /* terminate_real */

static void cputime(sysargs *args_ptr){ 


} /* cputime */

int cputime_real(int *time) {


} /* cputime_real */


static void timeofday(sysargs *args_ptr) {


} /* timeofday */

int gettimeofday_real(int *time) {


} /* gettimeofday_real */