/* ------------------------------------------------------------------------
   phase2.c
   Applied Technology
   College of Applied Science and Technology
   The University of Arizona
   CSCV 452

   ------------------------------------------------------------------------ */
#include <stdlib.h>
#include <phase1.h>
#include <phase2.h>
#include <usloss.h>
#include "message.h"

/* ------------------------- Prototypes ----------------------------------- */
int start1 (char *);
extern int start2 (char *);
int  GetNextEmptyMailbox(void);
static void nullsys(sysargs *args);
void clock_handler2(int dev, void *arg);
void disk_handler(int dev, void *unit_ptr);
void term_handler(int dev, void *unit_ptr);
void syscall_handler(int dev, void *unit_ptr);
int  waitdevice(int type, int unit, int *status);
int  send_message(int mbox_id, void *msg_ptr, int msg_size, int wait); 
int  receive_message(int mbox_id, void *msg_ptr, int msg_size, int wait);
/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* the mailboxes */
mbox  mbox_tbl[MAXMBOX];
mslot mbox_slots[MAXSLOTS];

static List unused_slots;

static int clock_mbox;
static int disk_mbox[2];
static int term_mbox[4];
static int next_mbox_id = 0;

/* -------------------------- Functions -----------------------------------
  Below I have code provided to you that calls

  check_kernel_mode
  enableInterrupts
  disableInterupts
  
  These functions need to be redefined in this phase 2,because
  their phase 1 definitions are static 
  and are not supposed to be used outside of phase 1.  */

/* ------------------------------------------------------------------------
   Name - start1
   Purpose - Initializes mailboxes and interrupt vector.
             Start the phase2 test process.
   Parameters - one, default arg passed by fork1, not used here.
   Returns - one to indicate normal quit.
   Side Effects - lots since it initializes the phase2 data structures.
   ----------------------------------------------------------------------- */
int start1(char *arg) {

   int kid_pid, status; 

   if (DEBUG2 && debugflag2)
      console("start1(): at beginning\n");

   // check_kernel_mode("start1");

   // disableInterrupts();

   /* intialize interrupt handlers */
   // int_vec[CLOCK_DEV] = clock_handler2;
   // int_vec[SYSCALL_INT] = syscall_handler;

   /* system call array of function pointers */
   void (*sys_vec[MAXSYSCALLS])(sysargs *args);

   for (int i = 0; i < MAXSYSCALLS; i++) {
      sys_vec[i] = nullsys;
   }

   /* initialize mailbox table */
   for (int i = 0; i < MAXMBOX; i++) {
      mbox_tbl[i].mbox_id = -1;
      mbox_tbl[i].status  = MBSTATUS_EMPTY;
   }

   /* allocate mailboxes for interrupt handlers */
   clock_mbox = MboxCreate(0, sizeof(int));

   for (int i = 0; i < 2; i++) {
      disk_mbox[i] = MboxCreate(0, sizeof(int));
   }

   for (int i = 0; i < 4; i ++) {
      term_mbox[i] = MboxCreate(0, sizeof(int));
   }

   /* ensure 0 slot mailboxes are created successfully */
   if ( clock_mbox <= -1 || 
      disk_mbox[0] <= -1 || 
      disk_mbox[1] <= -1 ||
      term_mbox[0] <= -1 ||
      term_mbox[1] <= -1 ||
      term_mbox[2] <= -1 ||
      term_mbox[3] <= -1  ) 
   {
      console("start1: Error creating mailbox\n");
      halt(1);
   }
   
   //enableInterrupts();

   /* Create a process for start2, then block on a join until start2 quits */
   if (DEBUG2 && debugflag2)
      console("start1(): fork'ing start2 process\n");

   kid_pid = fork1("start2", start2, NULL, 4 * USLOSS_MIN_STACK, 1);
   
   if ( join(&status) != kid_pid ) {
      console("start2(): join returned something other than start2's pid\n");
   }

   return 0;
} /* start1 */


/* ------------------------------------------------------------------------
   Name - MboxCreate
   Purpose - gets a free mailbox from the table of mailboxes and initializes it 
   Parameters - maximum number of slots in the mailbox and the max size of a msg
                sent to the mailbox.
   Returns - -1 to indicate that no mailbox was created, or a value >= 0 as the
             mailbox id.
   Side Effects - initializes one element of the mail box array. 
   ----------------------------------------------------------------------- */
int MboxCreate(int slots, int slot_size) {

   int mbox_pos;

   /* get next mbox id */
   next_mbox_id = GetNextEmptyMailbox();

   /* return -1 if mbox table is full */
   if (next_mbox_id == -1) {
      return next_mbox_id;
   }

   /* set mailbox position in table */
   mbox_pos = next_mbox_id % MAXMBOX;

   /* pointer to new mailbox in mailbox table */
   mbox_ptr new_mbox = &mbox_tbl[mbox_pos];

   /* initialize new mailbox */
   new_mbox->mbox_id = next_mbox_id;
   new_mbox->status  = MBSTATUS_RUNNING;

   return new_mbox->mbox_id;

} /* MboxCreate */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size) {

   // check_kernel_mode("waitdevice");
   return send_message(mbox_id, msg_ptr, msg_size, 1);

} /* MboxSend */

/* ------------------------------------------------------------------------
   Name - MboxCondSend
   Purpose - Conditionally send a message to a mailbox. 
             Do not block the invoking process.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size) {

   // check_kernel_mode("waitdevice");
   return send_message(mbox_id, msg_ptr, msg_size, 0);

} /* MboxCondSend */

/* ------------------------------------------------------------------------
   Name - send_message
   Purpose - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int  send_message(int mbox_id, void *msg_ptr, int msg_size, int wait) {

   // code

} /* send_message */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_max_size) {

   // check_kernel_mode("MboxReceive");
   return receive_message(mbox_id, msg_ptr, msg_max_size, 1);

} /* MboxReceive */

/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Conditionally receive a message from a mailbox. 
             Do not block the invoking process in cases where there are no messages to receive.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_max_size) {

   // check_kernel_mode("MboxCondReceive");
   return receive_message(mbox_id, msg_ptr, msg_max_size, 0);

} /* MboxCondReceive */


/* ------------------------------------------------------------------------
   Name - receive_message
   Purpose - 
   Returns - 
   Side Effects - 
   ----------------------------------------------------------------------- */
int receive_message(int mbox_id, void *msg_ptr, int msg_size, int wait) {

   // code

} /* rcv_message */


/* ------------------------------------------------------------------------
   Name - MboxRelease
   Purpose - Releases a previously created mailbox. 
             Any process waiting on the mailbox should be zapped.
   Parameters - mailbox id
   Returns - 0 if successful, -1 if not a mailbox that is in use.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxRelease(int mbox_id) {


} /* MboxRelease */


/* ------------------------------------------------------------------------
   Name - GetNextEmptyMailbox
   Purpose - Returns next empty mailbox id
   Parameters - none
   Returns - New mbox_id if successful, -1 if mailbox table is full.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int GetNextEmptyMailbox(void) {

   int new_mbox_id = -1;
   int next_pos = next_mbox_id % MAXMBOX;
   int num_full = 0;

   while ((num_full < MAXMBOX) && mbox_tbl[next_pos].status != MBSTATUS_EMPTY) {
      next_mbox_id++;
      next_pos = next_mbox_id % MAXMBOX;
      num_full++;
   }

   if (num_full < MAXMBOX) {
      new_mbox_id = next_mbox_id++;
      return new_mbox_id;
   }
   
   return new_mbox_id;

} /* GetNextEmptyMailbox */


/* ------------------------------------------------------------------------
   Name - waitdevice
   Purpose - This routine will be used to synchronize with a device driver process in phase 4.
   Parameters - device type, unit, status
   Returns - Returns the device’s status register in *status, -1 if zapped while waiting.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int waitdevice(int type, int unit, int *status) {

   int result = 0;

   /* sanity check */
   // see phase2 description

   switch (type) {
      case CLOCK_DEV:
         result = MboxReceive(clock_mbox, status, sizeof(int));
         break;
      case DISK_DEV:
         result = MboxReceive(disk_mbox[unit], status, sizeof(int));
         break;
      case TERM_DEV:
         result = MboxReceive(term_mbox[unit], status, sizeof(int));
         break;
      default:
         console("waitdevice(); bad type (%d). Halting...\n", type);
         halt(1);
      }
   
   if (result == -3) {
      /* we were zapped */
      return -1;
   }

   return 0;

} /* waitdevice */


void clock_handler2(int dev, void *arg) {

   time_slice();
   // add correct code

} /* clock_handler2 */

void disk_handler(int dev, void *unit_ptr) {

   int status;
   int result;
   int unit = (int)unit_ptr;

   /* sanity checks */
   if (unit < 0 || unit > 1) {
      // checks which disk is sending data
      // see phase 2 description for return values
   }
   // make sure the arguments dev and unit are valid
   // more checking if necessary

   device_input(DISK_DEV, unit, &status);

   result = MboxCondSend(disk_mbox[unit], &status, sizeof(status));
   // do some error checking on the returned result value

} /* disk_handler */

void term_handler(int dev, void *unit_ptr) {
      
   int status;
   int result;
   int unit = (int)unit_ptr;

   /* sanity checks */
   if (unit < 0 || unit > TERM_UNITS) {
      // see phase 2 description for return values
   }
   // make sure the arguments dev and unit are valid
   // more checking if necessary

   device_input(TERM_DEV, unit, &status);

   result = MboxCondSend(term_mbox[unit], &status, sizeof(status));
   // do some error checking on the returned result value

} /* term_handler */

// void syscall_handler(int dev, void *unit_ptr) {
// 
//    sysargs *sys_ptr;
//    sys_ptr = (sysargs *) unit_ptr;
// 
//    /* sanity check */
//    if (dev != SYSCALL_INT) {
//       console("syscall_handler(): device out of range. Halting...\n");
//       halt(1);
//    }
// 
//    if (sys_ptr->number < 0 || sys_ptr->number > MAXSYSCALLS) {
//       console("syscall_handler(): system call number out of range. Halting...\n");
//       halt(1);
//    }
// 
//    /* call appropriate system call handler */
//    sys_vec[sys_ptr->number](sys_ptr);
// 
// } /* syscall_handler */

static void nullsys(sysargs *args) {

   console("nullsys(): Invalid syscall %d. Halting...\n", args->number);
   halt(1);
} /* nullsys */

int check_io() {
   /*
   if (wait_list->count > 0) {
      return 1;
   }
   */
   return 0; 
}