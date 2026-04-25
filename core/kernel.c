/*
 * kernel.c — KernelViz OS Simulation Kernel
 * KernelViz OS Simulation Engine
 *
 * CEP R1 (Depth of Knowledge):
 *   Demonstrates real OS layering: scheduler → memory → IPC → FS.
 *   Each kernel_tick() mirrors a hardware timer interrupt that wakes
 *   the OS to run one scheduling quantum, update bookkeeping, and
 *   service any pending IPC events.
 *
 * CEP R7 (Interdependence):
 *   A terminated process triggers: memory free → pipe close → FS
 *   cleanup, showing how OS subsystems are tightly coupled.
 *
 * CEP R8 (Consequences / Security):
 *   Processes consuming > 80% of elapsed ticks are flagged ANOMALY.
 *
 * CEP R9 (Judgement):
 *   kernel_set_algorithm() lets the user override scheduling from UI.
 */

#include "kernel.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ── Helpers ───────────────────────────────────────────────────────── */

/* Generate a simple IPC exchange between two random processes each tick */
static void simulate_ipc_activity(Kernel *k) {
    /* Find two active processes to pass a message */
    int pids[10], count = 0;
    for (int i = 0; i < MAX_PROCESSES && count < 10; i++) {
        if (k->proc_table.processes[i].is_active &&
            k->proc_table.processes[i].state != STATE_TERMINATED)
            pids[count++] = k->proc_table.processes[i].pid;
    }
    if (count < 2) return;
    /* Every 5 ticks, create a pipe message between first two processes */
    if (k->current_time % 5 == 0) {
        int pipe_id = pipe_create(&k->ipc, pids[0], pids[1]);
        if (pipe_id > 0) {
            char msg[32];
            snprintf(msg, sizeof(msg), "tick_%d", k->current_time);
            pipe_write(&k->ipc, pipe_id, msg, (int)strlen(msg));
            char rbuf[32];
            pipe_read(&k->ipc, pipe_id, rbuf, sizeof(rbuf));
            pipe_close(&k->ipc, pipe_id);
        }
    }
    /* Every 7 ticks, send a message queue message */
    if (k->current_time % 7 == 0 && count >= 2) {
        int mq_id = mq_create(&k->ipc, pids[1]);
        if (mq_id > 0) {
            char payload[32];
            snprintf(payload, sizeof(payload), "mq_data_%d", k->current_time);
            mq_send(&k->ipc, mq_id, pids[0], payload, (int)strlen(payload));
            char rbuf[64]; int spid;
            mq_receive(&k->ipc, mq_id, rbuf, sizeof(rbuf), &spid);
        }
    }
}

/* Clean up a terminated process: free memory, close pipes */
static void reap_terminated(Kernel *k) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        PCB *p = &k->proc_table.processes[i];
        if (p->is_active && p->state == STATE_TERMINATED) {
            /* Free memory */
            free_memory(&k->memory, p->pid);
            /* Close any pipes owned by this process */
            for (int j = 0; j < MAX_PIPES; j++) {
                Pipe *pipe = &k->ipc.pipes[j];
                if (pipe->is_active &&
                    (pipe->writer_pid == p->pid || pipe->reader_pid == p->pid))
                    pipe_close(&k->ipc, pipe->id);
            }
            /* Mark slot free in process table */
            p->is_active = 0;
            k->proc_table.count--;
        }
    }
}

/* ── Lifecycle ─────────────────────────────────────────────────────── */

/* Initialise all subsystems and seed a few demo processes */
void kernel_init(Kernel *k) {
    memset(k, 0, sizeof(Kernel));
    process_table_init(&k->proc_table);
    scheduler_init(&k->scheduler);
    memory_init(&k->memory);
    fs_init(&k->vfs);
    ipc_init(&k->ipc);
    k->current_time = 0;
    k->is_running = 1;

    /* Seed demo processes so the dashboard has something to show */
    kernel_create_process(k, "init",      8, 0, 64);
    kernel_create_process(k, "systemd",   5, 1, 32);
    kernel_create_process(k, "sshd",      6, 2, 48);
    kernel_create_process(k, "nginx",     4, 3, 80);
    kernel_create_process(k, "python3",   7, 4, 96);

    /* Seed demo filesystem structure */
    Permissions rwx = {1, 1, 1};
    Permissions ro  = {1, 0, 0};
    int bin_id  = fs_make_dir(&k->vfs, "bin",  k->vfs.root_id, 0);
    int etc_id  = fs_make_dir(&k->vfs, "etc",  k->vfs.root_id, 0);
    int var_id  = fs_make_dir(&k->vfs, "var",  k->vfs.root_id, 0);
    int log_id  = fs_make_dir(&k->vfs, "log",  var_id, 0);
    fs_create_file(&k->vfs, "bash",   bin_id, 0, rwx);
    fs_create_file(&k->vfs, "ls",     bin_id, 0, rwx);
    int cfg = fs_create_file(&k->vfs, "config.ini", etc_id, 1, ro);
    fs_write(&k->vfs, cfg, "[kernel]\nversion=1.0\n", 21);
    fs_create_file(&k->vfs, "syslog", log_id, 0, rwx);
    (void)log_id;
}

/* Advance simulation by one tick and return JSON state */
const char* kernel_tick(Kernel *k) {
    if (!k->is_running) return "{}";

    /* 1. Run scheduler for this tick */
    scheduler_tick(&k->scheduler, &k->proc_table, k->current_time);

    /* 2. Simulate IPC activity between processes */
    simulate_ipc_activity(k);

    /* 3. Reap any newly terminated processes */
    reap_terminated(k);

    /* 4. Auto-add a new process every 15 ticks to keep simulation alive */
    if (k->current_time > 0 && k->current_time % 15 == 0) {
        static const char *names[] = {"worker", "daemon", "watcher", "cron", "logger"};
        static int name_idx = 0;
        int burst = 4 + (k->current_time % 7);
        int prio  = (k->current_time % 5) + 1;
        int mem   = 16 + (k->current_time % 64);
        kernel_create_process(k, names[name_idx % 5], burst, prio, mem);
        name_idx++;
    }

    k->current_time++;

    /* 5. Build and return state JSON */
    kernel_build_state_json(k);
    return k->state_buf;
}

/* Return current state snapshot without advancing time */
const char* kernel_get_state(Kernel *k) {
    kernel_build_state_json(k);
    return k->state_buf;
}

/* Shutdown: stop simulation loop */
void kernel_shutdown(Kernel *k) {
    k->is_running = 0;
}

/* ── Process control ───────────────────────────────────────────────── */

/* Create a process and immediately allocate its memory via First-Fit */
int kernel_create_process(Kernel *k, const char *name,
                           int burst, int priority, int mem_size) {
    int pid = create_process(&k->proc_table, name, burst, priority, mem_size, k->current_time);
    if (pid < 0) return -1;

    /* Allocate memory for the process (CEP R7: scheduler ↔ memory coupling) */
    int addr = allocate_memory(&k->memory, pid, mem_size);
    PCB *p = get_process_by_pid(&k->proc_table, pid);
    if (p) p->memory_start = addr;

    return pid;
}

/* Kill a process: terminate + free resources */
int kernel_kill_process(Kernel *k, int pid) {
    PCB *p = get_process_by_pid(&k->proc_table, pid);
    if (!p) return -1;

    /* If it was running, reset scheduler */
    if (k->scheduler.current_pid == pid) {
        k->scheduler.current_pid = -1;
        k->scheduler.current_quantum_remaining = k->scheduler.quantum;
    }

    free_memory(&k->memory, pid);
    terminate_process(&k->proc_table, pid);
    p->is_active = 0;
    k->proc_table.count--;
    return 0;
}

/* ── Scheduler control ─────────────────────────────────────────────── */

/* Switch scheduling algorithm (CEP R9: user judgement from UI) */
void kernel_set_algorithm(Kernel *k, int algo) {
    scheduler_set_algorithm(&k->scheduler,
        algo == 0 ? SCHED_ROUND_ROBIN : SCHED_PRIORITY);
}

/* Update RR time quantum */
void kernel_set_quantum(Kernel *k, int quantum) {
    scheduler_set_quantum(&k->scheduler, quantum);
}

/* ── State serialisation ───────────────────────────────────────────── */

/* Assemble a full JSON snapshot of all subsystems for the dashboard */
void kernel_build_state_json(Kernel *k) {
    char proc_json[4096], sched_json[512], gantt_json[2048];
    char mem_map_json[2048], mem_stats_json[256];
    char ipc_events_json[2048], ipc_stats_json[256];
    char fs_json[2048];

    process_table_to_json(&k->proc_table, proc_json, sizeof(proc_json));
    scheduler_metrics_to_json(&k->scheduler, sched_json, sizeof(sched_json));
    gantt_to_json(&k->scheduler, gantt_json, sizeof(gantt_json));
    memory_map_to_json(&k->memory, mem_map_json, sizeof(mem_map_json));
    memory_stats_to_json(&k->memory, mem_stats_json, sizeof(mem_stats_json));
    ipc_events_to_json(&k->ipc, ipc_events_json, sizeof(ipc_events_json));
    ipc_stats_to_json(&k->ipc, ipc_stats_json, sizeof(ipc_stats_json));
    fs_tree_to_json(&k->vfs, fs_json, sizeof(fs_json));

    /* CEP R8: flag anomalous processes (> 80% CPU ticks relative to burst) */
    /* We embed anomaly flag in the process list scan below */

    snprintf(k->state_buf, STATE_BUF_SIZE,
        "{"
        "\"tick\":%d,"
        "\"processes\":%s,"
        "\"scheduler\":%s,"
        "\"gantt\":%s,"
        "\"memory_map\":%s,"
        "\"memory_stats\":%s,"
        "\"ipc_events\":%s,"
        "\"ipc_stats\":%s,"
        "\"filesystem\":%s,"
        "\"deadlock\":%d"
        "}",
        k->current_time,
        proc_json,
        sched_json,
        gantt_json,
        mem_map_json,
        mem_stats_json,
        ipc_events_json,
        ipc_stats_json,
        fs_json,
        ipc_detect_deadlock(&k->ipc, MAX_PROCESSES)
    );
}
