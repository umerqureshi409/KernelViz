# KernelViz — Technical Report
**Student:** 24CS005 | **Course:** Operating Systems (CEP)

---

## 1. Abstract

KernelViz is a three-layer OS simulation and monitoring system that implements core kernel subsystems — CPU scheduling, memory management, inter-process communication, and a virtual file system — in ANSI C, bridges them to a Python WebSocket server, and visualises everything in a real-time browser dashboard. The system combines a deterministic simulation engine with live host-OS statistics collected via psutil, giving users simultaneous insight into both simulated and real kernel behaviour. The result is a working demo that covers all nine CEP characteristics through concrete, runnable code and an impressive cyberpunk-themed UI.

---

## 2. System Architecture

```
┌──────────────────────────────────────────────────────────┐
│  Layer 3 — Dashboard (HTML/CSS/JS)                       │
│  WebSocket client · Chart.js · Canvas Gantt + Memory     │
│  Live process table · IPC log · Real system stats        │
└────────────────────────┬─────────────────────────────────┘
                         │ ws://localhost:8765
┌────────────────────────▼─────────────────────────────────┐
│  Layer 2 — Python Bridge                                 │
│  kernel_bridge.py (ctypes) · ebpf_monitor.py (psutil)   │
│  data_stream.py (websockets) — merges & streams          │
└────────────────────────┬─────────────────────────────────┘
                         │ ctypes FFI
┌────────────────────────▼─────────────────────────────────┐
│  Layer 1 — C Core (libkernelviz.so)                      │
│  kernel.c · scheduler.c · memory.c · fs.c · ipc.c       │
│  process.c — all compiled as a POSIX shared library      │
└──────────────────────────────────────────────────────────┘
```

Each layer communicates via clearly defined interfaces: C exports JSON strings via `char*` return values; Python decodes them and augments with real psutil data; the browser receives a merged JSON blob over WebSocket every second.

---

## 3. Kernel Design

### 3.1 Process Management (`process.c / process.h`)
Each process is represented by a **PCB (Process Control Block)** holding: PID, name, state (NEW→READY→RUNNING→WAITING→TERMINATED), priority, burst time, remaining time, arrival time, memory start address, wait time, and turnaround time. A fixed-size `ProcessTable[50]` avoids dynamic allocation.

### 3.2 CPU Scheduler (`scheduler.c / scheduler.h`)
Two algorithms share one tick-driven interface:
- **Round Robin** (`schedule_rr`): each process gets `quantum` ticks, then is preempted to the tail of the READY queue. Fair by design.
- **Priority Scheduling** (`schedule_priority`): the highest-priority (lowest number) READY process runs; preempted if a higher-priority process arrives.

After each completed process, running averages for wait time, turnaround time, and CPU utilisation are updated in O(1). A **Gantt chart** (ring buffer of 256 entries) records which PID ran each tick for dashboard visualisation.

### 3.3 Memory Manager (`memory.c / memory.h`)
Simulates **1024 units** of RAM as a sorted list of `MemBlock` structs. **First-Fit** scans from address 0 and returns the first block large enough — O(n) where n is the current block count (max 128). On free, adjacent free blocks are **coalesced** to combat external fragmentation. Fragmentation % = `1 − (largest_free / total_free) × 100`.

### 3.4 Virtual File System (`fs.c / fs.h`)
An in-memory inode table (max 100 inodes) forms a tree up to 5 levels deep. Operations: `create_file`, `make_dir`, `delete`, `read`, `write`, `find`. Each inode stores permissions (rwx bitmask), owner PID, size, and content (max 512 B). The root `/` directory is inode 1; parent-child links are maintained as child ID arrays.

### 3.5 IPC (`ipc.c / ipc.h`)
- **Pipes**: byte-stream buffers (256 B) between writer and reader PIDs. `pipe_write` / `pipe_read` with data shift on consume.
- **Message Queues**: circular buffer of 16 messages per queue. `mq_send` / `mq_receive`.
- **Deadlock Detection** (CEP R4): a 50×50 **wait-for graph** detects cycles using iterative DFS — a back-edge in the recursion stack signals a deadlock.

### 3.6 Kernel Orchestrator (`kernel.c / kernel.h`)
`kernel_tick()` is the heart of the simulation:
1. Call `scheduler_tick()` → decide which process runs
2. Simulate IPC activity (pipe + MQ exchanges every N ticks)
3. Reap terminated processes → free memory, close pipes
4. Auto-spawn a new process every 15 ticks to keep the simulation alive
5. Serialize all subsystem state to a single JSON string (~4–16 KB)

---

## 4. Algorithm Analysis

### 4.1 Round Robin — O(n) per tick
The ready queue is scanned linearly from `rr_index` wrapping at `MAX_PROCESSES = 50`. In the worst case all 50 slots are checked. With 1 Hz ticks this is negligible. Context-switch overhead is modelled as zero (instantaneous at tick boundary), which simplifies analysis while preserving correctness of wait/turnaround metrics.

**Example** (quantum = 3):
```
Processes: P1(burst=6), P2(burst=4), P3(burst=2)
Tick 1-3:  P1 runs (remaining=3)
Tick 4-6:  P2 runs (remaining=1)
Tick 7:    P3 runs (remaining=1)
Tick 8:    P3 completes — turnaround = 8, wait = 6
Tick 8-10: P1 runs (remaining=0) — completes
Tick 10:   P2 completes — wait=4, turnaround=10
```

### 4.2 Priority Scheduling — preemptive, O(n) per tick
Scans all active processes each tick to find lowest priority number. Preempts current process if a higher-priority arrival is detected. Risk: **starvation** of low-priority processes if high-priority processes keep arriving.

### 4.3 First-Fit Memory — O(n) blocks
Iterates the block list from index 0. For n=128 blocks this is fast (<1 µs). Coalescing after each free is O(n²) worst-case but runs rarely. The tradeoff: speed vs. fragmentation growth over time.

---

## 5. CEP Characteristics Mapping

| # | Characteristic | How Addressed in KernelViz |
|---|---|---|
| R1 | Depth of Knowledge | PCB lifecycle, RR/Priority algorithms, First-Fit, inode VFS, IPC — all implemented from scratch in C with full OS theory applied |
| R2 | Conflicting Requirements | RR (fairness) vs Priority (efficiency) in `scheduler.c`; First-Fit (speed) vs fragmentation in `memory.c` — both documented and visualised |
| R3 | Depth of Analysis | Post-run metrics: avg wait, avg turnaround, CPU utilisation, fragmentation %; Gantt chart for visual analysis |
| R4 | Infrequently Encountered | Wait-for graph deadlock detection via DFS cycle detection in `ipc.c` — rare in student projects |
| R5 | Beyond Standards | Python psutil bridge pulls live kernel statistics from the real host OS and merges with simulation data in the WebSocket stream |
| R6 | Stakeholders | SIMULATION MODE (student learning) vs LIVE MODE (developer monitoring) — toggled automatically based on WebSocket connection status |
| R7 | Interdependence | `kernel_tick()` chains: scheduler → memory alloc/free → IPC → FS; a process termination cascades through all subsystems |
| R8 | Consequences | Processes consuming >80% of CPU ticks are flagged "ANOMALY" in red in the process table (CEP R8 security awareness) |
| R9 | Judgement | User can add processes, kill processes, switch algorithms, change quantum from the dashboard — overriding default kernel decisions |

---

## 6. Results & Metrics

| Metric | Value (fill during demo) |
|---|---|
| Avg Wait Time (RR, Q=3) | — ticks |
| Avg Wait Time (Priority) | — ticks |
| Avg Turnaround (RR) | — ticks |
| CPU Utilisation | — % |
| Peak Fragmentation | — % |
| IPC Events / minute | — |
| Deadlock detections | — |
| Real RAM (host) | — GB |
| Real CPU cores shown | — |

---

## 7. Conclusion

KernelViz demonstrates that all nine CEP characteristics can be covered in a single cohesive project by designing across three abstraction layers. The C core provides rigorous OS algorithm implementations; the Python bridge makes the simulation accessible over a standard WebSocket API; the JavaScript dashboard makes every data point visible in real time with professional-grade visualisation. The project goes beyond textbook simulation by connecting to the real host OS via psutil, giving students a direct comparison between simulated and actual kernel behaviour.

*— 24CS005*

---

## 9. User Authentication & Security Subsystem (v1.1)

### 9.1 Multi-User Login
KernelViz OS 1.1 adds a Linux-style TTY login sequence between the boot screen and the OS shell. Four built-in accounts are provided with distinct privilege levels:

| UID   | Username | Role        | Shell       | Capabilities                      |
|-------|----------|-------------|-------------|-----------------------------------|
| 0     | root     | Superuser   | /bin/bash   | Unrestricted (full kernel access) |
| 1000  | admin    | Admin       | /bin/bash   | mount, modprobe, raw sockets      |
| 1001  | student  | Standard    | /bin/sh     | Own processes, read /proc         |
| 65534 | guest    | Guest       | /sbin/nologin | Read-only, restricted sandbox   |

Credentials are stored in a `USER_DB` object in `login.js` (analogous to `/etc/passwd` + `/etc/shadow`). Authentication flow:
1. Boot completes → `showLogin()` is called.
2. User selects account or types username → password is prompted (except guest).
3. `attemptLogin()` validates credentials; failed attempts are logged.
4. On success, `window.SESSION` is populated and `initOS()` is called.
5. Logout restores the login screen without page reload.

### 9.2 Security Audit Log
Every login success, login failure, and logout is appended to an in-memory `securityLog[]` array and displayed in the Security panel in reverse-chronological order, matching the format of Linux's `/var/log/auth.log`.

### 9.3 Permission Model
The Security panel renders the current user's POSIX-style permissions (read shadow, write home, kill processes, mount, modprobe, raw sockets) dynamically based on their role, accurately reflecting Linux DAC (Discretionary Access Control).

### 9.4 Simulated Encryption
The Security panel reports AES-256-XTS disk encryption, ChaCha20-Poly1305 IPC encryption, kernel ASLR level 4, stack canaries, and Seccomp enforcement — matching a hardened Linux system's security profile.

---

## 10. Device Drivers & IRQ Simulation

### 10.1 Loaded Modules
The Drivers panel renders a `lsmod`-style table of 7 real Linux kernel modules (drm_kms_helper, e1000e, xhci_hcd, ahci, snd_hda_intel, btusb, aesni_intel). The sound driver realistically transitions between LOADING and LOADED states.

### 10.2 IRQ Counter Simulation
The IRQ table updates every second with realistic interrupt rates:
- **IRQ 0 (timer):** ~1000 interrupts/second (hardware clock)
- **IRQ 1 (keyboard):** sparse (5% chance per second)
- **IRQ 16 (e1000e network):** ~30–70 per second
- **IRQ 24 (USB):** ~5–25 per second
- **IRQ 28 (SATA):** ~10–40 per second

### 10.3 Terminal Commands
New offline-mode commands: `whoami`, `id`, `users`, `last`, `passwd`, `uptime`, `hostname`, `lsmod`, `lspci`, `ifconfig`, `sudo`, `su`, `logout`/`exit`.

---

## 11. CEP Compliance Matrix (Final)

| Requirement            | Implementation                                                    | File(s)              |
|------------------------|-------------------------------------------------------------------|----------------------|
| Kernel Development     | kernel.c with tick engine, system calls, state machine           | core/kernel.c        |
| Process Scheduling     | Round Robin + Priority, Gantt chart, live metrics                | core/scheduler.c     |
| Memory Management      | First-fit allocator, fragmentation display, compaction           | core/memory.c        |
| File System            | Hierarchical VFS with permissions, inode table                   | core/fs.c            |
| Device Drivers         | 7 modules, IRQ table, DMA channels, live counters                | dashboard/           |
| IPC                    | Shared memory, pipes, message queues, semaphores, deadlock       | core/ipc.c           |
| Security               | Multi-user login, ACL, audit log, encryption status, ASLR       | dashboard/login.js   |
| GUI                    | Cyberpunk dashboard, 10 panels, boot sequence, real-time charts  | dashboard/           |
| Error Handling         | Kernel alert system, deadlock detection, error log in terminal   | app.js, kernel.c     |
| Performance Monitoring | CPU/Memory rings, history sparkline, real psutil stats           | bridge/, dashboard/  |
