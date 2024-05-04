
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
List disk_requests[2];

/* global variables */
static int next_proc_slot = 0;
static int diskpids[DISK_UNITS];
static int num_tracks[DISK_UNITS];
static int disk_sems[DISK_UNITS];
static int running; /*semaphore to synchronize drivers and start3*/

/* prototypes */
static int	ClockDriver(char *);
static int	DiskDriver(char *);
static void sleep_sys(sysargs  *args_ptr);
static void disk_size(sysargs  *args_ptr);
static void disk_read(sysargs  *args_ptr);
static void disk_write(sysargs *args_ptr);
void list_add_snode(List *list, void *list_node);
void list_add_node(List *list, void *list_node);
void *list_pop_node(List *list);
void clear_entry(int target);
int  get_next_proc_slot(void);

int start3(char *arg) {

    char name[128], termbuf[10], buf[32];
    int	 i, clockPID, pid, status, slot;
    proc_ptr newproc, target;

    /* assign system call handlers */
    sys_vec[SYS_SLEEP]     = sleep_sys;
    sys_vec[SYS_DISKSIZE]  = disk_size;
    sys_vec[SYS_DISKREAD]  = disk_read;
    sys_vec[SYS_DISKWRITE] = disk_write;


    /* initialize the phase 4 process table */
    for (int i = 0; i < MAXPROC; i++) {
        proc_tbl[i].pid = -1;
        proc_tbl[i].sleep_sem = semcreate_real(0);
        proc_tbl[i].disk_sem  = semcreate_real(0);
        proc_tbl[i].wake_time = -1;
        proc_tbl[i].been_zapped = FALSE;
        proc_tbl[i].status = STATUS_FREE;
        proc_tbl[i].request.operation = -1;
        proc_tbl[i].request.disk_buffer  = NULL;
        proc_tbl[i].request.num_sectors  = -1;
        proc_tbl[i].request.track_start  = -1;
        proc_tbl[i].request.sector_start = -1;
        proc_tbl[i].request.current_sector = 0;
        proc_tbl[i].request.sectors_read = 0;
        proc_tbl[i].request.current_track = 0;
        proc_tbl[i].request.sector_count = 0;

    }

    /* initialize disk semaphores */
    for (int i = 0; i < DISK_UNITS; i++) {
        disk_sems[i] = semcreate_real(0);
    }

    /* create clock device driver and semaphore for syncronization */
    running = semcreate_real(0);
    clockPID = fork1("Clock driver", ClockDriver, NULL, USLOSS_MIN_STACK, 2);

    /* error handling */
    if (clockPID < 0) {
        console("start3(): Can't create clock driver\n");
        halt(1);
    }

    /* get slot in proc table */
    next_proc_slot = get_next_proc_slot();

    /* if table is full return -1 */
    if (next_proc_slot == -1) {
        return -1;
    }

    /* store driver process in proc table */
    slot = next_proc_slot % MAXPROC;
    newproc = &proc_tbl[slot];

    /* set up process */
    newproc->pid = clockPID;
    newproc->status = STATUS_USED;

    /* wait for the clock driver to start */
    semp_real(running);

    /* create disk drivers and initialize disk drivers */
    for (i = 0; i < DISK_UNITS; i++) {
        sprintf(buf, "%d", i);
        sprintf(name, "DiskDriver%d", i);
        diskpids[i] = fork1(name, DiskDriver, buf, USLOSS_MIN_STACK, 2);

        /* error handling */
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

        /* set up process */
        newproc->pid = diskpids[i];
        newproc->status = STATUS_USED;
    }

    /* wait on disk drivers */
    semp_real(running);
    semp_real(running);


    /* create first user-level process and wait for it to finish */
    pid = spawn_real("start4", start4, NULL,  8 * USLOSS_MIN_STACK, 3);
    pid = wait_real(&status);

    /* zap the device drivers */
    zap(clockPID);          // clock driver
    clear_entry(clockPID);  // clear process table entry
    join(&status);          // join with children

    /* release sems to terminate disk drivers */
    for (i = 0; i < DISK_UNITS; i++) {
        semfree_real(disk_sems[i]);
        join(&status); 
    }

    return(0);
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
        curtime = sys_clock();                  // get current system time
        waking_proc = sleepingprocs.head;       // get pointer to head of sleeping list
        while (waking_proc != NULL) {
            /* wake up processes accordingly */
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

    int i, track_count, result, status, unit, slot, current_track;
    device_request my_request; // current device request
    proc_ptr current_req;      // current requester process

    unit = atoi(arg);

    if (DEBUG4 && debugflag4) {
        console("DiskDriver(%d): started\n", unit);
    }

    /* get the number of tracks for this disk */
    my_request.opr  = DISK_TRACKS;
    my_request.reg1 = &track_count;

    result = device_output(DISK_DEV, unit, &my_request);

    if (result != DEV_OK) {
        console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
        console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
        halt(1);
    }

    result = waitdevice(DISK_DEV, unit, &status);

    if (result != 0) {
        return 0;
    }

    /* set number of tracks */
    num_tracks[unit] = track_count;

    if (DEBUG4 && debugflag4)
        console("DiskDriver(%d): tracks = %d\n", unit, num_tracks[unit]);

    /* signal start3 that we are running */
    semv_real(running);

    while(!is_zapped()) {

        /* block on sem while waiting for request */
        semp_real(disk_sems[unit]);

        if (disk_requests[unit].count > 0) {
            /* get next request */
            current_req = list_pop_node(&disk_requests[unit]);

            /* init loop and track counter */
            i = 0;
            current_track = 0;

            /* read all sectors in a request */
            while(current_req->request.sectors_read != current_req->request.num_sectors) {

                /* check arm position and adjust */
                if (current_track != current_req->request.track_start) {
                    my_request.opr  = DISK_SEEK;
                    my_request.reg1 = current_track = current_req->request.track_start;

                    result = device_output(DISK_DEV, unit, &my_request);

                    if (result != DEV_OK) {
                        console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
                        console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
                        halt(1);
                    }

                    result = waitdevice(DISK_DEV, unit, &status);

                    if (result != 0) {
                        return 0;
                    }
                }

                /* read or write to sector */
                my_request.opr  = current_req->request.operation;           // set operation type
                my_request.reg1 = current_req->request.sector_start + i;    // set sector start
                my_request.reg2 = current_req->request.disk_buffer +        // set buffer
                current_req->request.sectors_read * DISK_SECTOR_SIZE;

                result = device_output(DISK_DEV, unit, &my_request);

                if (result != DEV_OK) {
                    console("DiskDriver %d: did not get DEV_OK on DISK_TRACKS call\n", unit);
                    console("DiskDriver %d: is the file disk%d present???\n", unit, unit);
                    halt(1);
                }

                result = waitdevice(DISK_DEV, unit, &status);

                if (result != 0) {
                    return 0;
                }

                /* keep track of sectors read with loop counter*/
                i++;
                current_req->request.sectors_read++;

                /* if we reach end of track sector */
                if (current_req->request.sector_start >= 15) {
                    current_track = ++current_req->request.track_start; // inc track start
                    current_req->request.sector_start = 0;              // reset sector start
                    i = 0;                                              // reset loop counter
                }
            }

            /* wake up requester process */
            semv_real(current_req->disk_sem);
        }
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

    /* set up process */
    callingproc->pid = getpid();
    callingproc->status = STATUS_USED;
    callingproc->wake_time = (sleeptime * 1000000) + sys_clock(); // add sleeptime to current time

    /* add process to sleepinglist */
    list_add_snode(&sleepingprocs, callingproc);

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
    int unit, slot;
    proc_ptr callingproc;

    unit = (int)args_ptr->arg5;

    /* check if unit is out of range */
    if (unit < 0 || unit >= DISK_UNITS) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    /* check if track start is out of range */
    if ((int)args_ptr->arg3 < 0 || (int)args_ptr->arg3 > DISK_TRACK_SIZE) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    /* check if sector start is out of range */
    if ((int)args_ptr->arg4 < 0 || (int)args_ptr->arg4 > DISK_TRACK_SIZE) {
        args_ptr->arg4 = (int)-1;
        return;
    }

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

    /* set up process */
    callingproc->pid = getpid();
    callingproc->status = STATUS_USED;                          // set status
    callingproc->request.operation = (int)DISK_READ;            // operation type
    callingproc->request.disk_buffer  = args_ptr->arg1;         // pointer to buffer
    callingproc->request.num_sectors  = (int)args_ptr->arg2;    // number of sectors to read
    callingproc->request.track_start  = (int)args_ptr->arg3;    // track start point
    callingproc->request.sector_start = (int)args_ptr->arg4;    // sector start point

    /* add to disk request queue */
    list_add_node(&disk_requests[unit], callingproc);

    /* wake up disk driver */
    semv_real(disk_sems[unit]);

    /* block on private sem */
    semp_real(callingproc->disk_sem);

    /* return values */
    args_ptr->arg1 = (int)0;
    args_ptr->arg4 = (int)0;
}

/* disk write system call */
static void disk_write(sysargs *args_ptr) {
    int unit, slot;
    proc_ptr callingproc;

    unit = (int)args_ptr->arg5;

    /* check if unit is out of range */
    if (unit < 0 || unit >= DISK_UNITS) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    /* check if track start is out of range */
    if ((int)args_ptr->arg3 < 0 || (int)args_ptr->arg3 > DISK_TRACK_SIZE) {
        args_ptr->arg4 = (int)-1;
        return;
    }

    /* check if sector start is out of range */
    if ((int)args_ptr->arg4 < 0 || (int)args_ptr->arg4 > DISK_TRACK_SIZE) {
        args_ptr->arg4 = (int)-1;
        return;
    }

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

    /* set up process */
    callingproc->pid = getpid();
    callingproc->status = STATUS_USED;                          // set status
    callingproc->request.operation = (int)DISK_WRITE;           // operation type
    callingproc->request.disk_buffer  = args_ptr->arg1;         // pointer to buffer
    callingproc->request.num_sectors  = (int)args_ptr->arg2;    // number of sectors to write
    callingproc->request.track_start  = (int)args_ptr->arg3;    // track start point
    callingproc->request.sector_start = (int)args_ptr->arg4;    // sector start point

    /* add to disk request queue */
    list_add_node(&disk_requests[unit], callingproc);

    /* wake up disk driver */
    semv_real(disk_sems[unit]);

    /* block on private sem */
    semp_real(callingproc->disk_sem);

    /* return values */
    args_ptr->arg1 = (int)0;
    args_ptr->arg4 = (int)0;
}

void list_add_snode(List *list, void *list_node) {

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

} /* list_add_snode */

void list_add_node(List *list, void *list_node) {

   node *new_node = (node *)list_node;

   if (list->head == NULL) {
      /* list is empty */
      list->head = new_node;
      list->tail = new_node;
   } else if (((node *)list->head)->next == NULL) {
      /* list has only 1 node - add to end */
      ((node *)list->head)->next = new_node;
      new_node->prev = list->head;
      list->tail = new_node;
   } else {
      /* list has more than 1 node - add to end */
      ((node *)list->tail)->next = new_node;
      new_node->prev = list->tail;
      list->tail = new_node;
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

    /* reset all parameters */
    for (int i = 0; i < MAXPROC; i++) {
        if (proc_tbl[i].pid == target) {
            proc_tbl[i].pid = -1;
            proc_tbl[i].status = STATUS_FREE;
            proc_tbl[i].wake_time = -1;
            proc_tbl[i].been_zapped = FALSE;
            proc_tbl[i].request.operation = -1;
            proc_tbl[i].request.disk_buffer  = NULL;
            proc_tbl[i].request.num_sectors  = -1;
            proc_tbl[i].request.track_start  = -1;
            proc_tbl[i].request.sector_start = -1;
            proc_tbl[i].request.current_sector = 0;
            proc_tbl[i].request.sectors_read = 0;
            proc_tbl[i].request.current_track = 0;
            proc_tbl[i].request.sector_count = 0;
            //semfree_real(proc_tbl[i].sleep_sem);
            //semfree_real(proc_tbl[i].disk_sem);
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