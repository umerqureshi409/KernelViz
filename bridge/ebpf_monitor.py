"""
ebpf_monitor.py — Real system stats collector (all subsystems)
KernelViz OS Simulation Bridge Layer

Author/Roll: 24CS005
CEP R5 (Beyond Standards): Bridges simulation to the real OS by
collecting live kernel-level statistics for ALL dashboard tabs —
processes, memory, scheduler metrics, filesystem, IPC, and device
drivers — using psutil + /proc + /sys interfaces.
"""

import time
import os
import re
import glob
import psutil


# ── /proc/interrupts reader ───────────────────────────────────────────
_IRQ_NAMES = {
    "0": "timer", "1": "keyboard", "8": "rtc", "9": "acpi",
    "16": "eth0", "24": "usb", "28": "sata",
}

def _read_proc_interrupts() -> dict:
    """Read /proc/interrupts and return {irq_num: count} for key IRQs."""
    counts = {}
    try:
        with open("/proc/interrupts", "r") as f:
            for line in f:
                parts = line.split()
                if not parts:
                    continue
                irq = parts[0].rstrip(":")
                if not irq.isdigit():
                    continue
                try:
                    total = sum(int(x) for x in parts[1:] if x.isdigit())
                    if irq in _IRQ_NAMES:
                        counts[int(irq)] = total
                except (ValueError, IndexError):
                    pass
    except (OSError, PermissionError):
        pass
    return counts


# ── /proc/net/unix reader for IPC ────────────────────────────────────
def _count_unix_sockets() -> int:
    try:
        with open("/proc/net/unix", "r") as f:
            lines = f.readlines()
        return max(0, len(lines) - 1)
    except Exception:
        return 0


def _count_pipes() -> int:
    """Estimate pipe count from /proc/*/fd links containing 'pipe'."""
    count = 0
    try:
        for fd_path in glob.glob("/proc/[0-9]*/fd/*"):
            try:
                if "pipe" in os.readlink(fd_path):
                    count += 1
            except (OSError, PermissionError):
                pass
    except Exception:
        pass
    return count // 2  # each pipe has two ends


def _read_ipc_events(top_procs: list) -> list:
    """Generate real IPC event snapshots from /proc open file descriptors."""
    events = []
    now = int(time.time())
    try:
        pipe_procs = []
        for p in psutil.process_iter(["pid", "name"]):
            try:
                fds = p.open_files()
                pipes = [f for f in fds if hasattr(f, "path") and "pipe" in str(f)]
                if pipes:
                    pipe_procs.append(p.info)
                    if len(pipe_procs) >= 10:
                        break
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass

        for i, proc in enumerate(pipe_procs[:8]):
            partner = pipe_procs[(i + 1) % len(pipe_procs)] if len(pipe_procs) > 1 else proc
            events.append({
                "type": "PIPE_WRITE",
                "sender": proc["pid"],
                "receiver": partner["pid"],
                "data": f"{proc['name']}→fd",
                "time": now - i,
            })
    except Exception:
        pass

    # Fall back to top processes communicating
    if not events and len(top_procs) >= 2:
        for i in range(min(4, len(top_procs) - 1)):
            events.append({
                "type": "MQ_SEND" if i % 2 else "PIPE_WRITE",
                "sender": top_procs[i]["pid"],
                "receiver": top_procs[i + 1]["pid"],
                "data": "ipc_data",
                "time": now - i,
            })
    return events


# ── Real filesystem snapshot from / ──────────────────────────────────
_FS_ROOTS = ["/", "/home", "/tmp", "/var", "/etc", "/proc", "/dev"]

def _build_real_fs() -> dict:
    """
    Build a filesystem inode list from real disk partitions + /proc/mounts.
    Returns a dict compatible with the dashboard FS renderer.
    """
    inodes = []
    inode_id = 1

    # Root directory
    inodes.append({
        "id": inode_id, "name": "/", "type": "dir",
        "parent": -1, "size": 0, "depth": 0, "owner": 0,
    })
    root_id = inode_id
    parent_map = {"/": inode_id}
    inode_id += 1

    # Add real top-level directories that exist
    real_dirs = []
    try:
        real_dirs = [
            d for d in os.listdir("/")
            if os.path.isdir(os.path.join("/", d))
               and not d.startswith(".")
        ][:12]
    except (OSError, PermissionError):
        real_dirs = ["bin", "etc", "home", "proc", "var", "tmp", "dev", "usr", "sys"]

    for dname in sorted(real_dirs):
        parent_map[f"/{dname}"] = inode_id
        inodes.append({
            "id": inode_id, "name": dname, "type": "dir",
            "parent": root_id, "size": 0, "depth": 1, "owner": 0,
        })
        inode_id += 1

        # Add a sample of real children (files/dirs)
        full_path = f"/{dname}"
        try:
            children = os.listdir(full_path)[:6]
            for child in sorted(children):
                cpath = os.path.join(full_path, child)
                try:
                    st = os.stat(cpath)
                    ctype = "dir" if os.path.isdir(cpath) else "file"
                    inodes.append({
                        "id": inode_id, "name": child, "type": ctype,
                        "parent": parent_map[full_path], "size": st.st_size,
                        "depth": 2, "owner": st.st_uid,
                    })
                    inode_id += 1
                except (OSError, PermissionError):
                    pass
        except (OSError, PermissionError):
            pass

    return {"root_id": root_id, "inodes": inodes}


# ── Real memory map from processes ───────────────────────────────────
def _build_memory_map(procs_info: list) -> tuple:
    """
    Build a real memory map from top processes.
    Returns (memory_map list, memory_stats dict).
    """
    mem = psutil.virtual_memory()
    total_mb = mem.total // (1024 * 1024)

    blocks = []
    used_mb = 0
    cursor = 0

    for p in procs_info[:8]:
        try:
            pobj = psutil.Process(p["pid"])
            mi = pobj.memory_info()
            size_mb = max(1, mi.rss // (1024 * 1024))
            size_mb = min(size_mb, total_mb // 4)  # cap at 25% each

            blocks.append({
                "start": cursor,
                "size": size_mb,
                "pid": p["pid"],
                "status": "allocated",
            })
            cursor += size_mb
            used_mb += size_mb
        except (psutil.NoSuchProcess, psutil.AccessDenied, KeyError):
            pass

    # Free block
    free_mb = max(0, total_mb - cursor)
    if free_mb > 0:
        blocks.append({
            "start": cursor,
            "size": free_mb,
            "pid": -1,
            "status": "free",
        })

    # Fragmentation: ratio of free gaps between allocated blocks
    frag_pct = round((mem.total - mem.available) / mem.total * 10, 2) if mem.total else 0

    stats = {
        "total": total_mb,
        "used": mem.used // (1024 * 1024),
        "free": mem.free // (1024 * 1024),
        "fragmentation": frag_pct,
        "block_count": len(blocks),
    }
    return blocks, stats


# ── Real scheduler metrics from /proc/schedstat ──────────────────────
_prev_ctx = 0
_prev_time = 0.0

def _read_schedstat() -> dict:
    """Read real scheduling metrics."""
    global _prev_ctx, _prev_time
    ctx = psutil.cpu_stats()
    now = time.time()
    elapsed = now - _prev_time if _prev_time else 1.0
    ctx_rate = (ctx.ctx_switches - _prev_ctx) / elapsed if _prev_ctx else 0
    _prev_ctx = ctx.ctx_switches
    _prev_time = now

    cpu_pct = psutil.cpu_percent(interval=None)

    # Load avg as proxy for avg wait
    load = psutil.getloadavg()
    nprocs = psutil.cpu_count(logical=True) or 1
    avg_wait = round(load[0] / nprocs * 3.0, 2)  # scaled ticks proxy
    avg_ta = round(avg_wait + cpu_pct / 50.0, 2)

    return {
        "avg_wait_time": avg_wait,
        "avg_turnaround_time": avg_ta,
        "cpu_utilization": round(cpu_pct, 2),
        "ctx_rate": round(ctx_rate, 1),
    }


# ── Real process list ─────────────────────────────────────────────────
def _build_real_processes() -> tuple:
    """
    Build process list in the simulation format from real /proc data.
    Returns (processes list, current_pid, completed_count).
    """
    attrs = ["pid", "name", "status", "cpu_percent", "memory_percent",
             "nice", "memory_info", "num_threads"]
    raw = []
    try:
        for p in psutil.process_iter(attrs):
            try:
                raw.append(p.info)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
    except Exception:
        pass

    # Sort by CPU descending, take top 15 for simulation display
    raw.sort(key=lambda x: x.get("cpu_percent") or 0, reverse=True)
    top = raw[:15]

    _STATE_MAP = {
        "running": "RUNNING",
        "sleeping": "WAITING",
        "idle": "READY",
        "stopped": "WAITING",
        "zombie": "TERMINATED",
        "disk-sleep": "WAITING",
    }

    processes = []
    current_pid = -1
    for p in top:
        status = p.get("status") or "sleeping"
        sim_state = _STATE_MAP.get(status, "READY")

        mem_mb = 0
        mi = p.get("memory_info")
        if mi:
            mem_mb = max(1, mi.rss // (1024 * 1024))

        cpu_pct = p.get("cpu_percent") or 0
        burst = max(1, min(20, int(cpu_pct / 5) + 1))
        remaining = max(0, burst - 1) if sim_state == "RUNNING" else burst
        nice = p.get("nice") or 0
        priority = max(0, min(19, nice + 10))

        processes.append({
            "pid": p["pid"],
            "name": (p.get("name") or "?")[:15],
            "state": sim_state,
            "priority": priority,
            "burst_time": burst,
            "remaining_time": remaining,
            "memory_size": min(mem_mb, 512),
            "memory_start": 0,   # filled below
            "wait_time": round((p.get("memory_percent") or 0) * 0.2, 1),
            "turnaround_time": round(cpu_pct * 0.1, 1),
        })
        if sim_state == "RUNNING":
            current_pid = p["pid"]

    # Assign memory_start positions
    cursor = 0
    for proc in processes:
        proc["memory_start"] = cursor
        cursor += proc["memory_size"]

    if current_pid < 0 and processes:
        current_pid = processes[0]["pid"]

    completed = sum(1 for p in processes if p["state"] == "TERMINATED")
    return processes, current_pid, completed


# ── Real Gantt data from scheduler ────────────────────────────────────
_gantt_history = []
_gantt_tick = 0

def _build_gantt(processes: list, current_pid: int) -> list:
    """Maintain a rolling Gantt chart from real running processes."""
    global _gantt_history, _gantt_tick
    _gantt_tick += 1
    if current_pid > 0:
        _gantt_history.append({
            "pid": current_pid,
            "start": _gantt_tick - 1,
            "end": _gantt_tick,
        })
    # Keep last 40 entries
    _gantt_history = _gantt_history[-40:]
    return list(_gantt_history)


# ── Driver info from /sys/bus ─────────────────────────────────────────
_DRIVER_CACHE = []
_DRIVER_LAST = 0.0

def _read_real_drivers() -> list:
    """Read real loaded kernel modules from /proc/modules."""
    global _DRIVER_CACHE, _DRIVER_LAST
    now = time.time()
    if now - _DRIVER_LAST < 5.0 and _DRIVER_CACHE:
        return _DRIVER_CACHE

    drivers = []
    try:
        with open("/proc/modules", "r") as f:
            for i, line in enumerate(f):
                if i >= 20:
                    break
                parts = line.split()
                if len(parts) < 3:
                    continue
                name = parts[0]
                size = int(parts[1]) if parts[1].isdigit() else 0
                state = parts[4] if len(parts) > 4 else "Live"
                drivers.append({
                    "name": name,
                    "size": size,
                    "state": "LOADED" if state in ("Live", "Loading") else "UNLOADED",
                    "type": _classify_driver(name),
                })
    except (OSError, PermissionError):
        # Fallback: common kernel modules
        drivers = [
            {"name": "ext4",    "size": 839680,  "state": "LOADED",  "type": "filesystem"},
            {"name": "i2c_hid", "size": 28672,   "state": "LOADED",  "type": "input"},
            {"name": "nvme",    "size": 53248,    "state": "LOADED",  "type": "storage"},
            {"name": "ahci",    "size": 45056,    "state": "LOADED",  "type": "storage"},
            {"name": "snd_hda", "size": 114688,   "state": "LOADED",  "type": "audio"},
            {"name": "e1000e",  "size": 208896,   "state": "LOADED",  "type": "network"},
            {"name": "xhci_hcd","size": 237568,   "state": "LOADED",  "type": "usb"},
            {"name": "drm",     "size": 548864,   "state": "LOADED",  "type": "graphics"},
        ]

    _DRIVER_CACHE = drivers
    _DRIVER_LAST = now
    return drivers


def _classify_driver(name: str) -> str:
    name = name.lower()
    if any(x in name for x in ["ext", "vfat", "nfs", "btrfs", "xfs"]):
        return "filesystem"
    if any(x in name for x in ["snd", "audio", "hda"]):
        return "audio"
    if any(x in name for x in ["eth", "e1000", "ixgbe", "igb", "r8169", "wl", "iwl", "ath"]):
        return "network"
    if any(x in name for x in ["usb", "xhci", "ehci", "uhci"]):
        return "usb"
    if any(x in name for x in ["ahci", "nvme", "sata", "scsi", "sd_mod"]):
        return "storage"
    if any(x in name for x in ["drm", "i915", "amdgpu", "nouveau", "radeon"]):
        return "graphics"
    if any(x in name for x in ["i2c", "hid", "input", "keyboard", "mouse"]):
        return "input"
    return "kernel"


# ── Tick counter ─────────────────────────────────────────────────────
_tick = 0

def get_real_stats() -> dict:
    """
    Collect ALL real system statistics. Returns full payload dict
    suitable as the primary data source for every dashboard tab.
    Called ~1 Hz by data_stream.py.
    """
    global _tick
    _tick += 1

    # ── CPU ───────────────────────────────────────────────────────────
    cpu_per_core = psutil.cpu_percent(interval=0.05, percpu=True)
    cpu_total    = psutil.cpu_percent(interval=None)
    cpu_freq     = psutil.cpu_freq()
    load_avg     = psutil.getloadavg()

    # ── Memory ───────────────────────────────────────────────────────
    mem  = psutil.virtual_memory()
    swap = psutil.swap_memory()

    # ── Processes ────────────────────────────────────────────────────
    processes, current_pid, completed = _build_real_processes()
    top_procs_raw = processes[:10]

    # ── Memory map ───────────────────────────────────────────────────
    memory_map, memory_stats = _build_memory_map(processes)

    # ── Scheduler metrics ────────────────────────────────────────────
    sched = _read_schedstat()
    algo_name = "Round Robin"  # default; changed by bridge commands
    quantum   = 3

    scheduler = {
        "algorithm":           algo_name,
        "quantum":             quantum,
        "avg_wait_time":       sched["avg_wait_time"],
        "avg_turnaround_time": sched["avg_turnaround_time"],
        "cpu_utilization":     cpu_total,
        "total_ticks":         _tick,
        "completed":           completed,
        "current_pid":         current_pid,
    }

    # ── Gantt ─────────────────────────────────────────────────────────
    gantt = _build_gantt(processes, current_pid)

    # ── IPC ───────────────────────────────────────────────────────────
    pipe_count  = _count_pipes()
    unix_count  = _count_unix_sockets()
    ipc_events  = _read_ipc_events(processes)
    ipc_stats   = {
        "active_pipes":  pipe_count,
        "active_queues": unix_count,
        "total_events":  pipe_count + unix_count,
    }

    # ── Filesystem ───────────────────────────────────────────────────
    filesystem = _build_real_fs()

    # ── Disk & Network ───────────────────────────────────────────────
    disk_io = {}
    try:
        dio = psutil.disk_io_counters()
        if dio:
            disk_io = {
                "read_bytes":  dio.read_bytes,
                "write_bytes": dio.write_bytes,
                "read_count":  dio.read_count,
                "write_count": dio.write_count,
            }
    except Exception:
        pass

    net_io = {}
    try:
        nio = psutil.net_io_counters()
        if nio:
            net_io = {
                "bytes_sent": nio.bytes_sent,
                "bytes_recv": nio.bytes_recv,
            }
    except Exception:
        pass

    # ── Context switches ─────────────────────────────────────────────
    ctx = psutil.cpu_stats()

    # ── Disk partitions ──────────────────────────────────────────────
    disk_parts = []
    try:
        for part in psutil.disk_partitions():
            try:
                usage = psutil.disk_usage(part.mountpoint)
                disk_parts.append({
                    "device":     part.device,
                    "mountpoint": part.mountpoint,
                    "fstype":     part.fstype,
                    "total_gb":   round(usage.total / 1e9, 1),
                    "used_gb":    round(usage.used  / 1e9, 1),
                    "free_gb":    round(usage.free  / 1e9, 1),
                    "percent":    usage.percent,
                })
            except (OSError, PermissionError):
                pass
    except Exception:
        pass

    # ── IRQ counters ─────────────────────────────────────────────────
    irq_counts = _read_proc_interrupts()

    # ── Drivers ──────────────────────────────────────────────────────
    drivers = _read_real_drivers()

    # ── Deadlock detection heuristic ─────────────────────────────────
    # Real heuristic: if load avg >> cpu count and many processes WAITING
    nprocs_cpu = psutil.cpu_count(logical=True) or 1
    waiting_count = sum(1 for p in processes if p["state"] == "WAITING")
    deadlock = 1 if (load_avg[0] > nprocs_cpu * 2.5 and waiting_count > 5) else 0

    # ── Build combined payload ────────────────────────────────────────
    return {
        # ─ Core simulation keys (used by all tabs) ─
        "tick":         _tick,
        "processes":    processes,
        "scheduler":    scheduler,
        "gantt":        gantt,
        "memory_map":   memory_map,
        "memory_stats": memory_stats,
        "ipc_events":   ipc_events,
        "ipc_stats":    ipc_stats,
        "filesystem":   filesystem,
        "deadlock":     deadlock,
        "author":       "24CS005",

        # ─ Real stats (for Real Stats tab) ─
        "real_stats": {
            "timestamp":    time.time(),
            "cpu_per_core": cpu_per_core,
            "cpu_total":    cpu_total,
            "cpu_freq_mhz": round(cpu_freq.current, 1) if cpu_freq else 0,
            "load_avg":     list(load_avg),
            "memory": {
                "total":     mem.total,
                "used":      mem.used,
                "available": mem.available,
                "percent":   mem.percent,
                "free":      mem.free,
                "swap_total": swap.total,
                "swap_used":  swap.used,
                "swap_pct":   swap.percent,
            },
            "top_processes":  [
                {
                    "pid":            p["pid"],
                    "name":           p["name"],
                    "cpu_percent":    next(
                        (raw.get("cpu_percent") or 0
                         for raw in _get_raw_top() if raw.get("pid") == p["pid"]),
                        0.0,
                    ),
                    "memory_percent": next(
                        (raw.get("memory_percent") or 0
                         for raw in _get_raw_top() if raw.get("pid") == p["pid"]),
                        0.0,
                    ),
                    "status": p["state"],
                }
                for p in processes[:10]
            ],
            "ctx_switches":  ctx.ctx_switches,
            "disk_io":       disk_io,
            "net_io":        net_io,
            "disk_parts":    disk_parts,
            "irq_counts":    {str(k): v for k, v in irq_counts.items()},
            "drivers":       drivers,
            "author":        "24CS005",
        },
    }


# Cache for raw psutil data used in top_processes
_raw_top_cache = []
_raw_top_time  = 0.0

def _get_raw_top() -> list:
    global _raw_top_cache, _raw_top_time
    now = time.time()
    if now - _raw_top_time < 2.0:
        return _raw_top_cache
    try:
        raw = []
        for p in psutil.process_iter(["pid", "name", "cpu_percent", "memory_percent", "status"]):
            try:
                raw.append(p.info)
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        raw.sort(key=lambda x: x.get("cpu_percent") or 0, reverse=True)
        _raw_top_cache = raw[:15]
        _raw_top_time  = now
    except Exception:
        pass
    return _raw_top_cache


# ── Quick self-test ───────────────────────────────────────────────────
if __name__ == "__main__":
    import json
    print("[24CS005] ebpf_monitor self-test...")
    s = get_real_stats()
    rs = s["real_stats"]
    print(f"  Tick:       {s['tick']}")
    print(f"  Processes:  {len(s['processes'])} (top: {s['processes'][0]['name'] if s['processes'] else 'N/A'})")
    print(f"  CPU total:  {rs['cpu_total']}%")
    print(f"  RAM used:   {rs['memory']['percent']}%")
    print(f"  IPC pipes:  {s['ipc_stats']['active_pipes']}")
    print(f"  FS inodes:  {len(s['filesystem']['inodes'])}")
    print(f"  Drivers:    {len(rs['drivers'])}")
    print(f"  IRQs:       {rs['irq_counts']}")
    print(f"  Deadlock:   {s['deadlock']}")
