/*
 * kernel.h — Main OS simulation kernel interface
 * KernelViz OS Simulation Engine
 *
 * CEP R1 (Depth of Knowledge): The kernel is the central orchestrator.
 * It initialises all subsystems and drives the simulation loop via
 * kernel_tick(), which mirrors how a real OS timer interrupt triggers
 * the scheduler, memory reclaimer, and IPC dispatcher each quantum.
 *
 * CEP R7 (Interdependence): Every tick shows the chain:
 *   Scheduler decision → Memory allocation/free → IPC event dispatch
 * A terminated process frees memory AND closes its pipes in one tick.
 *
 * CEP R8 (Consequences): Processes using >80% of total CPU ticks are
 * flagged as ANOMALY in the exported state JSON for the dashboard.
 *
 * Exported symbols are decorated with KVIZ_API so the Python bridge
 * can locate them in the shared library via ctypes.
 */

#ifndef KERNEL_H
#define KERNEL_H

#include "process.h"
#include "scheduler.h"
#include "memory.h"
#include "fs.h"
#include "ipc.h"

/* Mark functions for shared-library export */
#define KVIZ_API

/* Maximum size of the JSON state snapshot returned each tick */
#define STATE_BUF_SIZE (1024 * 16)   /* 16 KB */

/* Combined kernel state — owns all subsystems */
typedef struct {
    ProcessTable proc_table;   /* All process control blocks */
    Scheduler    scheduler;    /* CPU scheduling state */
    MemoryManager memory;      /* Memory allocator state */
    VFS          vfs;          /* Virtual file system */
    IPCManager   ipc;          /* Pipes + message queues */
    int          current_time; /* Simulation clock (ticks) */
    int          is_running;   /* 1 if kernel is active */
    char         state_buf[STATE_BUF_SIZE]; /* Reusable JSON output buffer */
} Kernel;

/* ── Lifecycle ─────────────────────────────────────────────────────── */

/* Initialise all kernel subsystems and seed a few demo processes */
KVIZ_API void kernel_init(Kernel *k);

/* Advance simulation by one time unit. Returns JSON state string */
KVIZ_API const char* kernel_tick(Kernel *k);

/* Return current state snapshot as JSON without advancing time */
KVIZ_API const char* kernel_get_state(Kernel *k);

/* Clean shutdown of all subsystems */
KVIZ_API void kernel_shutdown(Kernel *k);

/* ── Process control ───────────────────────────────────────────────── */

/* Create a process and allocate its memory. Returns PID or -1 */
KVIZ_API int kernel_create_process(Kernel *k, const char *name,
                                   int burst, int priority, int mem_size);

/* Terminate a process, free its memory, close its pipes */
KVIZ_API int kernel_kill_process(Kernel *k, int pid);

/* ── Scheduler control ─────────────────────────────────────────────── */

/* Switch scheduling algorithm: 0=Round Robin, 1=Priority */
KVIZ_API void kernel_set_algorithm(Kernel *k, int algo);

/* Change Round Robin quantum */
KVIZ_API void kernel_set_quantum(Kernel *k, int quantum);

/* ── Internal helpers (not exported to bridge) ─────────────────────── */

/* Build the full JSON state snapshot into k->state_buf */
void kernel_build_state_json(Kernel *k);

#endif /* KERNEL_H */
