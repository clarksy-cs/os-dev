#define DEBUG4 0

typedef struct driver_proc * driver_proc_ptr;
typedef struct disk_request * disk_request_ptr;

struct driver_proc {
   driver_proc_ptr next_ptr;
   driver_proc_ptr prev_ptr;

   /* for sleep syscall */
   int   pid;
   int   wake_time;
   int   been_zapped;
   int   sleep_mbox;


   /* used for disk requests */
   int   operation;    /* DISK_READ, DISK_WRITE, DISK_SEEK, DISK_TRACKS */
   int   track_start;
   int   sector_start;
   int   num_sectors;
   void *disk_buf;
};

struct disk_request {

};

extern int debugflag4;

