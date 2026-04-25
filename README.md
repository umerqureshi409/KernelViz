# KernelViz — OS Scheduler & Process Monitor
**Author / Roll No:** 24CS005 | **Course:** Operating Systems CEP

> A real-time OS kernel simulator with a cyberpunk dashboard. Combines a C simulation engine, a Python WebSocket bridge, and a live browser UI.

---

## Quick Start (3 commands)

```bash
# 1. Compile C core (Linux / WSL / MinGW)
cd core && make

# 2. Start WebSocket server
cd ../bridge && python data_stream.py

# 3. Open dashboard
open ../dashboard/index.html   # or double-click in browser
```

> **Windows without WSL:** Skip step 1. The Python server runs in DEMO mode with realistic fake data — the dashboard works fully.

---

## Features

| Feature | Implementation |
|---------|---------------|
| Round Robin Scheduling | `core/scheduler.c` — O(n) per tick, configurable quantum |
| Priority Scheduling | `core/scheduler.c` — preemptive, starvation risk documented |
| First-Fit Memory Allocation | `core/memory.c` — with free-block coalescing |
| Virtual File System | `core/fs.c` — inode tree, 5 levels deep, rwx permissions |
| IPC: Pipes + Message Queues | `core/ipc.c` — with deadlock detection (CEP R4) |
| Live Gantt Chart | Canvas 2D rendering in `dashboard/app.js` |
| Memory Grid Visualisation | 32×32 canvas with hover tooltip |
| Real OS Stats | `bridge/ebpf_monitor.py` via psutil |
| WebSocket stream | `bridge/data_stream.py` — 1 Hz broadcast |
| Add/Kill processes from UI | WebSocket commands → C kernel |
| Anomaly detection | Processes >80% CPU flagged in red (CEP R8) |
| Deadlock detection | DFS on wait-for graph (CEP R4) |

---

## Project Structure

```
kernelviz/
├── core/               # C simulation engine
│   ├── kernel.c/h      # Main orchestrator, kernel_tick()
│   ├── scheduler.c/h   # Round Robin + Priority scheduling
│   ├── memory.c/h      # First-Fit memory allocator
│   ├── fs.c/h          # In-memory virtual file system
│   ├── ipc.c/h         # Pipes + message queues + deadlock detect
│   ├── process.c/h     # PCB + process lifecycle
│   └── Makefile        # → libkernelviz.so
│
├── bridge/             # Python data layer
│   ├── kernel_bridge.py   # ctypes wrapper for libkernelviz.so
│   ├── ebpf_monitor.py    # Real system stats via psutil
│   └── data_stream.py     # WebSocket server (ws://localhost:8765)
│
├── dashboard/          # Browser frontend
│   ├── index.html      # Single-page app
│   ├── style.css       # Cyberpunk dark theme
│   └── app.js          # WebSocket client + all panel rendering
│
├── docs/
│   ├── technical_report.md
│   └── user_manual.md
│
└── README.md
```

---

## CEP Coverage

| Req | Characteristic | Location |
|-----|---------------|----------|
| R1 | Depth of Knowledge | All C modules — scheduler, memory, FS, IPC theory |
| R2 | Conflicting Requirements | RR vs Priority in `scheduler.c`; First-Fit vs fragmentation in `memory.c` |
| R3 | Depth of Analysis | Gantt chart + avg wait/turnaround metrics in dashboard |
| R4 | Infrequently Encountered | DFS deadlock detection in `ipc.c` |
| R5 | Beyond Standards | Live host-OS stats via psutil in `ebpf_monitor.py` |
| R6 | Stakeholders | Simulation mode (student) ↔ Live mode (developer) auto-toggle |
| R7 | Interdependence | `kernel_tick()` chains scheduler→memory→IPC→FS |
| R8 | Consequences | Anomaly flag for processes >80% CPU in process table |
| R9 | Judgement | User overrides scheduling algorithm + adds/kills processes from UI |

---

## Dependencies

```bash
# Python
pip install psutil websockets

# C (via apt on Debian/Ubuntu/Kali)
sudo apt install gcc make
```

No npm, no React, no build tools. Pure HTML/CSS/JS with CDN-loaded Chart.js.

---

*24CS005 — KernelViz OS Monitor — CEP Submission*
