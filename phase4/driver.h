#define DEBUG4 0
#define TRUE   1
#define FALSE  0

typedef struct driver_proc driver_proc;
typedef struct driver_proc * proc_ptr;

typedef struct disk_request disk_request;
typedef struct disk_request * req_ptr;

typedef struct node {
   void     *next;
   void     *prev;
} node;

typedef struct List {
   void     *head;
   void     *tail;
   int      count;
} List;

struct driver_proc {
   /* list management */
   proc_ptr next_ptr;
   proc_ptr prev_ptr;

   /* used for disk requests */
   req_ptr request;

   /* for sleep syscall */
   int   pid;
   int   wake_time;
   int   been_zapped;
   int   sleep_sem;
};

struct disk_request {
   int   operation;    /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */
   int   track_start;
   int   sector_start;
   int   num_sectors;
   void *disk_buf;
};

extern int debugflag4;

