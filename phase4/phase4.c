
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <usloss.h>
#include <phase1.h>
#include <phase2.h>
#include <phase3.h>
#include <phase4.h>
#include <usyscall.h>
#include <provided_prototypes.h>
#include "driver.h"

/* process table */
static struct driver_proc proc_tbl[MAXPROC];

/* sleeping processes */
List sleepingprocs;

/* global variables */
static int next_proc_slot = 0;
static int diskpids[DISK_UNITS];
static int num_tracks[DISK_UNITS];
static int disk_sems[DISK_UNITS];
static int running; /*semaphore to synchronize drivers and start3*/

/* prototypes */
static int	ClockDriver(char *);
static int	DiskDriver(char *);
static void sleep_sys(sysargs *args_ptr);
static void disk_size(sysargs *args_ptr);
static void disk_read(sysargs *args_ptr);
static void disk_write(sysargs *args_ptr);
void list_add_node(List *list, void *list_node);
void *list_pop_node(List *list);
void clear_entry(int target);
int  get_next_proc_slot(void);

int start3(char *arg) {

    char name[128], termbuf[10], buf[32];
    int	 i, clockPID, pid, status, slot;
    proc_ptr newproc, target;
    /*
     * Check kernel mode here.
     */

    /* Assignment system call handlers */
    sys_vec[SYS_SLEEP]     = sleep_sys;
    sys_vec[SYS_DISKSIZE]  = disk_size;
    sys_vec[SYS_DISKREAD]  = disk_read;
    sys_vec[SYS_DISKWRITE] = disk_write;
    //more for this phase's system call handlings


    /* Initialize the phase 4 process table */
    for (int i = 0; i < MAXPROC; i++) {
        proc_tbl[i].pid = -1;
        proc_tbl[i].sleep_sem = semcreate_real(0);
        proc_tbl[i].wake_time = -1;
        proc_tbl[i].been_zapped = FALSE;
        proc_tbl[i].status = STATUS_FREE;

    }

    /* initialize disk semaphores */
    for (int i = 0; i < DISK_UNITS; i++) {
        disk_sems[i] = semcreate_real(0);
    }

    /*
     * Create clock device driver 
     * I am assuming a semaphore here for coordination.  A mailbox can
     * be used instead -- your choice.
     */
    running = semcreate_real(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);

    if (clockPID < 0) {
        console("start3(): Can't create clock driver\n");
        halt(1);
    }

    /* get slot in proc table */
    next_proc_slot = get_next_proc_slot();

    /* if table is full */
    if (next_proc_slot == -1) {
        return -1;
    }

    /* store driver process in proc table */
    slot = next_proc_slot % MAXPROC;
    newproc = &proc_tbl[slot];

    newproc->pid = clockPID;
    newproc->status = STATUS_USED;

    /*
     * Wait for the clock driver to start. The idea is that ClockDriver
     * will V the semaphore "running" once it is running.
     */

    semp_real(running);

    /*
     * Create the disk device drivers here.  You may need to increase
     * the stack size depending on the complexity of your
     * driver, and perhaps do something with the pid returned.
     */

    for (i = 0; i < DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        diskpids[i] = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);

        if (diskpids[i] < 0) {
            console("start3(): Can't create disk driver %d\n", i);
            halt(1);
        }

        /* get slot in proc table */
        next_proc_slot = get_next_proc_slot();

        /* if table is full */
        if (next_proc_slot == -1) {
            return -1;
        }

        /* store driver process in proc table */
        slot = next_proc_slot % MAXPROC;
        newproc = &proc_tbl[slot];

        newproc->pid = diskpids[i];
        newproc->status = STATUS_USED;
    }
    semp_real(running);
    semp_real(running);


    /*
     * Create first user-level process and wait for it to finish.
     * These are lower-case because they are not system calls;
     * system calls cannot be invoked from kernel mode.
     * I'm assuming kernel-mode versions of the system calls
     * with lower-case names.
     */
    pid = spawn_real("start4", start4, NULL,  8 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

    /* zap the device drivers */
    zap(clockPID);  // clock driver
    clear_entry(clockPID);
    join(&status); 

    for (i = 0; i < MAXPROC; i++) {
        if (proc_tbl[i].pid == diskpids[0]) {
            target = &proc_tbl[i];
            target->been_zapped = TRUE; // indicate zapped for disk driver
            semv_real(disk_sems[0]);
        }
        if (proc_tbl[i].pid == diskpids[1]) {
            target = &proc_tbl[i];
            target->been_zapped = TRUE; // indicate zapped for disk driver
            semv_real(disk_sems[1]);
        }
    }

    /* zap any user-level processes remaining */
    for (i = 0; i < MAXPROC; i++) {
        if (proc_tbl[i].pid == STATUS_USED) {
            zap(proc_tbl[i].pid);
            clear_entry(proc_tbl[i].pid);
        }
    }

    join(&status);
    join(&status);

    terminate_real(0);
}

static int ClockDriver(char *arg) {

    int result, status, curtime;
    proc_ptr waking_proc;

    /* let the parent know we are running and enable interrupts */
    semv_real(running);
    psr_set(psr_get() | PSR_CURRENT_INT);

    while(!is_zapped()) {
	    result = waitdevice(CLOCK_DEV, 0, &status);

	    if (result != 0) {
	        return 0;
	    }

        /* compute the current time and wake up any processes whose time has come */
        curtime = sys_clock();
        waking_proc = sleepingprocs.head;
        while (waking_proc != NULL) {
            /* wake up processes */
            if (waking_proc->wake_time <= curtime) {
                waking_proc = list_pop_node(&sleepingprocs);    // get first ready proc
                semv_real(waking_proc->sleep_sem);              // wake up by inc semaphore
                clear_entry(waking_proc->pid);                  // clear proc table entry
                waking_proc = sleepingprocs.head;               // get next ready proc
            } else {
                /* break when wake_time exceeds current time */
                waking_proc = NULL;
            }
        }
    }
}

static int DiskDriver(char *arg) {

    int track_count, result, status, unit, slot;
    device_request my_request;
    proc_ptr current_req;

    unit = atoi(arg);

    if (DEBUG4 && debugflag4) {
        console("DiskDriver(%d): started\n", unit);
    }

    /* Get the number of tracks for this disk */
    my_request.opr  = DISK_TRACKS;
    my_request.reg1 = &track_count;

    result = device_output(DISK_DEV, unit, &my_request);

    if (result != DEV_OK) {
        console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
        console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
        halt(1);
    }

    waitdevice(DISK_DEV, unit, &status);

    // errorcheck on waitdevice

    num_tracks[unit] = track_count;

    // if (DEBUG4 && debugflag4)
    //     console("DiskDriver(%d): tracks = %d\n", unit, num_tracks[unit]);

    /* signal start3 that we are running */
    semv_real(running);

    while(1) { // TRY USING GETPID HERE AND TERMINATE IF ZAPPED
        int target = diskpids[unit];
        for (int i = 0; i < MAXPROC; i++) {
            current_req = &proc_tbl[i];
            if (current_req->pid == target && current_req->been_zapped) {
                clear_entry(current_req->pid);
                terminate_real(0);
            }
        }
        semp_real(disk_sems[unit]);
    }

    return 0;
}

/* sleep system call */
static void sleep_sys(sysargs *args_ptr) {
    int sleeptime, slot;
    proc_ptr callingproc;

    sleeptime = (int)args_ptr->arg1;

    /* get next proc slot */
    next_proc_slot = get_next_proc_slot();

    /* if table is full */
    if (next_proc_slot == -1) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    /* store driver process in proc table */
    slot = next_proc_slot % MAXPROC;
    callingproc = &proc_tbl[slot];

    callingproc->pid = getpid() % MAXPROC;
    callingproc->status = STATUS_USED;
    callingproc->wake_time = (sleeptime * 1000000) + sys_clock();

    /* add process to sleepinglist */
    list_add_node(&sleepingprocs, callingproc);

    /* wait for semaphore */
    semp_real(callingproc->sleep_sem);

    /* return result */
    args_ptr->arg4 = (int)0;    
}

/* disk size system call */
static void disk_size(sysargs *args_ptr) {

    int unit;

    unit = (int)args_ptr->arg1;
    
    /* check if unit is out of range */
    if (unit < 0 || unit >= DISK_UNITS) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    args_ptr->arg1 = (int)DISK_SECTOR_SIZE;     // sector size
    args_ptr->arg2 = (int)DISK_TRACK_SIZE;      // sectors per track
    args_ptr->arg3 = (int)num_tracks[unit];     // track count
    args_ptr->arg4 = (int)0;                    // status

}

/* disk read system call */
static void disk_read(sysargs *args_ptr) {
    int sectors, track, first, unit;

    sectors = (int)args_ptr->arg2;
    track   = (int)args_ptr->arg3;
    first   = (int)args_ptr->arg4;
    unit    = (int)args_ptr->arg5;

    /* check if unit is out of range */
    if (unit < 0 || unit >= DISK_UNITS) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    args_ptr->arg1 = (int)0;
    args_ptr->arg4 = (int)0;
}

/* disk write system call */
static void disk_write(sysargs *args_ptr) {
    int sectors, track, first, unit;
    char *disk_buffer;

    disk_buffer  = args_ptr->arg1;
    sectors = (int)args_ptr->arg2;
    track   = (int)args_ptr->arg3;
    first   = (int)args_ptr->arg4;
    unit    = (int)args_ptr->arg5;

    /* check if unit is out of range */
    if (unit < 0 || unit >= DISK_UNITS) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    args_ptr->arg1 = (int)0;
    args_ptr->arg4 = (int)0;

}

void list_add_node(List *list, void *list_node) {

    node *new_node = (node *)list_node;

    if (list->head == NULL) {
        /* list is empty */
        list->head = new_node;
        list->tail = new_node;
    } else {
        node *current = list->head;
        node *prev = NULL;

        /* find correct position in list */
        while (current != NULL && ((driver_proc *)new_node)->wake_time > ((driver_proc *)current)->wake_time) {
            prev = current;
            current = current->next;
        }

        /* insert new node at beginning of the list */
        if (prev == NULL) {
            new_node->next = list->head;
            new_node->prev = NULL;
            ((node *)list->head)->prev = new_node;
            list->head = new_node;
        } else if (current == NULL) {
            /* insert new node at end of the list */
            prev->next = new_node;
            new_node->prev = prev;
            new_node->next = NULL;
            list->tail = new_node;
        } else {
            /* insert new node in middle of list */
            prev->next = new_node;
            new_node->prev = prev;
            new_node->next = current;
            current->prev = new_node;
        }
    }

    list->count++;

} /* list_add_node */

void *list_pop_node(List *list) {

    node *rm_node = (node *)list->head;

    if (list->head == list->tail) {
        /* list has only 1 node - restore pointers */
        list->head = NULL;
        list->tail = NULL;
    } else {
        /* list has more than 1 node - adjust pointers */
        list->head = ((node *)list->head)->next;
        ((node *)list->head)->prev = NULL;
    }

    list->count--;

    /* ensure count is not out of bounds */
    if (list->count < 0) {
        list->count = 0;
    }

    return rm_node;

} /* list_pop_node */

void clear_entry(int target) {

    for (int i = 0; i < MAXPROC; i++) {
        if (proc_tbl[i].pid == target) {
            proc_tbl[i].pid = -1;
            proc_tbl[i].status = STATUS_FREE;
            proc_tbl[i].wake_time = -1;
            proc_tbl[i].been_zapped = FALSE;
        }
    }

}


int  get_next_proc_slot(void) {
    int found = -1;

    /* search for a free proc slot */
    for (int i = 0; i < MAXPROC; i++) {
        if (proc_tbl[i].status == STATUS_FREE) {
            found = i;  // if a free slot is found, store the index
            break;      
        }
    }

    if (found != -1) {
        proc_tbl[found].status = STATUS_USED; // mark the found slot as used
        return found; // return the index of the free slot found
    } else {
        return -1; // return -1 if no free slot is available
    }
}