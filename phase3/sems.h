#define DEBUG2 1

#define MBSTATUS_EMPTY     0
#define MBSTATUS_RUNNING   1

#define BLOCKED_SEND       10
#define BLOCKED_RCV        11
#define BLOCKED_RL         12

#define STATUS_FREE        0
#define STATUS_USED        1

typedef struct mailslot mailslot;
typedef struct mailslot *slot_ptr;

typedef struct mailbox mailbox;
typedef struct mailbox *mbox_ptr;

typedef struct waitingproc waitingproc;
typedef struct waitingproc *proc_ptr;

typedef struct node {
   void     *next;
   void     *prev;
} node;

typedef struct list {
   void     *head;
   void     *tail;
   int      count;
} list;

struct mailbox {
   int      mbox_id;
   int      status;
   int      slot_size;
   int      slot_count;
   list     slots;
   list     waiting_send;
   list     waiting_rcv;
   list     released_procs;
   int      releasing_pid;
};

struct mailslot {
   slot_ptr next;
   slot_ptr prev;
   int      slot_id;
   int      status;
   int      size;
   char     buffer[MAX_MESSAGE];
};

struct waitingproc {
   proc_ptr next;
   proc_ptr prev;
   int      pid;
   slot_ptr slot;
};

struct psr_bits {
    unsigned int cur_mode:1;
    unsigned int cur_int_enable:1;
    unsigned int prev_mode:1;
    unsigned int prev_int_enable:1;
    unsigned int unused:28;
};

union psr_values {
   struct psr_bits bits;
   unsigned int integer_part;
};