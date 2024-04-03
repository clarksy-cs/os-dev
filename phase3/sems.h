#define CHECKMODE          0

#define MBSTATUS_EMPTY     0
#define MBSTATUS_RUNNING   1

#define BLOCKED_SEND       10
#define BLOCKED_RCV        11
#define BLOCKED_RL         12

#define STATUS_FREE        0
#define STATUS_USED        1

typedef struct userproc userproc;
typedef struct userproc *uproc_ptr;

typedef struct semaphore semaphore;
typedef struct semaphore *sem_ptr;

typedef struct node {
   void     *next;
   void     *prev;
} node;

typedef struct list {
   void     *head;
   void     *tail;
   int      count;
} list;

struct semaphore {
   int      status;
   int      id;
   int      value;
};

struct userproc {
   uproc_ptr next;
   uproc_ptr prev;
   int       pid;
   int       parentPID;
   int       (*entrypoint)(char *);
   char      name[MAXNAME];
   int       startup_mbox;
   int       private_mbox;
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