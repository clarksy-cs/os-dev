#define DEBUG2 1

#define MBSTATUS_EMPTY     0
#define MBSTATUS_RUNNING   1

typedef struct mailslot *slot_ptr;
typedef struct mbox_proc *mbox_proc_ptr;
typedef struct mailbox *mbox_ptr;

typedef struct mailbox {
   int      mbox_id;
   int      status;
   /* other items as needed... */
} mbox;

typedef struct mailslot {
   int      mbox_id;
   int      status;
   /* other items as needed... */
} mslot;

typedef struct {
   slot_ptr p_head;
   slot_ptr p_tail;
   int count;
   int offset;
} List;

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
