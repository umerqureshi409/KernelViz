/*
 * process.c — Process lifecycle management implementation
 * KernelViz OS Simulation Engine
 *
 * CEP R1: Demonstrates deep understanding of OS process management.
 * Each process goes through: NEW → READY → RUNNING → TERMINATED lifecycle.
 * Wait and turnaround times are tracked for performance analysis.
 */

#include "process.h"
#include <string.h>
#include <stdio.h>

/* Initialize the process table to empty state */
void process_table_init(ProcessTable *pt) {
    memset(pt, 0, sizeof(ProcessTable));
    pt->count = 0;
    pt->next_pid = 1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        pt->processes[i].is_active = 0;
        pt->processes[i].memory_start = -1;
    }
}

/* Create a new process with given parameters. Returns PID or -1 on failure */
int create_process(ProcessTable *pt, const char *name, int burst_time, int priority, int memory_size, int arrival_time) {
    if (pt->count >= MAX_PROCESSES) return -1;

    /* Find first free slot in process table */
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!pt->processes[i].is_active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return -1;

    PCB *p = &pt->processes[slot];
    p->pid = pt->next_pid++;
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->name[MAX_NAME_LEN - 1] = '\0';
    p->state = STATE_NEW;
    p->priority = (priority < 0) ? 0 : (priority > 9 ? 9 : priority);
    p->burst_time = burst_time;
    p->remaining_time = burst_time;
    p->arrival_time = arrival_time;
    p->memory_size = (memory_size > 0) ? memory_size : 1;
    p->memory_start = -1;
    p->wait_time = 0;
    p->turnaround_time = 0;
    p->is_active = 1;
    pt->count++;

    return p->pid;
}

/* Terminate a process by PID, freeing its slot */
int terminate_process(ProcessTable *pt, int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pt->processes[i].is_active && pt->processes[i].pid == pid) {
            pt->processes[i].state = STATE_TERMINATED;
            pt->processes[i].is_active = 0;
            pt->count--;
            return 0;
        }
    }
    return -1; /* Process not found */
}

/* Convert process state enum to human-readable string */
const char* get_process_state_string(ProcessState state) {
    switch (state) {
        case STATE_NEW:        return "NEW";
        case STATE_READY:      return "READY";
        case STATE_RUNNING:    return "RUNNING";
        case STATE_WAITING:    return "WAITING";
        case STATE_TERMINATED: return "TERMINATED";
        default:               return "UNKNOWN";
    }
}

/* Look up a process by PID. Returns pointer or NULL */
PCB* get_process_by_pid(ProcessTable *pt, int pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (pt->processes[i].is_active && pt->processes[i].pid == pid) {
            return &pt->processes[i];
        }
    }
    return NULL;
}

/* Serialize entire process table to JSON format */
void process_table_to_json(ProcessTable *pt, char *buf, int buf_size) {
    int offset = 0;
    offset += snprintf(buf + offset, buf_size - offset, "[");

    int first = 1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!pt->processes[i].is_active) continue;
        PCB *p = &pt->processes[i];
        if (!first) offset += snprintf(buf + offset, buf_size - offset, ",");
        first = 0;
        offset += snprintf(buf + offset, buf_size - offset,
            "{\"pid\":%d,\"name\":\"%s\",\"state\":\"%s\",\"priority\":%d,"
            "\"burst_time\":%d,\"remaining_time\":%d,\"arrival_time\":%d,"
            "\"memory_size\":%d,\"memory_start\":%d,\"wait_time\":%d,"
            "\"turnaround_time\":%d}",
            p->pid, p->name, get_process_state_string(p->state), p->priority,
            p->burst_time, p->remaining_time, p->arrival_time,
            p->memory_size, p->memory_start, p->wait_time, p->turnaround_time);
        if (offset >= buf_size - 1) break;
    }
    snprintf(buf + offset, buf_size - offset, "]");
}
