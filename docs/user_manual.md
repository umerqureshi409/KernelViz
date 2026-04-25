# KernelViz — User Manual
**Author:** 24CS005 | **Version:** 1.0

---

## 1. Installation

### Prerequisites
| Tool | Version | Purpose |
|------|---------|---------|
| GCC  | ≥ 9.0   | Compile C core |
| Python | ≥ 3.9 | Run bridge + server |
| Modern browser | Chrome/Firefox/Edge | View dashboard |

### Install Python dependencies
```bash
pip install psutil websockets
```

### Clone / unzip the project
```
kernelviz/
├── core/          ← C simulation engine
├── bridge/        ← Python WebSocket server
├── dashboard/     ← HTML/CSS/JS frontend
└── docs/          ← This manual + technical report
```

---

## 2. Running KernelViz (3 commands)

### Step 1 — Compile the C core
```bash
cd kernelviz/core
make
# Expected output:
# [OK] Built libkernelviz.so
```
> **Note (Windows):** Use WSL or MinGW. The `.so` is a Linux shared library.
> On Windows native, the Python bridge runs in DEMO mode automatically.

### Step 2 — Start the WebSocket server
```bash
cd kernelviz/bridge
python data_stream.py
```
Expected output:
```
[24CS005] KernelViz WebSocket server starting on ws://localhost:8765
[24CS005] Kernel mode: LIVE (libkernelviz.so)   ← or DEMO if no .so
```

### Step 3 — Open the dashboard
Open `kernelviz/dashboard/index.html` in your browser.

The header connection dot will turn **green** and show **LIVE** within 1–2 seconds.

---

## 3. Using the Dashboard

### Panel 1 — System Header Bar (top)
| Element | Description |
|---------|-------------|
| `KERNELVIZ` title | Glowing logo — always visible |
| `● LIVE MODE` badge | Green = connected to server; shows `RECONNECTING` if disconnected |
| `24CS005` badge | Your roll number — always displayed |
| TICK counter | Simulation clock — increments every second |
| CPU / MEM / PROCS pills | At-a-glance system summary |
| Clock | Live local time |
| UP: Xs counter | Time since dashboard opened + roll number |

### Panel 2 — CPU Scheduler (left column)
| Element | Description |
|---------|-------------|
| Gantt Chart | Horizontal timeline: coloured blocks show which process ran each tick. Scrolls right automatically. |
| `RR ↔ PRIORITY` button | Click to toggle between Round Robin and Priority scheduling |
| `Q` input | Time quantum for Round Robin (1–20 ticks). Press Enter or Tab to apply. |
| AVG WAIT / AVG TURNAROUND | Running averages across all completed processes |
| CPU UTIL | Percentage of ticks where CPU was busy (not idle) |
| READY QUEUE | Live list of processes waiting for CPU with burst-progress bars |

### Panel 3 — Memory Map (center)
| Element | Description |
|---------|-------------|
| 32×32 grid | Each cell = 1 memory unit. Green = free, colours = allocated by PID |
| Hover tooltip | Shows PID, block address, and size |
| FRAGMENTATION arc gauge | Real-time fragmentation % — green → yellow → orange as it rises |
| USED / FREE / FRAG bars | Numeric memory breakdown |

### Panel 4 — Process Table (right column)
| Element | Description |
|---------|-------------|
| Table rows | PID, name, state badge, priority, memory, remaining/burst time, wait time, KILL button |
| State badges | Blue=NEW, Yellow=READY, Green=RUNNING (pulsing), Orange=WAITING, Grey=TERMINATED, Red=ANOMALY |
| `+ ADD PROCESS` button | Opens the inline add-process form |
| `KILL` button | Immediately terminates that process |
| ⚠ DEADLOCK banner | Appears (red, pulsing) if a deadlock cycle is detected in the wait-for graph |

### Panel 5 — Real System Stats (bottom bar)
| Card | Description |
|------|-------------|
| CPU PER CORE | Live horizontal bar chart of real CPU usage per core (via psutil) |
| RAM USAGE | Donut chart showing real RAM used vs free |
| TOP PROCESSES | Top 5 real system processes by CPU% with progress bars |
| IPC EVENT LOG | Scrolling terminal log of pipe/MQ events from the simulation |

---

## 4. Adding Processes (Walkthrough)

1. Click **`+ ADD PROCESS`** in Panel 4 header
2. Fill in the inline form:
   - **Name** — up to 31 characters (e.g. `myworker`)
   - **Burst** — CPU time needed (1–50 ticks)
   - **Priority** — 0 = highest, 9 = lowest
   - **Memory** — units required (1–512)
3. Click **CREATE** (or press Enter)
4. The command is sent over WebSocket → kernel creates the process → it appears in the table within 1 second
5. Watch the Gantt chart: the new process enters the READY queue and gets scheduled in its turn

---

## 5. Understanding the Metrics

### CPU Utilisation
```
CPU Utilisation = (busy_ticks / total_ticks) × 100
```
A value near 100% means the scheduler always had work to do. Low values indicate idle periods (no READY processes).

### Average Wait Time
```
Avg Wait = Σ(wait_time for each completed process) / completed_count
```
Wait time accumulates every tick a process spends in READY state. Lower = better (less time wasted in queue).

### Average Turnaround Time
```
Turnaround = completion_tick − arrival_tick
Avg Turnaround = Σ(turnaround) / completed_count
```
Total time from process creation to completion. Includes both wait time and execution time.

### Memory Fragmentation
```
Fragmentation % = (1 − largest_free_block / total_free) × 100
```
0% means all free memory is contiguous (ideal). 100% means free memory is completely scattered in tiny unusable fragments.

### Anomaly Detection (CEP R8)
A process is flagged **ANOMALY** (red badge) when:
```
(burst_time - remaining_time) / total_ticks > 0.80
```
i.e., it has consumed >80% of all CPU ticks — indicating a potential runaway process.

### Deadlock Detection (CEP R4)
The kernel maintains a **wait-for graph**: an edge from process A to B means A is waiting for B. A cycle in this graph (detected by DFS) means deadlock. The `⚠ DEADLOCK DETECTED` banner appears in Panel 4 when this occurs.

---

*KernelViz — 24CS005 — OS CEP Project*
