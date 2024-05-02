
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

int start3(char *arg) {

    char name[128], termbuf[10], buf[32];
    int	 i, clockPID, pid, status;
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

    /*
     * Zap the device drivers
     */
    zap(clockPID);  // clock driver
    join(&status); /* for the Clock Driver */
}

static int ClockDriver(char *arg) {

    int result, status, curtime;
    proc_ptr waking_proc;

    /*
     * Let the parent know we are running and enable interrupts.
     */
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
                waking_proc = list_pop_node(&sleepingprocs);
                semv_real(waking_proc->sleep_sem);
                waking_proc = waking_proc->next_ptr;
            } else {
                /* break when wake_time exceeds current time */
                waking_proc = NULL;
            }
        }
    }
}

static int DiskDriver(char *arg) {

    int track_count, result, status, unit;
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

    //while(1) {
    //    semp_real(disk_sems[unit]);
    //}

    return 0;
}

/* sleep system call */
static void sleep_sys(sysargs *args_ptr) {
    int sleeptime;

    sleeptime = (int)args_ptr->arg1;
    
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


}

/* disk write system call */
static void disk_write(sysargs *args_ptr) {


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