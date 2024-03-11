#define DEBUG2 1

#define MBSTATUS_EMPTY     0
#define MBSTATUS_RUNNING   1

typedef struct mailslot *slot_ptr;
typedef struct mailbox *mbox_ptr;
typedef struct waitingproc waitingproc;
typedef struct waitingproc *wproc_ptr;

typedef struct Node {
   struct Node *next;
} Node;

typedef struct List {
   Node     *p_head;
   Node     *p_tail;
   int      count;
} List;

typedef struct mailbox {
   int      mbox_id;
   int      status;
   int      slot_size;
   int      slot_count;
   List     delivered_mail;
   List     waiting_send;
   List     waiting_rcv;
   List     released_procs;
   int      releasing_pid;
} mbox;

typedef struct mailslot {
   int      mbox_id;
   int      status;
   /* other items as needed... */
} mslot;

struct waitingproc {
   wproc_ptr   next_proc_ptr;
   wproc_ptr   prev_proc_ptr;
   int         pid;
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
