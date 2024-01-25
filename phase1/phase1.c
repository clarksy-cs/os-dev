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
void list_add_node(List *current_proc, proc_ptr new_node);
proc_ptr list_pop_node(List *current_proc);
void clear_proc_entry(int target);

/* -------------------------- Globals ------------------------------------- */

/* debugging global variable... */
int debugflag = 1;

/* the process table */
proc_struct proc_tbl[MAXPROC];

/* process lists */
List ready_procs[6];

/* current process ID */
proc_ptr current = NULL;

/* the next pid to be assigned */
unsigned int new_pid = SENTINELPID;

/* initiates dispatch */
int start_flag = 1;

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
   current = &proc_tbl[0];

   /* initiate dispatch in fork1 */
   start_flag = 0;
  
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

   // TEST CODE ********* POSSIBLE SOLUTION
   if (proc_tbl[1].pid == 2) {
      start_flag = 1;
   }

   if (DEBUG && debugflag)
      console("fork1(): creating process %s\n", name);

   /* test if in kernel mode; halt if in user mode */
   // TODO:

   /* return if stack size is too small */
   if (stacksize < USLOSS_MIN_STACK) {
      return -2;
   }

   /* return if name is too long */
   if (strlen(name) >= (MAXNAME - 1)) {
      console("fork1(): Process name is too long. Halting...\n");
      halt(1);
   }

   /* find an empty slot in the process table */
   for (int i = 0; i < MAXPROC; i++) {
      if (proc_tbl[i].status == STATUS_EMPTY) {
         proc_slot = i;
         break;
      }
      if (i == MAXPROC - 1) {
         /* no empty slots available */
         return -1;
      }
   }

   /* pointer for new process in process table*/
   proc_ptr new_proc = &proc_tbl[proc_slot];

   /* fill-in entry in process table */
   strcpy(proc_tbl[proc_slot].name, name);
   strcpy(new_proc->name, name);
   new_proc->start_func = f;
   new_proc->stacksize = stacksize;
   new_proc->stack = malloc(stacksize);
   new_proc->pid = new_pid;
   new_proc->priority = priority;
   new_proc->status = STATUS_READY;

   new_pid++;

   if ( arg == NULL )
      new_proc->start_arg[0] = '\0';
   else if (strlen(arg) >= (MAXARG - 1)) {
      console("fork1(): Argument too long. Halting...\n");
      halt(1);
   } else {
      strcpy(new_proc->start_arg, arg);
   }

   if (current != NULL && current->pid != 1) {
      list_add_node(&current->children, new_proc);
      new_proc->parent_proc_ptr = current;
      // TEST //
      current->status = STATUS_JOIN_BLOCKED;
   }

   /* Initialize context for this process, but use launch function pointer for
    * the initial value of the process's program counter (PC) */
   context_init(&(new_proc->state), psr_get(), new_proc->stack, new_proc->stacksize, launch);

   /* for future phase(s) */
   p1_fork(new_proc->pid);

   /* add to ready list */
   add_ready_proc(new_proc);

   if (!start_flag) {
      dispatcher();
   }

   return new_proc->pid;

}  /* fork1 */

void list_add_node(List *current_proc, proc_ptr new_node) {

   if (current_proc->p_head == NULL) {
      /* list is empty */
      current_proc->p_head = new_node;
      current_proc->p_tail = new_node;
   } else if (current_proc->p_head->next_proc_ptr == NULL) {
      /* list has only 1 node -- add to end */
      current_proc->p_head->next_proc_ptr = new_node;
      new_node->prev_proc_ptr = current_proc->p_head;
      current_proc->p_tail = new_node;
   } else {
      /* list has more than 1 node -- add to end */
      current_proc->p_tail->next_proc_ptr = new_node;
      new_node->prev_proc_ptr = current_proc->p_tail;
      current_proc->p_tail = new_node;
   }

   current_proc->count++;
}

proc_ptr list_pop_node(List *current_proc) {

   proc_ptr rm_node = current_proc->p_head;

   if (current_proc->p_head == current_proc->p_tail) {
      current_proc->p_head = NULL;
      current_proc->p_tail = NULL;
   } else {
      current_proc->p_head = current_proc->p_head->next_proc_ptr;
      current_proc->p_head->prev_proc_ptr = NULL;
   }

   current_proc->count--;

   return rm_node;
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

   // ******WORK IN PROGRESS *************
   // TODO: figure out how to get start1 to loop here until
   //       all children quit

   // check if any child process has already quit
   proc_ptr child = current->quitting_children.p_head;
   int child_pid = 0;

   while (child != NULL) {
      if (child->status == -3) {
         // child has quit, set termination code
         *code = child->status;

         // remove child from the children list
         child = list_pop_node(&current->quitting_children);
         child_pid = child->pid;
         clear_proc_entry(child_pid);
         return child_pid;
      }
      child = child->next_proc_ptr;
   }

   // no child process has quit, block the parent
   current->status = STATUS_JOIN_BLOCKED;
   dispatcher();


   return -2;

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

   if (code == 0) {
      exit(0);
   }
   
   current->status = STATUS_QUIT;

   /* find parent process */
   if (current->parent_proc_ptr != NULL) {
      /*remove from children list and add to quit children list*/
      proc_ptr child = list_pop_node(&current->parent_proc_ptr->children);
      child->status = code;
      list_add_node(&current->parent_proc_ptr->quitting_children, child);
   }

   if (current->parent_proc_ptr != NULL && current->parent_proc_ptr->status == STATUS_JOIN_BLOCKED) {
      add_ready_proc(current->parent_proc_ptr);
   }

   dispatcher();
   p1_quit(current->pid);
   enableInterrupts();

} /* quit */

proc_ptr get_ready_proc(void) {

   for (int i = 0; i <= MINPRIORITY; i++) {
   
      if (ready_procs[i].count > 0) {
         /* if list is not empty */
         proc_ptr ready_proc = list_pop_node(&ready_procs[i]);
         return ready_proc;
      }
   }
   return NULL;
}

/* adds entry to ready list */
void add_ready_proc(proc_ptr new_ready_proc) {

   int priority = (new_ready_proc->priority - 1);
   new_ready_proc->status = STATUS_READY;
   list_add_node(&ready_procs[priority], new_ready_proc);
}

void clear_proc_entry(int target) {
   proc_tbl[target].next_proc_ptr = NULL;
   proc_tbl[target].prev_proc_ptr = NULL;
   proc_tbl[target].next_sibling_ptr = NULL;
   proc_tbl[target].prev_sibling_ptr = NULL;
   proc_tbl[target].parent_proc_ptr = NULL;
   proc_tbl[target].pid = 0;
   proc_tbl[target].priority = 0;
   proc_tbl[target].start_func = NULL;
   proc_tbl[target].stack = NULL;
   proc_tbl[target].stacksize = 0;
   proc_tbl[target].status = STATUS_EMPTY;
   proc_tbl[target].children.p_head = NULL;
   proc_tbl[target].children.p_tail = NULL;
   proc_tbl[target].children.count = 0;
   proc_tbl[target].children.offset = 0;

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
   context *prev_context_ptr;

   if (current->pid == 1) {
      prev_context_ptr = NULL;
   } else {
      prev_context_ptr = &current->state;
   }

   /* find the next process to run */
   next_process = get_ready_proc();

   if (next_process != NULL) {

      p1_switch(current->pid, next_process->pid);

      /* set current ptr to new process */
      current = next_process;
      current->status = STATUS_RUNNING;

      /* swap old process with new process */
      context_switch(prev_context_ptr, &current->state);
   }

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