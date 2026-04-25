/*
 * scheduler.c — Round Robin & Priority CPU Scheduling
 * KernelViz OS Simulation Engine
 *
 * CEP R2 (Conflicting Requirements):
 *   Fairness (Round Robin) vs Efficiency (Priority) tradeoff.
 *   RR ensures no process starves by giving equal time slices.
 *   Priority scheduling minimizes wait time for high-priority tasks
 *   but can cause starvation of low-priority processes.
 *
 * CEP R1 (Depth of Knowledge):
 *   RR Algorithm Complexity: O(n) per tick — we scan the ready queue
 *   to find the next process. With n=50 max processes this is acceptable.
 *   Context switch overhead is simulated as instant (1 tick boundary).
 */

#include "scheduler.h"
#include <stdio.h>
#include <string.h>

/* Initialize scheduler to default state */
void scheduler_init(Scheduler *sched) {
    memset(sched, 0, sizeof(Scheduler));
    sched->algorithm = SCHED_ROUND_ROBIN;
    sched->quantum = 3;
    sched->current_quantum_remaining = 3;
    sched->current_pid = -1;
    sched->gantt_count = 0;
    sched->rr_index = 0;
}

/* Add an entry to the Gantt chart history */
static void add_gantt_entry(Scheduler *sched, int pid, int time) {
    /* Extend last entry if same process continues */
    if (sched->gantt_count > 0 && sched->gantt[sched->gantt_count - 1].pid == pid) {
        sched->gantt[sched->gantt_count - 1].end_time = time + 1;
        return;
    }
    if (sched->gantt_count >= MAX_GANTT_ENTRIES) return;
    GanttEntry *e = &sched->gantt[sched->gantt_count++];
    e->pid = pid;
    e->start_time = time;
    e->end_time = time + 1;
}

/* Transition NEW processes to READY if they've arrived */
static void admit_new_processes(ProcessTable *pt, int current_time) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pt->processes[i].is_active && pt->processes[i].state == STATE_NEW &&
            pt->processes[i].arrival_time <= current_time) {
            pt->processes[i].state = STATE_READY;
        }
    }
}

/* Update wait times for all READY processes (they waited 1 tick) */
static void update_wait_times(ProcessTable *pt) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pt->processes[i].is_active && pt->processes[i].state == STATE_READY) {
            pt->processes[i].wait_time++;
        }
    }
}

/* Handle process completion: update stats and mark terminated */
static void complete_process(Scheduler *sched, PCB *p, int current_time) {
    p->state = STATE_TERMINATED;
    p->turnaround_time = current_time - p->arrival_time + 1;
    sched->completed_count++;
    sched->total_wait += p->wait_time;
    sched->total_turnaround += p->turnaround_time;
    if (sched->completed_count > 0) {
        sched->avg_wait_time = sched->total_wait / sched->completed_count;
        sched->avg_turnaround_time = sched->total_turnaround / sched->completed_count;
    }
    sched->current_pid = -1;
    sched->current_quantum_remaining = sched->quantum;
}

/* Round Robin scheduling — one tick. O(n) per tick */
void schedule_rr(Scheduler *sched, ProcessTable *pt, int current_time) {
    admit_new_processes(pt, current_time);
    update_wait_times(pt);
    sched->total_ticks++;

    /* If a process is currently running */
    if (sched->current_pid != -1) {
        PCB *running = get_process_by_pid(pt, sched->current_pid);
        if (running && running->is_active && running->state == STATE_RUNNING) {
            running->remaining_time--;
            sched->current_quantum_remaining--;

            /* Process completed its burst */
            if (running->remaining_time <= 0) {
                complete_process(sched, running, current_time);
            }
            /* Quantum expired — preempt and move to READY */
            else if (sched->current_quantum_remaining <= 0) {
                running->state = STATE_READY;
                sched->current_pid = -1;
                sched->current_quantum_remaining = sched->quantum;
            } else {
                /* Still running, record in Gantt */
                add_gantt_entry(sched, running->pid, current_time);
                sched->busy_ticks++;
                sched->cpu_utilization = (float)sched->busy_ticks / sched->total_ticks * 100.0f;
                return;
            }
        } else {
            sched->current_pid = -1;
            sched->current_quantum_remaining = sched->quantum;
        }
    }

    /* Pick next READY process using round-robin order */
    int start = sched->rr_index;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        int idx = (start + i) % MAX_PROCESSES;
        if (pt->processes[idx].is_active && pt->processes[idx].state == STATE_READY) {
            pt->processes[idx].state = STATE_RUNNING;
            sched->current_pid = pt->processes[idx].pid;
            sched->current_quantum_remaining = sched->quantum;
            pt->processes[idx].remaining_time--;

            if (pt->processes[idx].remaining_time <= 0) {
                complete_process(sched, &pt->processes[idx], current_time);
            } else {
                add_gantt_entry(sched, pt->processes[idx].pid, current_time);
                sched->busy_ticks++;
            }
            sched->rr_index = (idx + 1) % MAX_PROCESSES;
            sched->cpu_utilization = (float)sched->busy_ticks / sched->total_ticks * 100.0f;
            return;
        }
    }

    /* No process ready — CPU idle */
    add_gantt_entry(sched, -1, current_time);
    sched->cpu_utilization = (float)sched->busy_ticks / sched->total_ticks * 100.0f;
}

/* Priority scheduling — one tick. Higher priority (lower number) runs first */
void schedule_priority(Scheduler *sched, ProcessTable *pt, int current_time) {
    admit_new_processes(pt, current_time);
    update_wait_times(pt);
    sched->total_ticks++;

    /* If a process is currently running, let it continue (non-preemptive for burst) */
    if (sched->current_pid != -1) {
        PCB *running = get_process_by_pid(pt, sched->current_pid);
        if (running && running->is_active && running->state == STATE_RUNNING) {
            running->remaining_time--;
            if (running->remaining_time <= 0) {
                complete_process(sched, running, current_time);
            } else {
                /* Check if higher priority process exists (preemptive) */
                int highest_prio = running->priority;
                for (int i = 0; i < MAX_PROCESSES; i++) {
                    if (pt->processes[i].is_active && pt->processes[i].state == STATE_READY &&
                        pt->processes[i].priority < highest_prio) {
                        highest_prio = pt->processes[i].priority;
                    }
                }
                if (highest_prio < running->priority) {
                    /* Preempt current process */
                    running->state = STATE_READY;
                    sched->current_pid = -1;
                } else {
                    add_gantt_entry(sched, running->pid, current_time);
                    sched->busy_ticks++;
                    sched->cpu_utilization = (float)sched->busy_ticks / sched->total_ticks * 100.0f;
                    return;
                }
            }
        } else {
            sched->current_pid = -1;
        }
    }

    /* Find highest priority (lowest number) READY process */
    int best_idx = -1;
    int best_priority = 999;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pt->processes[i].is_active && pt->processes[i].state == STATE_READY) {
            if (pt->processes[i].priority < best_priority) {
                best_priority = pt->processes[i].priority;
                best_idx = i;
            }
        }
    }

    if (best_idx != -1) {
        pt->processes[best_idx].state = STATE_RUNNING;
        sched->current_pid = pt->processes[best_idx].pid;
        pt->processes[best_idx].remaining_time--;

        if (pt->processes[best_idx].remaining_time <= 0) {
            complete_process(sched, &pt->processes[best_idx], current_time);
        } else {
            add_gantt_entry(sched, pt->processes[best_idx].pid, current_time);
            sched->busy_ticks++;
        }
    } else {
        add_gantt_entry(sched, -1, current_time);
    }
    sched->cpu_utilization = (float)sched->busy_ticks / sched->total_ticks * 100.0f;
}

/* Dispatch to the currently selected algorithm */
void scheduler_tick(Scheduler *sched, ProcessTable *pt, int current_time) {
    if (sched->algorithm == SCHED_ROUND_ROBIN)
        schedule_rr(sched, pt, current_time);
    else
        schedule_priority(sched, pt, current_time);
}

/* Set scheduling algorithm and reset quantum state */
void scheduler_set_algorithm(Scheduler *sched, SchedAlgorithm algo) {
    sched->algorithm = algo;
    sched->current_quantum_remaining = sched->quantum;
}

/* Set RR quantum value */
void scheduler_set_quantum(Scheduler *sched, int quantum) {
    if (quantum < 1) quantum = 1;
    if (quantum > 20) quantum = 20;
    sched->quantum = quantum;
}

/* Serialize Gantt chart to JSON array */
void gantt_to_json(Scheduler *sched, char *buf, int buf_size) {
    int offset = 0;
    /* Only send last 64 entries to keep payload small */
    int start = (sched->gantt_count > 64) ? sched->gantt_count - 64 : 0;
    offset += snprintf(buf + offset, buf_size - offset, "[");
    for (int i = start; i < sched->gantt_count; i++) {
        if (i > start) offset += snprintf(buf + offset, buf_size - offset, ",");
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"pid\":%d,\"start\":%d,\"end\":%d}",
            sched->gantt[i].pid, sched->gantt[i].start_time, sched->gantt[i].end_time);
        if (offset >= buf_size - 2) break;
    }
    snprintf(buf + offset, buf_size - offset, "]");
}

/* Serialize scheduler metrics to JSON */
void scheduler_metrics_to_json(Scheduler *sched, char *buf, int buf_size) {
    snprintf(buf, buf_size,
        "{\"algorithm\":\"%s\",\"quantum\":%d,\"avg_wait_time\":%.2f,"
        "\"avg_turnaround_time\":%.2f,\"cpu_utilization\":%.2f,"
        "\"total_ticks\":%d,\"completed\":%d,\"current_pid\":%d}",
        sched->algorithm == SCHED_ROUND_ROBIN ? "Round Robin" : "Priority",
        sched->quantum, sched->avg_wait_time, sched->avg_turnaround_time,
        sched->cpu_utilization, sched->total_ticks, sched->completed_count,
        sched->current_pid);
}
