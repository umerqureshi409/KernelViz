/*
 * process.h — Process Control Block (PCB) definitions and lifecycle management
 * KernelViz OS Simulation Engine
 * 
 * CEP R1 (Depth of Knowledge): PCB is the fundamental data structure that
 * the OS kernel maintains for every process. It stores all information
 * needed to manage process execution, scheduling, and resource allocation.
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <time.h>

/* Maximum number of concurrent processes in the simulation */
#define MAX_PROCESSES 50

/* Maximum length of a process name */
#define MAX_NAME_LEN 32

/* Process states — mirrors real OS process lifecycle */
typedef enum {
    STATE_NEW,         /* Process just created, not yet admitted */
    STATE_READY,       /* Admitted, waiting in ready queue for CPU */
    STATE_RUNNING,     /* Currently executing on CPU */
    STATE_WAITING,     /* Blocked on I/O or IPC event */
    STATE_TERMINATED   /* Finished execution, awaiting cleanup */
} ProcessState;

/* Process Control Block — stores all process metadata */
typedef struct {
    int pid;                        /* Unique process identifier */
    char name[MAX_NAME_LEN];        /* Human-readable process name */
    ProcessState state;             /* Current process state */
    int priority;                   /* Priority level (0=highest, 9=lowest) */
    int burst_time;                 /* Total CPU burst time needed */
    int remaining_time;             /* Remaining CPU time to complete */
    int arrival_time;               /* Time unit when process arrived */
    int memory_size;                /* Memory units required */
    int memory_start;               /* Start address in memory (-1 if unallocated) */
    int wait_time;                  /* Total time spent waiting */
    int turnaround_time;            /* Total time from arrival to completion */
    int is_active;                  /* 1 if slot is in use, 0 if free */
} PCB;

/* Process table — global array of all PCBs */
typedef struct {
    PCB processes[MAX_PROCESSES];   /* Array of process control blocks */
    int count;                      /* Number of active processes */
    int next_pid;                   /* Next PID to assign */
} ProcessTable;

/* Initialize the process table */
void process_table_init(ProcessTable *pt);

/* Create a new process and add to process table. Returns PID or -1 on failure */
int create_process(ProcessTable *pt, const char *name, int burst_time, int priority, int memory_size, int arrival_time);

/* Terminate a process by PID. Returns 0 on success, -1 if not found */
int terminate_process(ProcessTable *pt, int pid);

/* Get human-readable string for a process state */
const char* get_process_state_string(ProcessState state);

/* Get a process pointer by PID. Returns NULL if not found */
PCB* get_process_by_pid(ProcessTable *pt, int pid);

/* Serialize process table to JSON string. Caller must provide buffer */
void process_table_to_json(ProcessTable *pt, char *buf, int buf_size);

#endif /* PROCESS_H */
