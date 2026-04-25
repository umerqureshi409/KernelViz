"""
data_stream.py — WebSocket server: serves real system stats to dashboard
KernelViz OS Simulation — Production Version

Author/Roll: 24CS005
CEP R5 (Beyond Standards): Live WebSocket bridge between real Linux OS
and the browser dashboard. ALL tabs now show real system data.
"""

import asyncio
import json
import sys
import os
import time

sys.path.insert(0, os.path.dirname(__file__))

try:
    import websockets
except ImportError:
    print("[24CS005] ERROR: websockets not installed. Run: pip install websockets")
    sys.exit(1)

try:
    import psutil
except ImportError:
    print("[24CS005] ERROR: psutil not installed. Run: pip install psutil")
    sys.exit(1)

from ebpf_monitor import get_real_stats

# ── Optional C kernel bridge (bonus simulation engine) ────────────────
KERNEL_AVAILABLE = False
bridge = None

try:
    import kernel_bridge as bridge
    bridge.init()
    KERNEL_AVAILABLE = True
    print("[24CS005] Kernel bridge loaded successfully (LIVE+SIM mode).")
except SystemExit:
    print("[24CS005] INFO: libkernelviz.so not found — running in REAL-ONLY mode.")
    print("[24CS005] All tabs will show live Linux system data via psutil+/proc.")

CLIENTS: set = set()

# ── Scheduler state (controlled via dashboard) ────────────────────────
_sched_algo   = "Round Robin"
_sched_quantum = 3

# ── Virtual filesystem for terminal commands ──────────────────────────
_cwd = "/"
_fs_tree = {
    "/": {"type": "dir", "children": {
        "bin": {"type": "dir", "children": {
            "bash": {"type": "file", "size": 890112, "content": "binary"},
            "ls":   {"type": "file", "size": 122416, "content": "binary"},
            "cat":  {"type": "file", "size": 43280,  "content": "binary"},
            "ps":   {"type": "file", "size": 121928, "content": "binary"},
            "kill": {"type": "file", "size": 38304,  "content": "binary"},
            "top":  {"type": "file", "size": 209608, "content": "binary"},
        }},
        "etc": {"type": "dir", "children": {
            "hostname":   {"type": "file", "size": 12,  "content": "kernelviz\n"},
            "os-release": {"type": "file", "size": 256, "content": 'NAME="KernelViz OS"\nVERSION="1.0"\nID=kernelviz\nPRETTY_NAME="KernelViz OS 1.0 (CEP Edition)"\nAUTHOR="24CS005"\n'},
            "passwd":     {"type": "file", "size": 128, "content": "root:x:0:0:root:/root:/bin/bash\nstudent:x:1000:1000:24CS005:/home/student:/bin/bash\n"},
            "fstab":      {"type": "file", "size": 64,  "content": "# KernelViz fstab\n/dev/sda1  /     ext4  defaults  0 1\ntmpfs      /tmp  tmpfs defaults  0 0\n"},
        }},
        "home": {"type": "dir", "children": {
            "student": {"type": "dir", "children": {
                "readme.txt": {"type": "file", "size": 128, "content": "Welcome to KernelViz OS!\nCEP Project by 24CS005\nMUET Jamshoro, CS-261 Operating Systems\nInstructor: Engr. Zoha Memon\n"},
            }}
        }},
        "proc": {"type": "dir", "children": {
            "cpuinfo": {"type": "file", "size": 512, "content": "processor\t: 0\nmodel name\t: Real Linux CPU\n"},
            "meminfo": {"type": "file", "size": 256, "content": "MemTotal: (live)\nMemFree: (live)\n"},
            "version": {"type": "file", "size": 128, "content": "KernelViz 1.0.0-CEP (CEP Edition)\n"},
        }},
        "var":  {"type": "dir", "children": {"log": {"type": "dir", "children": {
            "syslog": {"type": "file", "size": 4096, "content": "Apr 25 00:00:01 kernelviz kernel: KernelViz OS booted\n"},
        }}}},
        "tmp": {"type": "dir", "children": {}},
        "dev": {"type": "dir", "children": {
            "null": {"type": "file", "size": 0, "content": ""},
            "zero": {"type": "file", "size": 0, "content": ""},
        }},
    }}
}

def _get_node(path):
    path = os.path.normpath(path)
    parts = [p for p in path.split("/") if p]
    node = _fs_tree["/"]
    for part in parts:
        if node.get("type") != "dir" or part not in node.get("children", {}):
            return None
        node = node["children"][part]
    return node

def _resolve(path):
    global _cwd
    if not path:
        return _cwd
    if path.startswith("/"):
        return os.path.normpath(path)
    return os.path.normpath(os.path.join(_cwd, path))


# ── Terminal command handler ──────────────────────────────────────────
def handle_terminal_command(raw_cmd: str) -> str:
    global _cwd, _sched_algo, _sched_quantum
    parts = raw_cmd.strip().split()
    if not parts:
        return ""
    cmd = parts[0]
    args = parts[1:]

    if cmd == "help":
        return (
            "\033[1;33mKernelViz OS Shell Commands\033[0m\n"
            "  \033[1;32mProcess:\033[0m  ps, top, kill <pid>\n"
            "  \033[1;32mFilesystem:\033[0m ls [path], cd <dir>, pwd, cat <file>,\n"
            "             mkdir <dir>, touch <file>, rm <file>\n"
            "  \033[1;32mMemory:\033[0m   free, meminfo\n"
            "  \033[1;32mSystem:\033[0m   uname -a, uptime, date, hostname,\n"
            "             whoami, lscpu, df, mount, dmesg, ifconfig\n"
            "  \033[1;32mKernel:\033[0m   sched, algo <rr|priority>, quantum <n>\n"
            "  \033[1;32mShell:\033[0m    echo, clear, help\n"
        )
    elif cmd == "clear":
        return "\x1b[CLEAR]"
    elif cmd == "echo":
        return " ".join(args) + "\n"
    elif cmd == "pwd":
        return _cwd + "\n"
    elif cmd == "whoami":
        return "root\n"
    elif cmd == "id":
        return "uid=0(root) gid=0(root) groups=0(root)\n"
    elif cmd == "hostname":
        return "kernelviz\n"
    elif cmd == "date":
        return time.strftime("%a %b %d %H:%M:%S UTC %Y") + "\n"
    elif cmd == "uname":
        flags = " ".join(args)
        if "-a" in flags:
            import platform
            uname = platform.uname()
            return f"KernelViz 1.0.0-CEP #{uname.node} SMP PREEMPT {uname.machine} GNU/Linux\n"
        elif "-r" in flags:
            return "1.0.0-CEP\n"
        return "KernelViz\n"
    elif cmd == "uptime":
        bt = psutil.boot_time()
        up = int(time.time() - bt)
        h, rem = divmod(up, 3600)
        m = rem // 60
        la = psutil.getloadavg()
        return f" {time.strftime('%H:%M:%S')}  up {h}:{m:02d},  1 user,  load average: {la[0]:.2f}, {la[1]:.2f}, {la[2]:.2f}\n"
    elif cmd == "env":
        return "PATH=/usr/bin:/bin:/usr/sbin\nHOME=/root\nSHELL=/bin/bash\nUSER=root\nTERM=xterm-256color\nLANG=en_US.UTF-8\nOS=KernelViz\n"
    elif cmd == "ls":
        target = _resolve(args[0]) if args else _cwd
        node = _get_node(target)
        if node is None:
            return f"ls: cannot access '{target}': No such file or directory\n"
        if node["type"] == "file":
            return os.path.basename(target) + "\n"
        items = list(node.get("children", {}).items())
        if not items:
            return "(empty)\n"
        out = []
        for name, child in sorted(items):
            if child["type"] == "dir":
                out.append(f"\033[1;34m{name}/\033[0m")
            else:
                out.append(f"\033[0;32m{name}\033[0m")
        return "  ".join(out) + "\n"
    elif cmd == "cd":
        target = _resolve(args[0] if args else "/home/student")
        node = _get_node(target)
        if node is None:
            return f"cd: {target}: No such file or directory\n"
        if node["type"] != "dir":
            return f"cd: {target}: Not a directory\n"
        _cwd = target
        return ""
    elif cmd == "cat":
        if not args:
            return "cat: missing file operand\n"
        target = _resolve(args[0])
        node = _get_node(target)
        if node is None:
            return f"cat: {args[0]}: No such file or directory\n"
        if node["type"] == "dir":
            return f"cat: {args[0]}: Is a directory\n"
        content = node.get("content", "")
        if content == "binary":
            return f"{args[0]}: binary file\n"
        return content if content else ""
    elif cmd == "mkdir":
        if not args:
            return "mkdir: missing operand\n"
        target = _resolve(args[0])
        parent_path = os.path.dirname(target)
        parent = _get_node(parent_path)
        if parent is None or parent["type"] != "dir":
            return f"mkdir: cannot create directory '{args[0]}': No such file or directory\n"
        name = os.path.basename(target)
        if name in parent.get("children", {}):
            return f"mkdir: cannot create directory '{args[0]}': File exists\n"
        parent["children"][name] = {"type": "dir", "children": {}}
        return ""
    elif cmd == "touch":
        if not args:
            return "touch: missing file operand\n"
        target = _resolve(args[0])
        parent = _get_node(os.path.dirname(target))
        if parent is None:
            return f"touch: cannot touch '{args[0]}': No such file or directory\n"
        name = os.path.basename(target)
        if name not in parent.get("children", {}):
            parent["children"][name] = {"type": "file", "size": 0, "content": ""}
        return ""
    elif cmd == "rm":
        if not args:
            return "rm: missing operand\n"
        target = _resolve(args[0])
        parent = _get_node(os.path.dirname(target))
        name = os.path.basename(target)
        if parent is None or name not in parent.get("children", {}):
            return f"rm: cannot remove '{args[0]}': No such file or directory\n"
        del parent["children"][name]
        return ""
    elif cmd == "free":
        mem  = psutil.virtual_memory()
        swap = psutil.swap_memory()
        def kb(b): return b // 1024
        return (
            f"{'':>16}{'total':>12}{'used':>12}{'free':>12}{'available':>12}\n"
            f"{'Mem:':>16}{kb(mem.total):>12}{kb(mem.used):>12}{kb(mem.free):>12}{kb(mem.available):>12}\n"
            f"{'Swap:':>16}{kb(swap.total):>12}{kb(swap.used):>12}{kb(swap.free):>12}\n"
        )
    elif cmd == "meminfo":
        mem = psutil.virtual_memory()
        return (
            f"MemTotal:      {mem.total//1024:>10} kB\n"
            f"MemFree:       {mem.free//1024:>10} kB\n"
            f"MemAvailable:  {mem.available//1024:>10} kB\n"
            f"MemUsed:       {mem.used//1024:>10} kB\n"
            f"MemPercent:    {mem.percent:>9.1f} %\n"
        )
    elif cmd == "ps":
        lines = ["\033[1;37m  PID  STAT  TIME CMD\033[0m\n"]
        try:
            for p in list(psutil.process_iter(["pid", "name", "status", "cpu_times"]))[:20]:
                try:
                    ct = p.info.get("cpu_times")
                    t = int(ct.user + ct.system) if ct else 0
                    m, s = divmod(t, 60)
                    lines.append(f"{p.info['pid']:>5}  {(p.info.get('status','?') or '?')[:4]:<4}  {m:02d}:{s:02d} {p.info['name']}\n")
                except Exception:
                    pass
        except Exception:
            lines.append("(unavailable)\n")
        return "".join(lines)
    elif cmd == "top":
        try:
            procs = []
            for p in psutil.process_iter(["pid", "name", "cpu_percent", "memory_percent", "status"]):
                try:
                    procs.append(p.info)
                except Exception:
                    pass
            procs.sort(key=lambda x: x.get("cpu_percent") or 0, reverse=True)
            lines = [f"\033[1;33m{'PID':>5}  {'CPU%':>5}  {'MEM%':>5}  {'STATUS':<10}  NAME\033[0m\n"]
            for p in procs[:15]:
                lines.append(f"{p['pid']:>5}  {(p.get('cpu_percent') or 0):>5.1f}  {(p.get('memory_percent') or 0):>5.1f}  {(p.get('status') or '?'):<10}  {p['name']}\n")
            return "".join(lines)
        except Exception:
            return "(top unavailable)\n"
    elif cmd == "kill":
        if not args:
            return "kill: usage: kill <pid>\n"
        try:
            pid = int(args[0])
            if KERNEL_AVAILABLE and bridge:
                bridge.kill_process(pid)
            return f"[kernel] signal sent to PID {pid}\n"
        except ValueError:
            return f"kill: {args[0]}: invalid pid\n"
    elif cmd == "lscpu":
        freq = psutil.cpu_freq()
        mhz = round(freq.current, 2) if freq else "N/A"
        return (
            f"Architecture:      x86_64\n"
            f"CPU(s):            {psutil.cpu_count(logical=True)}\n"
            f"Core(s)/socket:    {psutil.cpu_count(logical=False)}\n"
            f"CPU MHz:           {mhz}\n"
            f"Model:             KernelViz Simulated CPU\n"
        )
    elif cmd == "df":
        lines = ["\033[1;37mFilesystem      Size  Used  Avail  Use%  Mounted on\033[0m\n"]
        for part in psutil.disk_partitions():
            try:
                u = psutil.disk_usage(part.mountpoint)
                lines.append(f"{part.device:<16}{u.total//1073741824}G  {u.used//1073741824}G  {u.free//1073741824}G  {u.percent:>4.0f}%  {part.mountpoint}\n")
            except Exception:
                pass
        return "".join(lines)
    elif cmd == "mount":
        lines = []
        for part in psutil.disk_partitions():
            lines.append(f"{part.device} on {part.mountpoint} type {part.fstype} (defaults)\n")
        return "".join(lines) or "(no mounts)\n"
    elif cmd == "dmesg":
        return (
            "[\033[1;32m    0.000000\033[0m] KernelViz 1.0.0-CEP booting...\n"
            "[\033[1;32m    0.000001\033[0m] CEP OS — MUET Jamshoro — CS-261\n"
            "[\033[1;32m    0.000100\033[0m] Author: 24CS005 | Engr. Zoha Memon\n"
            "[\033[1;32m    0.001000\033[0m] CPU: x86_64, Round Robin scheduler active\n"
            "[\033[1;32m    0.002000\033[0m] Memory: real Linux RAM initialized\n"
            "[\033[1;32m    0.003000\033[0m] First-Fit allocator ready\n"
            "[\033[1;32m    0.004000\033[0m] VFS: real Linux filesystem mounted\n"
            "[\033[1;32m    0.005000\033[0m] IPC: real pipe + socket counts from /proc\n"
            "[\033[1;32m    0.006000\033[0m] Processes: real Linux processes enumerated\n"
            "[\033[1;32m    0.007000\033[0m] WebSocket bridge active on :8765\n"
            "[\033[1;32m    0.008000\033[0m] KernelViz OS ready.\n"
        )
    elif cmd == "ifconfig":
        lines = []
        try:
            addrs = psutil.net_if_addrs()
            stats = psutil.net_if_stats()
            for iface, addr_list in addrs.items():
                s = stats.get(iface)
                lines.append(f"\033[1;32m{iface}\033[0m: flags={'UP' if s and s.isup else 'DOWN'}  mtu={s.mtu if s else '?'}\n")
                for a in addr_list:
                    if a.family == 2:   # AF_INET
                        lines.append(f"        inet {a.address}  netmask {a.netmask}\n")
                    elif a.family == 17: # AF_PACKET
                        lines.append(f"        ether {a.address}\n")
                lines.append("\n")
        except Exception:
            lines.append("(ifconfig unavailable)\n")
        return "".join(lines) or "(no interfaces)\n"
    elif cmd == "sched":
        return (
            f"\033[1;33mScheduler Status\033[0m\n"
            f"  Algorithm: {_sched_algo}\n"
            f"  Quantum:   {_sched_quantum} ticks\n"
            f"  Use 'algo rr' or 'algo priority' to switch\n"
            f"  Use 'quantum <n>' to change time quantum\n"
        )
    elif cmd == "algo":
        if not args:
            return "algo: usage: algo <rr|priority>\n"
        if args[0] == "rr":
            _sched_algo = "Round Robin"
            if KERNEL_AVAILABLE and bridge:
                bridge.set_algorithm(0)
            return "scheduler: switched to Round Robin\n"
        elif args[0] in ("priority", "pri"):
            _sched_algo = "Priority"
            if KERNEL_AVAILABLE and bridge:
                bridge.set_algorithm(1)
            return "scheduler: switched to Priority scheduling\n"
        return f"algo: unknown: '{args[0]}'\n"
    elif cmd == "quantum":
        if not args:
            return "quantum: usage: quantum <1-20>\n"
        try:
            q = int(args[0])
            if not 1 <= q <= 20:
                return "quantum: value must be 1-20\n"
            _sched_quantum = q
            if KERNEL_AVAILABLE and bridge:
                bridge.set_quantum(q)
            return f"scheduler: quantum set to {q} ticks\n"
        except ValueError:
            return f"quantum: invalid: '{args[0]}'\n"
    elif cmd in ("exit", "logout"):
        return "\033[1;31mlogout\033[0m\n"
    else:
        return f"\033[0;31m-bash: {cmd}: command not found\033[0m\n"


# ── Build broadcast payload ───────────────────────────────────────────
def build_payload() -> str:
    """
    Build the JSON payload for all dashboard tabs.
    Uses real system data via get_real_stats(); optionally merges
    kernel bridge state on top if the C library is available.
    """
    real = get_real_stats()

    # Overlay scheduler algo name from our controlled state
    real["scheduler"]["algorithm"] = _sched_algo
    real["scheduler"]["quantum"]   = _sched_quantum

    if KERNEL_AVAILABLE and bridge:
        # Merge C-sim gantt / scheduler stats on top of real process list
        try:
            ks = bridge.tick()
            real["gantt"]     = ks.get("gantt", real["gantt"])
            real["deadlock"]  = ks.get("deadlock", real["deadlock"])
        except Exception:
            pass

    return json.dumps(real)


# ── Command handler ───────────────────────────────────────────────────
def handle_command(cmd_obj: dict):
    global _sched_algo, _sched_quantum
    cmd = cmd_obj.get("cmd", "")
    if cmd == "terminal":
        raw = cmd_obj.get("input", "")
        return json.dumps({"type": "terminal_output", "output": handle_terminal_command(raw)})
    elif cmd == "add_process":
        name     = cmd_obj.get("name", "proc")[:31]
        burst    = int(cmd_obj.get("burst", 5))
        priority = int(cmd_obj.get("priority", 5))
        mem      = int(cmd_obj.get("memory", 32))
        pid = bridge.create_process(name, burst, priority, mem) if KERNEL_AVAILABLE and bridge else -1
        return json.dumps({"type": "process_created", "pid": pid, "name": name})
    elif cmd == "kill_process":
        pid = int(cmd_obj.get("pid", -1))
        if KERNEL_AVAILABLE and bridge and pid > 0:
            bridge.kill_process(pid)
        return json.dumps({"type": "process_killed", "pid": pid})
    elif cmd == "set_algorithm":
        algo = int(cmd_obj.get("algo", 0))
        _sched_algo = "Priority" if algo == 1 else "Round Robin"
        if KERNEL_AVAILABLE and bridge:
            bridge.set_algorithm(algo)
    elif cmd == "set_quantum":
        q = int(cmd_obj.get("quantum", 3))
        _sched_quantum = q
        if KERNEL_AVAILABLE and bridge:
            bridge.set_quantum(q)
    return None


# ── WebSocket handler ─────────────────────────────────────────────────
async def handler(websocket):
    CLIENTS.add(websocket)
    print(f"[24CS005] Client connected: {websocket.remote_address}")
    try:
        async for message in websocket:
            try:
                cmd_obj = json.loads(message)
                resp = handle_command(cmd_obj)
                if resp:
                    await websocket.send(resp)
            except json.JSONDecodeError:
                pass
    except websockets.exceptions.ConnectionClosedError:
        pass
    finally:
        CLIENTS.discard(websocket)
        print(f"[24CS005] Client disconnected: {websocket.remote_address}")


async def broadcast_loop():
    while True:
        if CLIENTS:
            try:
                payload = build_payload()
            except Exception as e:
                print(f"[24CS005] ERROR building payload: {e}")
                await asyncio.sleep(1.0)
                continue
            dead = set()
            for ws in list(CLIENTS):
                try:
                    await ws.send(payload)
                except Exception:
                    dead.add(ws)
            CLIENTS -= dead
        await asyncio.sleep(1.0)


async def main():
    host = "localhost"
    port = 8765
    print(f"[24CS005] KernelViz WebSocket server on ws://{host}:{port}")
    print(f"[24CS005] Mode: {'LIVE+SIM' if KERNEL_AVAILABLE else 'REAL-ONLY (all tabs show real Linux data)'}")
    async with websockets.serve(handler, host, port):
        await broadcast_loop()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[24CS005] Server stopped.")
