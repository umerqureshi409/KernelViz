"""
kernel_bridge.py — ctypes wrapper around libkernelviz.so
KernelViz OS Simulation Bridge Layer

Author/Roll: 24CS005
CEP R5 (Beyond Standards): This bridge connects a compiled C simulation
engine to Python in real-time, going beyond typical student projects that
stay entirely within one language.
"""

import ctypes
import json
import os
import sys

# Resolve shared library path relative to this file
_LIB_DIR = os.path.join(os.path.dirname(__file__), "..", "core")
_LIB_PATH = os.path.join(_LIB_DIR, "libkernelviz.so")

# ── Load shared library ────────────────────────────────────────────────
try:
    _lib = ctypes.CDLL(_LIB_PATH)
except OSError as e:
    print(f"[24CS005] ERROR: Cannot load {_LIB_PATH}: {e}")
    print("  Run: cd core && make")
    sys.exit(1)

# ── Opaque kernel handle (we pass a pointer to allocated Kernel struct) ─
# The Kernel struct is large (~200 KB); allocate a buffer big enough.
# We determine size from the C header constants:
#   MAX_PROCESSES=50, MAX_BLOCKS=128, MAX_INODES=100, MAX_PIPES=16, MAX_MQ=8
# Conservative buffer: 2 MB covers the struct + state_buf
_KERNEL_BUF_SIZE = 2 * 1024 * 1024

# ── Declare C function signatures ─────────────────────────────────────

# void kernel_init(Kernel *k)
_lib.kernel_init.argtypes = [ctypes.c_void_p]
_lib.kernel_init.restype  = None

# const char* kernel_tick(Kernel *k)
_lib.kernel_tick.argtypes = [ctypes.c_void_p]
_lib.kernel_tick.restype  = ctypes.c_char_p

# const char* kernel_get_state(Kernel *k)
_lib.kernel_get_state.argtypes = [ctypes.c_void_p]
_lib.kernel_get_state.restype  = ctypes.c_char_p

# void kernel_shutdown(Kernel *k)
_lib.kernel_shutdown.argtypes = [ctypes.c_void_p]
_lib.kernel_shutdown.restype  = None

# int kernel_create_process(Kernel *k, const char*, int, int, int)
_lib.kernel_create_process.argtypes = [
    ctypes.c_void_p, ctypes.c_char_p,
    ctypes.c_int, ctypes.c_int, ctypes.c_int
]
_lib.kernel_create_process.restype = ctypes.c_int

# int kernel_kill_process(Kernel *k, int pid)
_lib.kernel_kill_process.argtypes = [ctypes.c_void_p, ctypes.c_int]
_lib.kernel_kill_process.restype  = ctypes.c_int

# void kernel_set_algorithm(Kernel *k, int algo)
_lib.kernel_set_algorithm.argtypes = [ctypes.c_void_p, ctypes.c_int]
_lib.kernel_set_algorithm.restype  = None

# void kernel_set_quantum(Kernel *k, int quantum)
_lib.kernel_set_quantum.argtypes = [ctypes.c_void_p, ctypes.c_int]
_lib.kernel_set_quantum.restype  = None


# ── KernelBridge class ────────────────────────────────────────────────

class KernelBridge:
    """Python wrapper around the C kernel simulation library."""

    def __init__(self):
        # Allocate raw memory buffer for the Kernel struct
        self._buf = (ctypes.c_uint8 * _KERNEL_BUF_SIZE)()
        self._ptr = ctypes.cast(self._buf, ctypes.c_void_p)
        self._ready = False

    def init(self):
        """Initialise the kernel and all subsystems."""
        _lib.kernel_init(self._ptr)
        self._ready = True
        print("[24CS005] KernelBridge: kernel_init() OK")

    def tick(self) -> dict:
        """Advance simulation by one tick. Returns parsed state dict."""
        if not self._ready:
            return {}
        raw = _lib.kernel_tick(self._ptr)
        return self._parse(raw)

    def get_state(self) -> dict:
        """Return current state snapshot without advancing time."""
        if not self._ready:
            return {}
        raw = _lib.kernel_get_state(self._ptr)
        return self._parse(raw)

    def create_process(self, name: str, burst: int, priority: int, mem_size: int) -> int:
        """Create a new process in the simulation. Returns PID or -1."""
        if not self._ready:
            return -1
        pid = _lib.kernel_create_process(
            self._ptr,
            name.encode("utf-8"),
            ctypes.c_int(burst),
            ctypes.c_int(priority),
            ctypes.c_int(mem_size),
        )
        print(f"[24CS005] Created process '{name}' → PID {pid}")
        return pid

    def kill_process(self, pid: int) -> int:
        """Kill a process by PID. Returns 0 on success."""
        if not self._ready:
            return -1
        return _lib.kernel_kill_process(self._ptr, ctypes.c_int(pid))

    def set_algorithm(self, algo: int):
        """Set scheduling algorithm: 0=Round Robin, 1=Priority."""
        if self._ready:
            _lib.kernel_set_algorithm(self._ptr, ctypes.c_int(algo))

    def set_quantum(self, quantum: int):
        """Set RR time quantum (1–20)."""
        if self._ready:
            _lib.kernel_set_quantum(self._ptr, ctypes.c_int(quantum))

    def shutdown(self):
        """Shut down the kernel simulation."""
        if self._ready:
            _lib.kernel_shutdown(self._ptr)
            self._ready = False

    @staticmethod
    def _parse(raw: bytes) -> dict:
        """Decode C char* JSON string into a Python dict."""
        try:
            return json.loads(raw.decode("utf-8"))
        except (json.JSONDecodeError, UnicodeDecodeError) as e:
            print(f"[24CS005] JSON parse error: {e}")
            return {}


# ── Module-level singleton ────────────────────────────────────────────
_bridge = KernelBridge()


def init():
    """Initialise bridge (call once at startup)."""
    _bridge.init()


def tick() -> dict:
    """Advance one simulation tick and return state dict."""
    return _bridge.tick()


def get_state() -> dict:
    """Return current state without advancing time."""
    return _bridge.get_state()


def create_process(name, burst, priority, mem_size) -> int:
    """Convenience wrapper for process creation."""
    return _bridge.create_process(name, burst, priority, mem_size)


def kill_process(pid) -> int:
    """Convenience wrapper to kill a process."""
    return _bridge.kill_process(pid)


def set_algorithm(algo: int):
    """Switch scheduling algorithm."""
    _bridge.set_algorithm(algo)


def set_quantum(q: int):
    """Change RR quantum."""
    _bridge.set_quantum(q)
