/* ------------------------------------------------------------------------
   phase3.c
   Applied Technology
   College of Applied Science and Technology
   The University of Arizona
   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include "sems.h"

/* ------------------------- Prototypes ----------------------------------- */
int start2(char *arg);
int start3(char *arg);
static void nullsys3(sysargs *args_ptr);

extern int  spawn(char *name, int (*func)(char *), char *arg, int stack_size, int priority, int *pid);
extern int  wait(int *pid, int *status);
extern void terminate(int status);
extern void gettimeofday(int *tod);
extern void cputime(int *cpu);
extern void getPID(int *pid);
extern int  semcreate(int value, int *semaphore);
extern int  semp(int semaphore);
extern int  semv(int semaphore);
extern int  Semfree(int semaphore);

extern int  spawn_real(char *name, int (*func)(char *), char *arg, int stack_size, int priority);
extern int  wait_real(int *status);
extern void terminate_real(int exit_code);
extern int  gettimeofday_real(int *time);
extern int  cputime_real(int *time);
extern int  getPID_real(int *pid);
extern int  semcreate_real(int init_value);
extern int  semp_real(int semaphore);
extern int  semv_real(int semaphore);
extern int  semfree_real(int semaphore);

/* -------------------------- Globals ------------------------------------- */

/* -------------------------- Functions ----------------------------------- */

int start2(char *arg) {

    int pid;
    int	status;
    /*
     * Check kernel mode here.
     */

    /*
     * Data structure initialization as needed...
     */

    for (int i = 0; i < MAXSYSCALLS; i++) {
        sys_vec[i] = nullsys3;
    }

    sys_vec[SYS_SPAWN] = Spawn;


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
    pid = spawn_real("start3", start3, NULL, 4 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

} /* start2 */

int start3(char *arg) {

    int pid;
    int status;

    printf("start3(): started.  Calling Spawn for Child1\n");
    Spawn("Child1", Child1, NULL, USLOSS_MIN_STACK, 5, &pid);
    printf("start3(): fork %d\n", pid);
    Wait(&pid, &status);
    printf("start3(): result of wait, pid = %d, status = %d\n", pid, status);
    printf("start3(): Parent done. Calling Terminate.\n");
    Terminate(8);

    return 0;

} /* start3 */


int spawn(char *name, int (*func)(char *), char *arg, int stack_size, int priority, int *pid) {

    int (*func)(char *);
    char *arg;
    int stack_size;
    sysargs *sa;
    CHECKMODE;
    // more local variables

    sa->number = SYS_SPAWN;
    sa->arg1 = (void *) func;
    sa->arg2 = arg;
    sa->arg3 = (void *) stack_size;
    sa->arg4 = (void *) priority;
    sa->arg5 = name;

    if (is_zapped()) {
        // terminate process
    }

    func = sa->arg1;
    arg  = sa->arg2;
    stack_size = (int)sa->arg3;
    // extract syscall arguments with error handling

    // call another function to mudularize
    kid_pid = spawn_real(name, func, arg, stack_size, priority);
    sa->arg1 = (void *) kid_pid; // packing to return to caller
    sa->arg4 = (void *) 0;

    usyscall(&sa);

    if (is_zapped()) {
        // set to user mode
        // call psr_set to do this
    }

    *pid = (int) sa->arg1;    
    return (int) sa->arg4;

} /* spawn */

static void nullsys3(sysargs *args_ptr) {

   printf("nullsys3(): Invalid syscall %d\n", args_ptr->number);
   printf("nullsys3(): process %d terminating\n", getpid());
   terminate_real(1);

} /* nullsys3 */

semcreate(int value, int *semaphore) {

    sysargs sa;
    CHECKMODE;

    sa.number = SYS_SEMCREATE;
    sa.arg1 = (void *) value;

    usyscall(&sa);

    *semaphore = (int) sa.arg1;
    return (int) sa.arg4;

} /* semcreate */

