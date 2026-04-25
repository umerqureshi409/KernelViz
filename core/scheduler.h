/*
 * scheduler.h — CPU Scheduling algorithms
 * KernelViz OS Simulation Engine
 *
 * CEP R2 (Conflicting Requirements): This module implements both Round Robin
 * and Priority scheduling to demonstrate the fundamental tradeoff:
 * - Round Robin: Fair (equal time slices) but potentially higher turnaround
 * - Priority: Efficient (high-priority first) but risks starvation
 *
 * CEP R1: RR algorithm complexity is O(n) per tick where n = number of
 * ready processes. We scan the ready queue each quantum expiry.
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"

/* Maximum Gantt chart entries to record */
#define MAX_GANTT_ENTRIES 256

/* Scheduling algorithm types */
typedef enum {
    SCHED_ROUND_ROBIN,
    SCHED_PRIORITY
} SchedAlgorithm;

/* Single entry in the Gantt chart timeline */
typedef struct {
    int pid;          /* Which process ran (-1 for idle) */
    int start_time;   /* Start tick of this entry */
    int end_time;     /* End tick of this entry */
} GanttEntry;

/* Scheduler state and configuration */
typedef struct {
    SchedAlgorithm algorithm;          /* Current scheduling algorithm */
    int quantum;                       /* Time quantum for Round Robin */
    int current_quantum_remaining;     /* Ticks left in current quantum */
    int current_pid;                   /* PID of currently running process (-1 if idle) */
    GanttEntry gantt[MAX_GANTT_ENTRIES]; /* Gantt chart history */
    int gantt_count;                   /* Number of Gantt entries */
    float avg_wait_time;               /* Average wait time across completed processes */
    float avg_turnaround_time;         /* Average turnaround time */
    float cpu_utilization;             /* CPU utilization percentage */
    int total_ticks;                   /* Total ticks elapsed */
    int busy_ticks;                    /* Ticks CPU was busy */
    int completed_count;               /* Number of completed processes */
    float total_wait;                  /* Sum of wait times for average calc */
    float total_turnaround;            /* Sum of turnaround times for average calc */
    int rr_index;                      /* Current index for round-robin cycling */
} Scheduler;

/* Initialize scheduler with default settings */
void scheduler_init(Scheduler *sched);

/* Run one tick of Round Robin scheduling */
void schedule_rr(Scheduler *sched, ProcessTable *pt, int current_time);

/* Run one tick of Priority scheduling */
void schedule_priority(Scheduler *sched, ProcessTable *pt, int current_time);

/* Run one tick using the currently selected algorithm */
void scheduler_tick(Scheduler *sched, ProcessTable *pt, int current_time);

/* Set the scheduling algorithm */
void scheduler_set_algorithm(Scheduler *sched, SchedAlgorithm algo);

/* Set the time quantum for Round Robin */
void scheduler_set_quantum(Scheduler *sched, int quantum);

/* Serialize Gantt chart to JSON */
void gantt_to_json(Scheduler *sched, char *buf, int buf_size);

/* Serialize scheduler metrics to JSON */
void scheduler_metrics_to_json(Scheduler *sched, char *buf, int buf_size);

#endif /* SCHEDULER_H */
