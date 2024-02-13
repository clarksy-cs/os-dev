#define DEBUG 0

typedef struct proc_struct proc_struct;

typedef struct proc_struct * proc_ptr;

#define STATUS_EMPTY          0
#define STATUS_RUNNING        1
#define STATUS_READY          2
#define STATUS_QUIT           3
#define STATUS_JOIN_BLOCKED   4
#define STATUS_ZAP_BLOCKED    5
#define STATUS_LAST           6

#define TRUE                  1
#define FALSE                 0

typedef struct {
   proc_ptr p_head;
   proc_ptr p_tail;
   int count;
   int offset;
} List;

struct proc_struct {
   proc_ptr       next_proc_ptr; 
   proc_ptr       prev_proc_ptr; 
   proc_ptr       next_sibling_ptr; 
   proc_ptr       prev_sibling_ptr;

   proc_ptr       parent_proc_ptr;

   List           children;
   List           quitting_children;
   List           zappers;

   char           name[MAXNAME];     /* process's name */
   char           start_arg[MAXARG]; /* args passed to process */
   context        state;             /* current context for process */
   short          pid;               /* process id */
   int            priority;
   int (* start_func) (char *);      /* function where process begins -- launch */
   char          *stack;
   unsigned int   stacksize;
   int            status;            /* READY, BLOCKED, QUIT, etc. */
   int            exit_code;
   int            start_time;
   int            runtime;
   int            init_time;
   int            zap_flag;
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

/* Some useful constants.  Add more as needed... */
#define NO_CURRENT_PROCESS NULL
#define MINPRIORITY 5
#define MAXPRIORITY 1
#define SENTINELPID 1
#define SENTINELPRIORITY LOWEST_PRIORITY
#define MAXTIME 80000


