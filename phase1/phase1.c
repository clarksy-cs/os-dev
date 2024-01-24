/* ------------------------------------------------------------------------
   phase1.c

   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <phase1.h>
#include "kernel.h"

/* ------------------------- Prototypes ----------------------------------- */
int sentinel (char *);
extern int start1 (char *);
void dispatcher(void);
void launch();
static void enableInterrupts();
static void check_deadlock();
void add_ready_proc(proc_struct *next_ready_proc);
proc_ptr get_ready_proc();
proc_ptr pop_proc();
void list_add_node(List current_proc, proc_ptr new_node);

/* -------------------------- Globals ------------------------------------- */

/* debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct proc_tbl[MAXPROC];

/* process lists */
List ready_procs[6];

/* current process ID */
proc_ptr current;

/* the next pid to be assigned */
unsigned int new_pid = SENTINELPID;

/* initiates dispatch */
int starting = 1;

/* ready list */
proc_ptr ready_list[6];


/* -------------------------- Functions ----------------------------------- */
/* ------------------------------------------------------------------------
   Name - startup
   Purpose - Initializes process lists and clock interrupt vector.
	          Start up sentinel process and the test process.
   Parameters - none, called by USLOSS
   Returns - nothing
   Side Effects - lots, starts the whole thing
   ----------------------------------------------------------------------- */
void startup() {

   int result; /* value returned by call to fork1() */

   /* initialize the process table */
   for (int i = 0; i < MAXPROC; i++) {
      proc_tbl[i].next_proc_ptr = NULL;
      proc_tbl[i].prev_proc_ptr = NULL;
      proc_tbl[i].next_sibling_ptr = NULL;
      proc_tbl[i].prev_sibling_ptr = NULL;
      proc_tbl[i].parent_proc_ptr = NULL;
      proc_tbl[i].pid = 0;
      proc_tbl[i].priority = 0;
      proc_tbl[i].start_func = NULL;
      proc_tbl[i].stack = NULL;
      proc_tbl[i].stacksize = 0;
      proc_tbl[i].status = STATUS_EMPTY;

      proc_tbl[i].children.p_head = NULL;
      proc_tbl[i].children.p_tail = NULL;
      proc_tbl[i].children.count = 0;
      proc_tbl[i].children.offset = 0;
   }

   /* initialize ready process list*/
   for (int i = 0; i <= MINPRIORITY; i++) {
      ready_procs[i].p_head = NULL;
      ready_procs[i].p_tail = NULL;
      ready_procs[i].count = 0;
      ready_procs[i].offset = 0;
   }

   if (DEBUG && debugflag)
      console("startup(): initializing the Ready & Blocked lists\n");

   /* Initialize the clock interrupt handler */

   /* startup a sentinel process */
   if (DEBUG && debugflag)
       console("startup(): calling fork1() for sentinel\n");

   result = fork1("sentinel", sentinel, NULL, USLOSS_MIN_STACK, SENTINELPRIORITY);
   
   if (result < 0) {
      if (DEBUG && debugflag)
         console("startup(): fork1 of sentinel returned error, halting...\n");
      halt(1);
   }

   /* initialize current pointer to sentinel */
   /* set current's next process pointer to following position in process table*/
   current = &proc_tbl[0];
   current->next_proc_ptr = &proc_tbl[1];

   /* initiate dispatch in fork1 */
   starting = 0;
  
   /* start the test process */
   if (DEBUG && debugflag)
      console("startup(): calling fork1() for start1\n");

   result = fork1("start1", start1, NULL, 2 * USLOSS_MIN_STACK, 1);
   
   if (result < 0) {
      console("startup(): fork1 for start1 returned an error, halting...\n");
      halt(1);
   }

   console("startup(): Should not see this message! ");
   console("Returned from fork1 call that created start1\n");

   return;
} /* startup */

/* ------------------------------------------------------------------------
   Name - finish
   Purpose - Required by USLOSS
   Parameters - none
   Returns - nothing
   Side Effects - none
   ----------------------------------------------------------------------- */
void finish() {

   if (DEBUG && debugflag)
      console("in finish...\n");
} /* finish */

/* ------------------------------------------------------------------------
   Name - fork1
   Purpose - Gets a new process from the process table and initializes
             information of the process.  Updates information in the
             parent process to reflect this child process creation.
   Parameters - the process procedure address, the size of the stack and
                the priority to be assigned to the child process.
   Returns - the process id of the created child or -1 if no child could
             be created or if priority is not between max and min priority.
   Side Effects - ReadyList is changed, proc_tbl is changed, current
                  process information changed
   ------------------------------------------------------------------------ */
int fork1(char *name, int (*f)(char *), char *arg, int stacksize, int priority) {

   int proc_slot;

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */

   /* return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK) {
      console("fork1(): Stack size too small. Halting...\n");
      halt(1);
   }

   /* find an empty slot in the process table */
   for (int i = 0; i < MAXPROC; i++) {
      if (proc_tbl[i].status == STATUS_EMPTY) {
         proc_slot = i;
         break;
      }
      if (i == MAXPROC - 1) {
         console("fork1(): Exceeded max processes. Halting...\n");
         halt(1);
      }
   }

   /* return if name is too long */
   if (strlen(name) >= (MAXNAME - 1)) {
      console("fork1(): Process name is too long. Halting...\n");
      halt(1);
   }

   /* fill-in entry in process table */
   strcpy(proc_tbl[proc_slot].name, name);
   proc_tbl[proc_slot].start_func = f;
   proc_tbl[proc_slot].stacksize = stacksize;
   proc_tbl[proc_slot].stack = malloc(stacksize);
   proc_tbl[proc_slot].pid = new_pid;
   proc_tbl[proc_slot].priority = priority;
   proc_tbl[proc_slot].status = STATUS_READY;

   new_pid++;

   if ( arg == NULL )
      proc_tbl[proc_slot].start_arg[0] = '\0';
   else if (strlen(arg) >= (MAXARG - 1)) {
      console("fork1(): Argument too long. Halting...\n");
      halt(1);
   } else {
      strcpy(proc_tbl[proc_slot].start_arg, arg);
   }

   if (current != NULL) {
      list_add_node(current->children, &proc_tbl[proc_slot]);
      proc_tbl[proc_slot].parent_proc_ptr = current;
   }

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC) */
   context_init(&(proc_tbl[proc_slot].state), psr_get(),
                proc_tbl[proc_slot].stack, 
                proc_tbl[proc_slot].stacksize, launch);

   /* for future phase(s) */
   p1_fork(proc_tbl[proc_slot].pid);

   /* add to ready list */
   add_ready_proc(&proc_tbl[proc_slot]);

   // TODO:
   if (!starting) {
      dispatcher();
   }


}  /* fork1 */

void list_add_node(List current_proc, proc_ptr new_node) {

   if (current_proc.p_head == NULL) {
      /* list is empty */
      current_proc.p_head = new_node;
      current_proc.p_tail = new_node;
   } else if (current_proc.p_head->next_proc_ptr == NULL) {
      /* list has only 1 node -- add to end */
      current_proc.p_head->next_proc_ptr = new_node;
      new_node->prev_proc_ptr = current_proc.p_head;
      current_proc.p_tail = new_node;
   } else {
      /* list has more than 1 node -- add to end */
      current_proc.p_tail->next_proc_ptr = new_node;
      new_node->prev_proc_ptr = current_proc.p_tail;
      current_proc.p_tail = new_node;
   }

   current_proc.count++;
}

/* ------------------------------------------------------------------------
   Name - launch
   Purpose - Dummy function to enable interrupts and launch a given process
             upon startup.
   Parameters - none
   Returns - nothing
   Side Effects - enable interrupts
   ------------------------------------------------------------------------ */
void launch()
{
   int result;

   if (DEBUG && debugflag)
      console("launch(): started\n");

   /* Enable interrupts */
   enableInterrupts();

   /* Call the function passed to fork1, and capture its return value */
   result = current->start_func(current->start_arg);

   if (DEBUG && debugflag)
      console("Process %d returned to launch\n", current->pid);

   quit(result);

} /* launch */


/* ------------------------------------------------------------------------
   Name - join
   Purpose - Wait for a child process (if one has been forked) to quit.  If 
             one has already quit, don't wait.
   Parameters - a pointer to an int where the termination code of the 
                quitting process is to be stored.
   Returns - the process id of the quitting child joined on.
		-1 if the process was zapped in the join
		-2 if the process has no children
   Side Effects - If no child process has quit before join is called, the 
                  parent is removed from the ready list and blocked.
   ------------------------------------------------------------------------ */
int join(int *code) {

} /* join */


/* ------------------------------------------------------------------------
   Name - quit
   Purpose - Stops the child process and notifies the parent of the death by
             putting child quit info on the parents child completion code
             list.
   Parameters - the code to return to the grieving parent
   Returns - nothing
   Side Effects - changes the parent of pid child completion status list.
   ------------------------------------------------------------------------ */
void quit(int code) {

   p1_quit(current->pid);

   // TODO:
   // update proc table

} /* quit */

proc_ptr get_ready_proc(void) {
   // TODO:
   for (int i = 0; i <= MINPRIORITY; i++) {
   // if list is not empty
      if (ready_procs[i].count > 0) {
         proc_ptr ready_proc = pop_proc(i);
         return ready_proc;
      }
   }
   return NULL;
}

proc_ptr pop_proc(int priority) {
   // TODO:
   // get next proc and adjust linked list pointers
   proc_ptr proc_to_pop = ready_procs[priority].p_head;

   if (ready_procs[priority].p_head == ready_procs[priority].p_tail) {
      ready_procs[priority].p_head = NULL;
      ready_procs[priority].p_tail = NULL;
   } else {
      ready_procs[priority].p_head = ready_procs[priority].p_head->next_proc_ptr;
      ready_procs[priority].p_head->prev_proc_ptr = NULL;
   }

   ready_procs[priority].count--;

   return proc_to_pop;
}

void add_ready_proc(proc_ptr new_ready_proc) {
   // TODO:
   // add entry to ready list
   int priority = (new_ready_proc->priority - 1);

   if (ready_procs[priority].p_head == NULL) {
      /* list is empty */
      ready_procs[priority].p_head = new_ready_proc;
      ready_procs[priority].p_tail = new_ready_proc;
   } else if (ready_procs[priority].p_head->next_proc_ptr == NULL) {
      /* list has only 1 node -- add to end */
      ready_procs[priority].p_head->next_proc_ptr = new_ready_proc;
      new_ready_proc->prev_proc_ptr = ready_procs[priority].p_head;
      ready_procs[priority].p_tail = new_ready_proc;
   } else {
      /* list has more than 1 node -- add to end */
      ready_procs[priority].p_tail->next_proc_ptr = new_ready_proc;
      new_ready_proc->prev_proc_ptr = ready_procs[priority].p_tail;
      ready_procs[priority].p_tail = new_ready_proc;
   }

   ready_procs[priority].count++;
}

/* ------------------------------------------------------------------------
   Name - dispatcher
   Purpose - dispatches ready processes.  The process with the highest
             priority (the first on the ready list) is scheduled to
             run.  The old process is swapped out and the new process
             swapped in.
   Parameters - none
   Returns - nothing
   Side Effects - the context of the machine is changed
   ----------------------------------------------------------------------- */
void dispatcher(void) {

   proc_ptr next_process;

   // HARD CODE -- CHANGE THIS
   next_process->pid = (current->pid + 1);

   context *prev_context_ptr = &current->state;

   p1_switch(current->pid, next_process->pid);

   // find the next process to run
   next_process = get_ready_proc();

   // set current ptr to new process
   current = next_process;

   // swap old process with new process
   context_switch(prev_context_ptr, &current->state);

} /* dispatcher */


/* ------------------------------------------------------------------------
   Name - sentinel
   Purpose - The purpose of the sentinel routine is two-fold.  One
             responsibility is to keep the system going when all other
	     processes are blocked.  The other is to detect and report
	     simple deadlock states.
   Parameters - none
   Returns - nothing
   Side Effects -  if system is in deadlock, print appropriate error
		   and halt.
   ----------------------------------------------------------------------- */
int sentinel (char *dummy) {

   if (DEBUG && debugflag)
      console("sentinel(): called\n");
   while (1)
   {
      check_deadlock();
      waitint();
   }
} /* sentinel */


/* check to determine if deadlock has occurred... */
static void check_deadlock() {


} /* check_deadlock */


/* Disables the interrupts. */
void disableInterrupts() {

  /* turn the interrupts OFF iff we are in kernel mode */
  if((PSR_CURRENT_MODE & psr_get()) == 0) {
    //not in kernel mode
    console("Kernel Error: Not in kernel mode, may not disable interrupts\n");
    halt(1);
  } else
    /* We ARE in kernel mode */
    psr_set( psr_get() & ~PSR_CURRENT_INT );
} /* disableInterrupts */

static void enableInterrupts() {
   // TODO: write code for this function
}