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


/* -------------------------- Globals ------------------------------------- */

int debugflag2 = 0;

/* the mail boxes */
mail_box MailBoxTable[MAXMBOX];
int clockMbox;
int diskMbox[2];  // Initalize diskMBox
int termMBox[4];  // Initalize diskMBox


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
int start1(char *arg)
{
   int kid_pid, status; 

   if (DEBUG2 && debugflag2)
      console("start1(): at beginning\n");

   check_kernel_mode("start1");

   /* Disable interrupts */
   disableInterrupts();

   /* Initialize the mail box table, slots, & other data structures.
    * Initialize int_vec and sys_vec, allocate mailboxes for interrupt
    * handlers.  Etc... */

   /*Allocate mailboxes use use*/
   clockMbox = MBoxCreate(0, sizeof(int));
   diskMbox[0] = MBoxCreate(0, sizeof(int));
   diskMbox[1] = MBoxCreate(0, sizeof(int));
   termMBox[0] = MBoxCreate(0, sizeof(int));
   termMBox[1] = MBoxCreate(0, sizeof(int));
   termMBox[2] = MBoxCreate(0, sizeof(int));
   termMBox[3] = MBoxCreate(0, sizeof(int));
   if ( clockMbox   <= -1 ||
        diskMbox[0] <= -1 ||
        diskMbox[1] <= -1 ||
        diskMbox[0] <= -1 ||
        diskMbox[1] <= -1 ||
        diskMbox[2] <= -1 ||
        diskMbox[3] <= -1  )
   {
      console("start1(): Error creating mailboxes\n");
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
int MboxCreate(int slots, int slot_size)
{
} /* MboxCreate */

int MboxCondSend(int mbox_id, void *msg_ptr, int msg_size)
{
   check_kernel_mode("waitdevice");
   return SendMessage(mbox_id, msg_ptr, msg_size, 0);
} /* MboxCondSend */


/* ------------------------------------------------------------------------
   Name - MboxSend
   Purpose - Put a message into a slot for the indicated mailbox.
             Block the sending process if no slot available.
   Parameters - mailbox id, pointer to data of msg, # of bytes in msg.
   Returns - zero if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxSend(int mbox_id, void *msg_ptr, int msg_size)
{
   check_kernel_mode("waitdevice");
   return SendMessage(mbox_id, msg_ptr, msg_size, 1);
} /* MboxSend */


int MboxCondReceive(int mbox_id, void *msg_ptr, int msg_size)
{
   check_kernel_mode("waitdevice");
   return SendMessage(mbox_id, msg_ptr, msg_size, 0);
} /* MboxReceive */


/* ------------------------------------------------------------------------
   Name - MboxReceive
   Purpose - Get a msg from a slot of the indicated mailbox.
             Block the receiving process if no msg available.
   Parameters - mailbox id, pointer to put data of msg, max # of bytes that
                can be received.
   Returns - actual size of msg if successful, -1 if invalid args.
   Side Effects - none.
   ----------------------------------------------------------------------- */
int MboxReceive(int mbox_id, void *msg_ptr, int msg_size)
{
   check_kernel_mode("waitdevice");
   return SendMessage(mbox_id, msg_ptr, msg_size, 1);
} /* MboxReceive */


int check_io(){
    // ordered based on order of mailbox creation
    //Implement mailbox to use these variables
    for (int i = clockMbox; i<= termMBox[3]; i ++){
      //if (MailBoxTable[i].waitingForDelivery.coint > 0) {
         //return 1;
      }
    //}
//}
//return 0;

int MBoxRelease(int mailboxID) {
   
}

int waitdevice (int type, int unit, int *status) {

}