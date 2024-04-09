#define CHECKMODE          0

#define MBSTATUS_EMPTY     0
#define MBSTATUS_RUNNING   1

#define BLOCKED_SEND       10
#define BLOCKED_RCV        11
#define BLOCKED_RL         12

#define STATUS_FREE        0
#define STATUS_USED        1
#define STATUS_KILL        2

typedef struct userproc userproc;
typedef struct userproc *uproc_ptr;

typedef struct semaphore semaphore;
typedef struct semaphore *sem_ptr;

typedef struct node {
   void     *next;
   void     *prev;
} node;

typedef struct List {
   void     *head;
   void     *tail;
   int      count;
} List;

struct semaphore {
   int      status;
   int      id;
   int      value;
   List     waitingprocs;
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
   List      children;
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