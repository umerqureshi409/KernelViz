/**
 * app.js — KernelViz OS Dashboard Controller
 * 24CS005 | MUET Jamshoro | CS-261 Operating Systems CEP
 *
 * Handles: Boot sequence, WebSocket connection, all UI panels,
 * terminal emulator, real-time charts, process/memory/IPC/FS views.
 */

'use strict';

/* ════════════════════════════════════════════════════════════════════
   BOOT SEQUENCE
   ════════════════════════════════════════════════════════════════════ */

const BOOT_ART = `
  ██╗  ██╗███████╗██████╗ ███╗   ██╗███████╗██╗    ██╗   ██╗██╗███████╗
  ██║ ██╔╝██╔════╝██╔══██╗████╗  ██║██╔════╝██║    ██║   ██║██║╚══███╔╝
  █████╔╝ █████╗  ██████╔╝██╔██╗ ██║█████╗  ██║    ██║   ██║██║  ███╔╝ 
  ██╔═██╗ ██╔══╝  ██╔══██╗██║╚██╗██║██╔══╝  ██║    ╚██╗ ██╔╝██║ ███╔╝  
  ██║  ██╗███████╗██║  ██║██║ ╚████║███████╗███████╗╚████╔╝ ██║███████╗
  ╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝╚══════╝╚══════╝ ╚═══╝  ╚═╝╚══════╝
                     OS SIMULATION v1.0 — CEP Edition
              MUET Jamshoro · CS-261 · Author: 24CS005
`;

const BOOT_MESSAGES = [
  { text: '[    0.000000] KernelViz 1.0.0-CEP #1 SMP PREEMPT x86_64 GNU/Linux', cls: 'info', pct: 2 },
  { text: '[    0.000100] Command line: BOOT_IMAGE=/vmlinuz-kviz root=/dev/sda1', cls: '', pct: 5 },
  { text: '[    0.001000] BIOS-provided physical RAM map:', cls: '', pct: 8 },
  { text: '[    0.001100]   BIOS-e820: [mem 0x0000000000000000-0x000000003fffefff] usable', cls: '', pct: 10 },
  { text: '[    0.002000] ACPI: IRQ0 used by override', cls: '', pct: 12 },
  { text: '[    0.003000] PCI: Using configuration type 1 for base access', cls: '', pct: 15 },
  { text: '[    0.010000] Kernel memory allocator: First-Fit initialized', cls: 'ok', pct: 18 },
  { text: '[    0.012000] Memory: 1024MB total, 64MB reserved', cls: 'ok', pct: 22 },
  { text: '[    0.015000] CPU: x86_64 processor detected', cls: 'ok', pct: 26 },
  { text: '[    0.016000] Calibrating delay loop... 2400.00 BogoMIPS', cls: '', pct: 29 },
  { text: '[    0.020000] Scheduler: Round Robin (quantum=3 ticks) ready', cls: 'ok', pct: 33 },
  { text: '[    0.021000] Scheduler: Priority-based mode also available', cls: '', pct: 36 },
  { text: '[    0.025000] VFS: Mounting root filesystem (ext4)', cls: 'ok', pct: 40 },
  { text: '[    0.026000] EXT4: mounted /dev/sda1 at / with journaling', cls: 'ok', pct: 43 },
  { text: '[    0.030000] IPC: Anonymous pipe subsystem initialized', cls: 'ok', pct: 47 },
  { text: '[    0.031000] IPC: POSIX message queue subsystem initialized', cls: 'ok', pct: 50 },
  { text: '[    0.035000] IPC: Deadlock detection (cycle algorithm) ready', cls: 'ok', pct: 53 },
  { text: '[    0.040000] Starting init process [PID 1]', cls: 'ok', pct: 57 },
  { text: '[    0.041000] init: systemd[2] started', cls: 'ok', pct: 60 },
  { text: '[    0.042000] init: sshd[3] started', cls: 'ok', pct: 63 },
  { text: '[    0.043000] init: nginx[4] started', cls: 'ok', pct: 66 },
  { text: '[    0.044000] init: python3[5] started', cls: 'ok', pct: 69 },
  { text: '[    0.050000] Process table: 5 initial processes spawned', cls: 'ok', pct: 72 },
  { text: '[    0.055000] Memory map: blocks allocated for all processes', cls: 'ok', pct: 75 },
  { text: '[    0.060000] WebSocket bridge: listening on ws://localhost:8765', cls: 'ok', pct: 80 },
  { text: '[    0.065000] psutil bridge: real system stats collection ready', cls: 'ok', pct: 85 },
  { text: '[    0.070000] KernelViz OS: all subsystems operational', cls: 'ok', pct: 90 },
  { text: '', cls: '', pct: 93 },
  { text: 'KernelViz OS 1.0 (CEP Edition) kernelviz tty1', cls: 'info', pct: 96 },
  { text: 'kernelviz login: root (automatic)', cls: 'info', pct: 98 },
  { text: 'Last login: ' + new Date().toUTCString(), cls: '', pct: 99 },
  { text: '', cls: '', pct: 100 },
];

function runBootSequence() {
  const bootScreen = document.getElementById('boot-screen');
  const bootLog    = document.getElementById('boot-log');
  const bootBar    = document.getElementById('boot-bar');
  const bootPct    = document.getElementById('boot-pct');
  const bootArt    = document.getElementById('boot-art');

  bootArt.textContent = BOOT_ART;

  let idx = 0;
  function step() {
    if (idx >= BOOT_MESSAGES.length) {
      setTimeout(() => {
        bootScreen.classList.add('fade-out');
        setTimeout(() => {
          bootScreen.style.display = 'none';
          // Show login screen instead of OS directly
          if (typeof window.showLogin === 'function') {
            window.showLogin();
          }
        }, 700);
      }, 300);
      return;
    }
    const msg = BOOT_MESSAGES[idx++];
    if (msg.text) {
      const div = document.createElement('div');
      div.className = 'line ' + (msg.cls || '');
      div.textContent = msg.text;
      bootLog.appendChild(div);
      bootLog.scrollTop = bootLog.scrollHeight;
    }
    bootBar.style.width = msg.pct + '%';
    bootPct.textContent = msg.pct + '%';
    setTimeout(step, 60 + Math.random() * 40);
  }
  step();
}

/* ════════════════════════════════════════════════════════════════════
   OS INIT & STATE
   ════════════════════════════════════════════════════════════════════ */

let ws = null;
let wsConnected = false;
let state = {};
let cpuHistory = new Array(60).fill(0);
let uptime = 0;
let currentAlgo = 0; // 0=RR, 1=Priority
let _startTime = Date.now();

const PROC_COLORS = [
  '#00d4ff','#00ff88','#ffd700','#ff8c00','#ff3860',
  '#9b59b6','#3498db','#1abc9c','#e67e22','#e74c3c',
  '#2ecc71','#f39c12','#16a085','#8e44ad','#d35400',
];

function pidColor(pid) {
  return PROC_COLORS[(pid - 1) % PROC_COLORS.length];
}

/* ════════════════════════════════════════════════════════════════════
   WINDOW TABS
   ════════════════════════════════════════════════════════════════════ */

function initTabs() {
  document.querySelectorAll('.win-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      document.querySelectorAll('.win-tab').forEach(t => t.classList.remove('active'));
      document.querySelectorAll('.window').forEach(w => w.classList.remove('active'));
      tab.classList.add('active');
      const win = document.getElementById('win-' + tab.dataset.win);
      if (win) win.classList.add('active');
    });
  });
}

/* ════════════════════════════════════════════════════════════════════
   WEBSOCKET
   ════════════════════════════════════════════════════════════════════ */

function connectWS() {
  const indicator = document.getElementById('conn-indicator');
  const modeBadge = document.getElementById('mode-badge');
  indicator.className = 'conn-indicator connecting';
  indicator.textContent = '◉ CONNECTING';

  try {
    ws = new WebSocket('ws://localhost:8765');

    ws.onopen = () => {
      wsConnected = true;
      indicator.className = 'conn-indicator connected';
      indicator.textContent = '◉ LIVE';
      modeBadge.classList.remove('demo');
      modeBadge.textContent = '● LIVE';
      termPrint('system', '[WebSocket] Connected to KernelViz kernel bridge.\n');
    };

    ws.onmessage = (e) => {
      try {
        const data = JSON.parse(e.data);
        if (data.type === 'terminal_output') {
          handleTerminalResponse(data.output);
          return;
        }
        state = data;
        updateUI();
      } catch (err) {
        console.error('WS parse error:', err);
      }
    };

    ws.onerror = () => {
      wsConnected = false;
      indicator.className = 'conn-indicator disconnected';
      indicator.textContent = '◉ OFFLINE';
      modeBadge.classList.add('demo');
      modeBadge.textContent = '● DEMO';
    };

    ws.onclose = () => {
      wsConnected = false;
      indicator.className = 'conn-indicator disconnected';
      indicator.textContent = '◉ OFFLINE';
      termPrint('system', '[WebSocket] Disconnected. Retrying in 3s...\n');
      setTimeout(connectWS, 3000);
    };
  } catch (e) {
    indicator.className = 'conn-indicator disconnected';
    indicator.textContent = '◉ OFFLINE';
    setTimeout(connectWS, 5000);
  }
}

function sendCmd(obj) {
  if (ws && wsConnected) {
    ws.send(JSON.stringify(obj));
  }
}

/* ════════════════════════════════════════════════════════════════════
   CLOCK & UPTIME
   ════════════════════════════════════════════════════════════════════ */

function startClock() {
  setInterval(() => {
    const now = new Date();
    const hms = now.toTimeString().slice(0, 8);
    document.getElementById('live-clock').textContent = hms;

    uptime = Math.floor((Date.now() - _startTime) / 1000);
    const h = Math.floor(uptime / 3600);
    const m = Math.floor((uptime % 3600) / 60);
    const s = uptime % 60;
    document.getElementById('d-uptime').textContent =
      h > 0 ? `${h}h${m}m${s}s` : m > 0 ? `${m}m${s}s` : `${s}s`;
  }, 1000);
}

/* ════════════════════════════════════════════════════════════════════
   UPDATE UI — MAIN DISPATCH
   ════════════════════════════════════════════════════════════════════ */

function updateUI() {
  if (!state) return;
  updateHeader();
  updateDashboard();
  updateProcessTable();
  updateMemory();
  updateScheduler();
  updateFilesystem();
  updateIPC();
  updateRealStats();
}

/* ─── HEADER ────────────────────────────────────────────────────────── */
function updateHeader() {
  const s   = state.scheduler || {};
  const rs  = state.real_stats || {};
  const procs = (state.processes || []).filter(p => p.state !== 'TERMINATED');

  // Use real system values
  const cpu    = rs.cpu_total !== undefined ? rs.cpu_total : (s.cpu_utilization || 0);
  const memObj = rs.memory || {};
  const memPct = memObj.percent !== undefined
    ? Math.round(memObj.percent)
    : Math.round(((state.memory_stats?.used || 0) / (state.memory_stats?.total || 1024)) * 100);

  set('hdr-tick',  state.tick || 0);
  set('hdr-cpu',   cpu.toFixed(1) + '%');
  set('hdr-mem',   memPct + '%');
  set('hdr-procs', procs.length);
  set('hdr-algo',  s.algorithm === 'Priority' ? 'PRI' : 'RR');
}

/* ─── DASHBOARD ─────────────────────────────────────────────────────── */
function updateDashboard() {
  const s   = state.scheduler  || {};
  const m   = state.memory_stats || {};
  const rs  = state.real_stats  || {};
  const procs = state.processes || [];
  const ipc   = state.ipc_stats || {};

  // ── Prefer real CPU % from psutil; fall back to sim value ──────────
  const cpu    = rs.cpu_total !== undefined ? rs.cpu_total : (s.cpu_utilization || 0);
  const memObj = rs.memory || {};
  const memPct = memObj.percent !== undefined
    ? memObj.percent
    : Math.round(((m.used || 0) / (m.total || 1024)) * 100);

  const memUsedMB  = memObj.used  ? Math.round(memObj.used  / 1048576) : (m.used  || 0);
  const memTotalMB = memObj.total ? Math.round(memObj.total / 1048576) : (m.total || 1024);

  // CPU ring — real value
  drawRing('cpu-ring', cpu, '#00d4ff');
  set('ring-cpu-val', cpu.toFixed(1));
  set('ring-algo-name', s.algorithm === 'Priority' ? 'Priority Scheduling' : 'Round Robin');

  // Memory ring — real value
  drawRing('mem-ring', memPct, '#00ff88');
  set('ring-mem-val', Math.round(memPct));
  set('ring-mem-used', memUsedMB);
  set('ring-mem-total', memTotalMB);

  // CPU history chart — real CPU
  cpuHistory.push(cpu);
  if (cpuHistory.length > 60) cpuHistory.shift();
  drawSparkline('cpu-history', cpuHistory, '#00d4ff', '#00ff88');

  // Process states pie chart
  const states = { RUNNING: 0, READY: 0, WAITING: 0, NEW: 0, TERMINATED: 0 };
  procs.forEach(p => { if (states[p.state] !== undefined) states[p.state]++; });
  drawPieChart('state-chart', states);
  renderStateLegend(states);

  // Metrics from real scheduler
  set('avg-wait',        (s.avg_wait_time || 0).toFixed(2) + ' ticks');
  set('avg-ta',          (s.avg_turnaround_time || 0).toFixed(2) + ' ticks');
  set('completed-procs',  s.completed || 0);
  set('algo-display',    s.algorithm === 'Priority' ? 'Priority' : 'Round Robin');
  set('quantum-display',  s.quantum || 3);
  set('current-pid',     s.current_pid > 0 ? `PID ${s.current_pid}` : 'IDLE');

  // IPC summary
  set('d-pipes',     ipc.active_pipes  || 0);
  set('d-queues',    ipc.active_queues || 0);
  set('d-ipc-total', ipc.total_events  || 0);
  const dl = state.deadlock;
  set('d-deadlock', dl ? '⚠ YES' : 'NONE');
  const dlEl = document.getElementById('d-deadlock');
  if (dlEl) dlEl.style.color = dl ? 'var(--red)' : 'var(--green)';

  // System stats
  set('d-tick',    state.tick || 0);
  set('d-procs',   procs.length);
  set('d-blocks',  m.block_count || 0);
  set('d-frag',    (m.fragmentation || 0).toFixed(1) + '%');
  set('d-real-cpu', cpu.toFixed(1) + '%');
}

/* ─── RING CHART ────────────────────────────────────────────────────── */
function drawRing(id, pct, color) {
  const canvas = document.getElementById(id);
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, r = (W < H ? W : H) * 0.38;
  const lineW = 14;

  ctx.clearRect(0, 0, W, H);

  // Track
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.strokeStyle = '#1a2232';
  ctx.lineWidth = lineW;
  ctx.stroke();

  // Arc
  const angle = (pct / 100) * Math.PI * 2 - Math.PI / 2;
  ctx.beginPath();
  ctx.arc(cx, cy, r, -Math.PI / 2, angle);
  ctx.strokeStyle = color;
  ctx.lineWidth = lineW;
  ctx.lineCap = 'round';
  ctx.shadowColor = color;
  ctx.shadowBlur = 12;
  ctx.stroke();
  ctx.shadowBlur = 0;
}

/* ─── SPARKLINE ─────────────────────────────────────────────────────── */
function drawSparkline(id, data, color1, color2) {
  const canvas = document.getElementById(id);
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  ctx.clearRect(0, 0, W, H);

  if (data.length < 2) return;
  const max = Math.max(...data, 1);
  const step = W / (data.length - 1);

  // Grid lines
  ctx.strokeStyle = '#1a2232';
  ctx.lineWidth = 1;
  [25, 50, 75].forEach(y => {
    const yPos = H - (y / 100) * H;
    ctx.beginPath();
    ctx.moveTo(0, yPos);
    ctx.lineTo(W, yPos);
    ctx.stroke();
  });

  // Fill
  const grad = ctx.createLinearGradient(0, 0, W, 0);
  grad.addColorStop(0, color1 + '33');
  grad.addColorStop(1, color2 + '33');
  ctx.beginPath();
  ctx.moveTo(0, H);
  data.forEach((v, i) => {
    ctx.lineTo(i * step, H - (v / max) * H * 0.9);
  });
  ctx.lineTo((data.length - 1) * step, H);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Line
  const lineGrad = ctx.createLinearGradient(0, 0, W, 0);
  lineGrad.addColorStop(0, color1);
  lineGrad.addColorStop(1, color2);
  ctx.beginPath();
  data.forEach((v, i) => {
    const x = i * step;
    const y = H - (v / max) * H * 0.9;
    i === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y);
  });
  ctx.strokeStyle = lineGrad;
  ctx.lineWidth = 2;
  ctx.shadowColor = color1;
  ctx.shadowBlur = 8;
  ctx.stroke();
  ctx.shadowBlur = 0;
}

/* ─── PIE CHART ─────────────────────────────────────────────────────── */
const STATE_COLORS = {
  RUNNING: '#00ff88', READY: '#00d4ff', WAITING: '#ff8c00',
  NEW: '#9b59b6', TERMINATED: '#333'
};

function drawPieChart(id, states) {
  const canvas = document.getElementById(id);
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2 + 5;
  const r = Math.min(W, H) * 0.35;

  ctx.clearRect(0, 0, W, H);

  const total = Object.values(states).reduce((a, b) => a + b, 0);
  if (total === 0) return;

  let start = -Math.PI / 2;
  Object.entries(states).forEach(([s, count]) => {
    if (count === 0) return;
    const angle = (count / total) * Math.PI * 2;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, start, start + angle);
    ctx.closePath();
    ctx.fillStyle = STATE_COLORS[s] || '#555';
    ctx.fill();
    ctx.strokeStyle = '#07090f';
    ctx.lineWidth = 2;
    ctx.stroke();
    start += angle;
  });
}

function renderStateLegend(states) {
  const el = document.getElementById('state-legend');
  if (!el) return;
  el.innerHTML = Object.entries(states)
    .filter(([,c]) => c > 0)
    .map(([s, c]) => `<div class="sl-item"><span class="sl-dot" style="background:${STATE_COLORS[s]}"></span>${s}: ${c}</div>`)
    .join('');
}

/* ─── PROCESS TABLE ─────────────────────────────────────────────────── */
function updateProcessTable() {
  const tbody = document.getElementById('proc-tbody');
  if (!tbody) return;
  const procs = (state.processes || []).filter(p => p.state !== 'TERMINATED');
  const s = state.scheduler || {};

  tbody.innerHTML = procs.map(p => {
    const isCurrent = p.pid === s.current_pid;
    const rowClass = p.state === 'RUNNING' ? 'row-running' : p.state === 'WAITING' ? 'row-waiting' : '';
    const color = pidColor(p.pid);
    return `<tr class="${rowClass}">
      <td><span style="color:${color};font-weight:600">${p.pid}</span></td>
      <td>${p.name}${isCurrent ? ' <span style="color:var(--green)">▶</span>' : ''}</td>
      <td><span class="state-badge state-${(p.state||'').toLowerCase()}">${p.state}</span></td>
      <td>${p.priority}</td>
      <td>${p.memory_size} MB</td>
      <td>${p.burst_time}</td>
      <td><span style="color:var(--yellow)">${p.remaining_time}</span></td>
      <td>${p.wait_time}</td>
      <td><button class="btn-danger btn-sm" onclick="killProcess(${p.pid})">KILL</button></td>
    </tr>`;
  }).join('');

  // Deadlock alert
  const alert = document.getElementById('deadlock-alert');
  if (alert) alert.classList.toggle('hidden', !state.deadlock);
}

window.killProcess = function(pid) {
  sendCmd({ cmd: 'kill_process', pid });
  termPrint('system', `[kernel] kill signal sent to PID ${pid}\n`);
};

/* ─── SPAWN PROCESS FORM ─────────────────────────────────────────────── */
function initSpawnForm() {
  const btn = document.getElementById('btn-add-proc');
  const form = document.getElementById('add-proc-form');
  const cancel = document.getElementById('btn-spawn-cancel');
  const submit = document.getElementById('btn-spawn-submit');

  if (!btn) return;

  btn.addEventListener('click', () => form.classList.toggle('hidden'));
  cancel?.addEventListener('click', () => form.classList.add('hidden'));
  submit?.addEventListener('click', () => {
    const name     = document.getElementById('new-name').value || 'proc';
    const burst    = parseInt(document.getElementById('new-burst').value) || 5;
    const priority = parseInt(document.getElementById('new-priority').value) || 5;
    const memory   = parseInt(document.getElementById('new-memory').value) || 32;
    sendCmd({ cmd: 'add_process', name, burst, priority, memory });
    form.classList.add('hidden');
    termPrint('system', `[kernel] spawning '${name}' burst=${burst} prio=${priority} mem=${memory}MB\n`);
  });
}

/* ─── MEMORY MAP ────────────────────────────────────────────────────── */
function updateMemory() {
  const m = state.memory_stats || {};
  const map = state.memory_map || [];
  const total = m.total || 1024;
  const used = m.used || 0;
  const free = m.free || total;
  const frag = m.fragmentation || 0;

  set('mem-total', total + ' MB');
  set('mem-used', used + ' MB');
  set('mem-free', free + ' MB');
  set('mem-frag', frag.toFixed(1) + '%');
  set('mem-blocks', m.block_count || 0);

  const usedPct = (used / total) * 100;
  const freePct = (free / total) * 100;
  bar('mem-used-bar', usedPct);
  bar('mem-free-bar', freePct);
  bar('mem-frag-bar', Math.min(frag, 100));

  // Visual memory map
  const visual = document.getElementById('mem-map-visual');
  if (!visual) return;
  visual.innerHTML = '';

  const containerW = visual.offsetWidth || 400;
  map.forEach(block => {
    const pct = (block.size / total) * 100;
    const w = Math.max((block.size / total) * containerW - 2, 4);
    const div = document.createElement('div');
    div.className = 'mem-block ' + (block.pid < 0 ? 'free' : 'alloc');
    div.style.width = w + 'px';
    if (block.pid >= 0) {
      const color = pidColor(block.pid);
      div.style.background = color + '22';
      div.style.borderColor = color;
      div.style.color = color;
      div.title = `PID ${block.pid}: @${block.start} size=${block.size}MB`;
      if (w > 30) div.textContent = `P${block.pid}`;
    } else {
      div.title = `Free: @${block.start} size=${block.size}MB`;
      if (w > 40) div.textContent = 'FREE';
    }
    visual.appendChild(div);
  });
}

/* ─── SCHEDULER ─────────────────────────────────────────────────────── */
function updateScheduler() {
  const s = state.scheduler || {};
  const procs = (state.processes || []);

  set('sm-avg-wait',   (s.avg_wait_time || 0).toFixed(2) + ' ticks');
  set('sm-avg-ta',     (s.avg_turnaround_time || 0).toFixed(2) + ' ticks');
  set('sm-cpu-util',   (s.cpu_utilization || 0).toFixed(1) + '%');
  set('sm-algo',       s.algorithm === 'Priority' ? 'Priority' : 'Round Robin');
  set('sm-completed',  s.completed || 0);
  set('sm-ticks',      s.total_ticks || state.tick || 0);

  // Gantt chart
  drawGantt(state.gantt || []);

  // Ready queue
  const rq = document.getElementById('ready-queue');
  if (rq) {
    const readyProcs = procs.filter(p => p.state === 'READY' || p.state === 'RUNNING');
    rq.innerHTML = readyProcs.map(p => {
      const isCurrent = p.pid === s.current_pid;
      return `<div class="rq-item${isCurrent ? ' current-proc' : ''}" style="border-color:${pidColor(p.pid)}">
        <span style="color:${pidColor(p.pid)}">P${p.pid}</span> ${p.name}
        ${isCurrent ? ' (RUNNING)' : ''}
      </div>`;
    }).join('') || '<span style="color:var(--text-dim);font-size:11px">No processes ready</span>';
  }
}

function drawGantt(gantt) {
  const canvas = document.getElementById('gantt-canvas');
  const scroll = document.getElementById('gantt-scroll');
  if (!canvas || !gantt.length) return;

  const H = 60;
  const barH = 34;
  const barY = 8;
  const unitW = 24;

  const maxTick = gantt.reduce((m, e) => Math.max(m, e.end || e.end_time || 0), 0);
  const W = Math.max((maxTick + 2) * unitW, scroll.offsetWidth || 600);

  canvas.width = W;
  canvas.height = H;

  const ctx = canvas.getContext('2d');
  ctx.clearRect(0, 0, W, H);

  // Grid
  ctx.strokeStyle = '#1a2232';
  ctx.lineWidth = 1;
  for (let t = 0; t <= maxTick + 2; t++) {
    const x = t * unitW;
    ctx.beginPath();
    ctx.moveTo(x, 0);
    ctx.lineTo(x, H);
    ctx.stroke();
  }

  // Entries
  gantt.slice(-Math.floor(W / unitW)).forEach(entry => {
    const pid = entry.pid;
    const start = entry.start || entry.start_time || 0;
    const end   = entry.end   || entry.end_time   || start + 1;
    const color = pidColor(pid);
    const x = start * unitW;
    const w = (end - start) * unitW - 2;

    if (pid < 0) {
      // Idle
      ctx.fillStyle = '#111';
      ctx.fillRect(x + 1, barY, w, barH);
      ctx.fillStyle = '#333';
      ctx.font = '9px Share Tech Mono';
      if (w > 20) ctx.fillText('IDLE', x + 4, barY + 20);
    } else {
      ctx.fillStyle = color + '33';
      ctx.fillRect(x + 1, barY, w, barH);
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.5;
      ctx.strokeRect(x + 1, barY, w, barH);
      ctx.fillStyle = color;
      ctx.font = 'bold 10px Share Tech Mono';
      if (w > 18) ctx.fillText(`P${pid}`, x + 4, barY + 20);
    }

    // Tick label
    ctx.fillStyle = '#5a7a9a';
    ctx.font = '8px Share Tech Mono';
    ctx.fillText(start, x + 2, H - 2);
  });

  // Scroll to end
  scroll.scrollLeft = scroll.scrollWidth;
}

/* ─── FILESYSTEM ────────────────────────────────────────────────────── */
function updateFilesystem() {
  const fs = state.filesystem;
  if (!fs) return;

  const inodes = fs.inodes || [];
  renderFSTree(inodes, fs.root_id);
  renderFSInodes(inodes);
}

function renderFSTree(inodes, rootId) {
  const el = document.getElementById('fs-tree');
  if (!el) return;

  const byId = {};
  inodes.forEach(n => byId[n.id] = n);

  function renderNode(id, depth) {
    const node = byId[id];
    if (!node) return '';
    const indent = '&nbsp;'.repeat(depth * 4);
    const icon = node.type === 'dir' ? '📁' : '📄';
    const cls = node.type === 'dir' ? 'dir' : 'file';
    let html = `<div class="fs-node ${cls}">${indent}${icon} ${node.name}</div>`;
    // children
    inodes.filter(n => n.parent === id).forEach(child => {
      html += renderNode(child.id, depth + 1);
    });
    return html;
  }

  el.innerHTML = renderNode(rootId, 0);
}

function renderFSInodes(inodes) {
  const tbody = document.getElementById('fs-inode-tbody');
  if (!tbody) return;
  tbody.innerHTML = inodes.map(n => `<tr>
    <td>${n.id}</td>
    <td class="${n.type === 'dir' ? 't-dir' : 't-file'}">${n.name}</td>
    <td>${n.type}</td>
    <td>${n.size > 0 ? formatBytes(n.size) : '—'}</td>
    <td>${n.depth}</td>
    <td>${n.owner === 0 ? 'root' : 'student'}</td>
  </tr>`).join('');
}

/* ─── IPC ───────────────────────────────────────────────────────────── */
function updateIPC() {
  const events = state.ipc_events || [];
  const stats  = state.ipc_stats  || {};

  set('ipc-pipes',    stats.active_pipes  || 0);
  set('ipc-queues',   stats.active_queues || 0);
  set('ipc-total',    stats.total_events  || 0);
  const dl = state.deadlock;
  set('ipc-deadlock', dl ? '⚠ YES' : 'NONE');
  document.getElementById('ipc-deadlock').style.color = dl ? 'var(--red)' : 'var(--green)';

  const log = document.getElementById('ipc-log');
  if (!log) return;

  const entries = events.slice(-50).map(e => {
    const cls = e.type && e.type.includes('PIPE') ? 'pipe' : e.type && e.type.includes('MQ') ? 'mq' : 'other';
    const t = e.time ? new Date(e.time * 1000).toLocaleTimeString() : '';
    return `<div class="ipc-event ${cls}">
      <strong>${e.type || '?'}</strong> 
      PID ${e.sender} → PID ${e.receiver} 
      ${e.data ? `<span style="color:var(--text-dim)">[${e.data}]</span>` : ''} 
      <span style="color:var(--text-dim);font-size:10px">${t}</span>
    </div>`;
  }).join('');

  log.innerHTML = entries || '<span style="color:var(--text-dim);font-size:11px">No IPC events yet</span>';
  log.scrollTop = log.scrollHeight;
}

/* ─── REAL STATS ────────────────────────────────────────────────────── */
function updateRealStats() {
  const rs       = (state.real_stats) || {};
  const mem      = rs.memory      || {};
  const cores    = rs.cpu_per_core || [];
  const topProcs = rs.top_processes || [];
  const disk     = rs.disk_io     || {};
  const net      = rs.net_io      || {};
  const parts    = rs.disk_parts  || [];
  const loadAvg  = rs.load_avg    || [];

  // ── CPU per-core bars ─────────────────────────────────────────────
  const coresEl = document.getElementById('cpu-cores');
  if (coresEl && cores.length > 0) {
    coresEl.innerHTML = cores.map((pct, i) => {
      const color = pct > 80 ? 'var(--red)' : pct > 50 ? 'var(--yellow)' : 'var(--green)';
      return `<div class="core-row">
        <span class="core-lbl">CPU${i}</span>
        <div class="core-bar-wrap"><div class="core-bar" style="width:${pct}%;background:${color}"></div></div>
        <span class="core-pct">${pct.toFixed(0)}%</span>
      </div>`;
    }).join('');
  }

  // ── RAM donut ─────────────────────────────────────────────────────
  if (mem.total) {
    drawRAMDonut(mem);
    const gb = v => (v / 1073741824).toFixed(1) + ' GB';
    set('rs-used',  gb(mem.used  || 0));
    set('rs-free',  gb(mem.free  || 0));
    set('rs-total', gb(mem.total || 0));
  }

  // ── Top processes ─────────────────────────────────────────────────
  const tpEl = document.getElementById('top-proc-list');
  if (tpEl && topProcs.length > 0) {
    tpEl.innerHTML = topProcs.slice(0, 15).map((p, i) => {
      const cpu2 = (p.cpu_percent    || 0).toFixed(1);
      const mem2 = (p.memory_percent || 0).toFixed(1);
      const barW = Math.min(parseFloat(cpu2), 100);
      return `<div class="tp-item">
        <span class="tp-rank">${i + 1}</span>
        <span class="tp-name">${escHtml(p.name)}</span>
        <span class="tp-cpu">${cpu2}%</span>
        <span class="tp-mem">${mem2}%</span>
        <div class="tp-bar-wrap"><div class="tp-bar" style="width:${barW}%"></div></div>
      </div>`;
    }).join('');
  }

  // ── System metrics ────────────────────────────────────────────────
  set('rs-ctx',    rs.ctx_switches ? rs.ctx_switches.toLocaleString() : '-');
  set('rs-disk-r', disk.read_bytes  ? formatBytes(disk.read_bytes)  : '-');
  set('rs-disk-w', disk.write_bytes ? formatBytes(disk.write_bytes) : '-');
  set('rs-net-s',  net.bytes_sent   ? formatBytes(net.bytes_sent)   : '-');
  set('rs-net-r',  net.bytes_recv   ? formatBytes(net.bytes_recv)   : '-');

  // ── Load average (new) ────────────────────────────────────────────
  const laEl = document.getElementById('rs-load-avg');
  if (laEl && loadAvg.length >= 3) {
    laEl.textContent = `${loadAvg[0].toFixed(2)}, ${loadAvg[1].toFixed(2)}, ${loadAvg[2].toFixed(2)}`;
  }

  // ── Swap (new) ────────────────────────────────────────────────────
  const swapEl = document.getElementById('rs-swap');
  if (swapEl && mem.swap_total) {
    const gb = v => (v / 1073741824).toFixed(1) + ' GB';
    swapEl.textContent = `${gb(mem.swap_used)} / ${gb(mem.swap_total)} (${mem.swap_pct?.toFixed(0)}%)`;
  }

  // ── Disk partitions table (real df output) ────────────────────────
  const diskEl = document.getElementById('rs-disk-parts');
  if (diskEl && parts.length > 0) {
    diskEl.innerHTML = `
      <table style="width:100%;border-collapse:collapse;font-size:11px;">
        <thead>
          <tr style="color:var(--text-dim);border-bottom:1px solid var(--border);">
            <th style="text-align:left;padding:4px;">Device</th>
            <th style="text-align:left;padding:4px;">Mount</th>
            <th style="text-align:right;padding:4px;">Size</th>
            <th style="text-align:right;padding:4px;">Used</th>
            <th style="text-align:right;padding:4px;">Free</th>
            <th style="text-align:right;padding:4px;">Use%</th>
          </tr>
        </thead>
        <tbody>
          ${parts.map(p => {
            const color = p.percent > 85 ? 'var(--red)' : p.percent > 65 ? 'var(--yellow)' : 'var(--green)';
            return `<tr style="border-bottom:1px solid #1a2232;">
              <td style="padding:4px;color:var(--cyan)">${escHtml(p.device)}</td>
              <td style="padding:4px;">${escHtml(p.mountpoint)}</td>
              <td style="padding:4px;text-align:right;">${p.total_gb}G</td>
              <td style="padding:4px;text-align:right;">${p.used_gb}G</td>
              <td style="padding:4px;text-align:right;">${p.free_gb}G</td>
              <td style="padding:4px;text-align:right;color:${color}">${p.percent.toFixed(0)}%</td>
            </tr>`;
          }).join('')}
        </tbody>
      </table>`;
  }

  // ── Drivers are updated via tickDrivers() → updateDrivers() ───────
  // Called from the driverInterval which is wired to real data above.
}

function drawRAMDonut(mem) {
  const canvas = document.getElementById('real-ram-chart');
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const W = canvas.width, H = canvas.height;
  const cx = W/2, cy = H/2, r = Math.min(W, H) * 0.38, lw = 20;

  ctx.clearRect(0, 0, W, H);

  // Track
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, Math.PI * 2);
  ctx.strokeStyle = '#1a2232';
  ctx.lineWidth = lw;
  ctx.stroke();

  // Used
  const usedAngle = ((mem.used || 0) / (mem.total || 1)) * Math.PI * 2;
  ctx.beginPath();
  ctx.arc(cx, cy, r, -Math.PI/2, -Math.PI/2 + usedAngle);
  ctx.strokeStyle = '#00d4ff';
  ctx.lineWidth = lw;
  ctx.lineCap = 'butt';
  ctx.shadowColor = '#00d4ff';
  ctx.shadowBlur = 10;
  ctx.stroke();
  ctx.shadowBlur = 0;

  // Center text
  ctx.fillStyle = '#e8f4ff';
  ctx.font = 'bold 18px Share Tech Mono';
  ctx.textAlign = 'center';
  ctx.fillText((mem.percent || 0).toFixed(0) + '%', cx, cy + 6);
}

/* ════════════════════════════════════════════════════════════════════
   TERMINAL EMULATOR
   ════════════════════════════════════════════════════════════════════ */

const termHistory = [];
let termHistIdx = -1;
let termCwd = '/';

function getPromptText() {
  const sess = window.SESSION || {};
  const user = sess.user || 'root';
  const uid  = sess.uid  != null ? sess.uid : 0;
  const sym  = uid === 0 ? '#' : '$';
  return `${user}@kernelviz:${termCwd}${sym} `;
}

function updatePromptDisplay() {
  const prompt = document.getElementById('term-prompt');
  if (prompt) prompt.textContent = getPromptText();
}

function initTerminal() {
  const input  = document.getElementById('term-input');
  const output = document.getElementById('term-output');
  const prompt = document.getElementById('term-prompt');

  if (!input) return;

  updatePromptDisplay();

  // ── Fixed banner: use a <pre> element so \n renders correctly ──────
  const bannerDiv = document.createElement('div');
  bannerDiv.className = 'term-banner';
  bannerDiv.style.cssText = 'color:var(--cyan);white-space:pre;font-size:11px;line-height:1.2;margin-bottom:6px;';
  // Strip leading/trailing blank lines from BOOT_ART
  bannerDiv.textContent = BOOT_ART.replace(/^\n+|\n+$/g, '');
  output.appendChild(bannerDiv);

  termPrint('system', 'KernelViz OS 1.0 (CEP Edition). Type \'help\' for commands.\n');
  termPrint('system', 'Connect WebSocket server: python bridge/data_stream.py\n\n');

  input.addEventListener('keydown', e => {
    if (e.key === 'Enter') {
      const cmd = input.value.trim();
      input.value = '';

      if (cmd) {
        termHistory.unshift(cmd);
        termHistIdx = -1;
        termPrintLine(cmd);

        if (cmd === 'clear') {
          output.innerHTML = '';
          updatePromptDisplay();
        } else {
          const localResult = handleLocalCmd(cmd);
          if (localResult !== null) {
            termPrint('output', localResult);
          } else if (wsConnected) {
            sendCmd({ cmd: 'terminal', input: cmd });
          } else {
            termPrint('output', `bash: ${cmd.split(' ')[0]}: WebSocket not connected\n`);
          }
        }
      } else {
        termPrintLine('');
      }
      output.scrollTop = output.scrollHeight;

    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      termHistIdx = Math.min(termHistIdx + 1, termHistory.length - 1);
      input.value = termHistory[termHistIdx] || '';
    } else if (e.key === 'ArrowDown') {
      e.preventDefault();
      termHistIdx = Math.max(termHistIdx - 1, -1);
      input.value = termHistIdx >= 0 ? termHistory[termHistIdx] : '';
    } else if (e.key === 'Tab') {
      e.preventDefault();
      const partial = input.value;
      const cmds = ['help','ls','cd','cat','pwd','ps','top','kill','free','meminfo',
                    'uname','uptime','date','hostname','whoami','dmesg','lscpu','df',
                    'mount','echo','env','sched','algo','quantum','mkdir','touch','rm',
                    'clear','ifconfig'];
      const match = cmds.find(c => c.startsWith(partial));
      if (match) input.value = match;
    }
  });

  document.querySelector('[data-win="terminal"]')?.addEventListener('click', () => {
    setTimeout(() => input.focus(), 50);
  });
}

function termPrintLine(cmd) {
  const output = document.getElementById('term-output');
  const prompt = document.getElementById('term-prompt');
  if (!output) return;
  const div = document.createElement('div');
  div.innerHTML = `<span style="color:var(--green)">${escHtml(prompt.textContent)}</span><span style="color:#e8f4ff">${escHtml(cmd)}</span>`;
  output.appendChild(div);
}

function termPrint(type, text) {
  const output = document.getElementById('term-output');
  if (!output) return;
  const div = document.createElement('div');
  div.style.whiteSpace = 'pre-wrap';
  if (type === 'system') div.style.color = '#5a7a9a';
  else if (type === 'info') div.style.color = 'var(--cyan)';
  else if (type === 'error') div.style.color = 'var(--red)';
  else div.style.color = '#c8d8e8';
  div.innerHTML = ansiToHtml(text);
  output.appendChild(div);
  output.scrollTop = output.scrollHeight;
}

function handleTerminalResponse(text) {
  if (!text) return;
  if (text === '\x1b[CLEAR]') {
    document.getElementById('term-output').innerHTML = '';
    return;
  }
  // Update prompt cwd if cd worked
  const output = document.getElementById('term-output');
  const div = document.createElement('div');
  div.style.whiteSpace = 'pre-wrap';
  div.style.color = '#c8d8e8';
  div.innerHTML = ansiToHtml(text);
  output.appendChild(div);
  output.scrollTop = output.scrollHeight;
}

function handleLocalCmd(cmd) {
  const [c, ...args] = cmd.trim().split(/\s+/);
  const sess = window.SESSION || {};
  const user = sess.user || 'root';
  const uid  = sess.uid  != null ? sess.uid : 0;
  const role = sess.role || 'root';

  switch (c) {
    case 'help':
      return [
        'KernelViz OS — Built-in Commands:',
        '  whoami       — print current user',
        '  id           — print uid/gid/groups',
        '  users        — list logged-in users',
        '  last         — show login history',
        '  passwd       — (simulated) change password',
        '  uname [-a]   — kernel/system info',
        '  date         — current date/time',
        '  uptime       — system uptime',
        '  echo <text>  — print text',
        '  hostname     — print hostname',
        '  ps           — process list (needs bridge)',
        '  ls [path]    — list directory (needs bridge)',
        '  cat <file>   — read file (needs bridge)',
        '  dmesg        — kernel ring buffer (needs bridge)',
        '  free         — memory info (needs bridge)',
        '  top          — process monitor (needs bridge)',
        '  lsmod        — loaded modules (simulated)',
        '  lspci        — PCI devices (simulated)',
        '  ifconfig     — network info (simulated)',
        '  logout       — log out of current session',
        '',
        'Start bridge server: python bridge/data_stream.py',
        ''
      ].join('\n');

    case 'whoami':
      return user + '\n';

    case 'id':
      return `uid=${uid}(${user}) gid=${uid}(${user}) groups=${uid}(${user})${role === 'root' ? ',0(root)' : ''}\n`;

    case 'users':
      return user + '\n';

    case 'last': {
      const loginTime = sess.loginTime ? sess.loginTime.toString().slice(0,24) : 'unknown';
      return [
        `${user.padEnd(10)} tty1         ${loginTime}   still logged in`,
        '',
        'wtmp begins ' + new Date().toDateString(),
        ''
      ].join('\n');
    }

    case 'passwd':
      return `Changing password for ${user}.\npasswd: Authentication token manipulation error (simulated — edit USER_DB in login.js)\n`;

    case 'uname':
      if (args.includes('-a') || args.includes('--all'))
        return 'KernelViz 6.8.0-kviz #1 SMP PREEMPT_DYNAMIC x86_64 GNU/Linux\n';
      return 'KernelViz\n';

    case 'date':
      return new Date().toString() + '\n';

    case 'uptime': {
      const secs = Math.floor((Date.now() - _startTime) / 1000);
      const h = Math.floor(secs / 3600), m = Math.floor((secs % 3600) / 60), s = secs % 60;
      return ` ${new Date().toTimeString().slice(0,8)} up ${h}:${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')},  1 user,  load average: ${(Math.random()*1.5).toFixed(2)}, ${(Math.random()*1.2).toFixed(2)}, ${(Math.random()).toFixed(2)}\n`;
    }

    case 'hostname':
      return 'kernelviz-os\n';

    case 'echo':
      return args.join(' ') + '\n';

    case 'lsmod':
      return [
        'Module                  Size  Used by',
        'aesni_intel           372736  0',
        'drm_kms_helper        311296  1 i915',
        'e1000e                282624  0',
        'xhci_hcd              409600  0',
        'ahci                   45056  0',
        'snd_hda_intel         114688  0',
        'btusb                  73728  0',
        'kviz_sched             16384  1  [kernel]',
        'kviz_mem               12288  1  [kernel]',
        ''
      ].join('\n');

    case 'lspci':
      return [
        '00:00.0 Host bridge: Intel Corporation 8th Gen Core Processor Host Bridge/DRAM (rev 07)',
        '00:02.0 VGA compatible controller: Intel Corporation UHD Graphics 620',
        '00:14.0 USB controller: Intel Corporation Sunrise Point-LP USB 3.0 xHCI Controller',
        '00:17.0 SATA controller: Intel Corporation Sunrise Point-LP SATA Controller [AHCI mode]',
        '00:1f.3 Audio device: Intel Corporation Sunrise Point-LP HD Audio',
        '02:00.0 Network controller: Intel Corporation Wireless 8265 / 8275',
        ''
      ].join('\n');

    case 'ifconfig':
      return [
        'eth0: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500',
        '        inet 192.168.1.100  netmask 255.255.255.0  broadcast 192.168.1.255',
        '        inet6 fe80::1  prefixlen 64  scopeid 0x20<link>',
        '        ether 00:11:22:33:44:55  txqueuelen 1000  (Ethernet)',
        '        RX packets 12847  bytes 18234512 (17.3 MiB)',
        '        TX packets 8421   bytes 2198044 (2.0 MiB)',
        '',
        'lo: flags=73<UP,LOOPBACK,RUNNING>  mtu 65536',
        '        inet 127.0.0.1  netmask 255.0.0.0',
        '        loop  txqueuelen 1000  (Local Loopback)',
        ''
      ].join('\n');

    case 'logout':
    case 'exit':
      setTimeout(() => {
        if (typeof logout === 'function') logout();
        else if (confirm('Log out?')) window.location.reload();
      }, 200);
      return 'logout\n';

    case 'su':
      return `su: Authentication failure (use the login screen to switch users)\n`;

    case 'sudo':
      if (role !== 'root' && role !== 'admin')
        return `[sudo] password for ${user}: \nsudo: ${user} is not in the sudoers file. This incident will be reported.\n`;
      return `[sudo] Running as root: ${args.join(' ')}\n`;

    case 'ps':
    case 'ls':
    case 'cat':
    case 'dmesg':
    case 'free':
    case 'top':
      return wsConnected
        ? null  // pass to WS
        : `${c}: WebSocket not connected — start bridge/data_stream.py\n`;

    default:
      return `bash: ${c}: command not found\n`;
  }
}

function ansiToHtml(text) {
  return escHtml(text)
    .replace(/\x1b\[0m/g, '</span>')
    .replace(/\x1b\[1;32m/g, '<span style="color:#00ff88;font-weight:bold">')
    .replace(/\x1b\[0;32m/g, '<span style="color:#00ff88">')
    .replace(/\x1b\[1;33m/g, '<span style="color:#ffd700;font-weight:bold">')
    .replace(/\x1b\[1;31m/g, '<span style="color:#ff3860;font-weight:bold">')
    .replace(/\x1b\[0;31m/g, '<span style="color:#ff3860">')
    .replace(/\x1b\[1;34m/g, '<span style="color:#2196f3;font-weight:bold">')
    .replace(/\x1b\[1;37m/g, '<span style="color:#e8f4ff;font-weight:bold">')
    .replace(/\x1b\[1;36m/g, '<span style="color:#00d4ff;font-weight:bold">')
    .replace(/\x1b\[CLEAR\]/g, '');
}

function escHtml(s) {
  return (s || '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

/* ════════════════════════════════════════════════════════════════════
   SCHEDULER CONTROLS
   ════════════════════════════════════════════════════════════════════ */

function initSchedulerControls() {
  document.getElementById('btn-toggle-algo')?.addEventListener('click', () => {
    currentAlgo = currentAlgo === 0 ? 1 : 0;
    sendCmd({ cmd: 'set_algorithm', algo: currentAlgo });
    const name = currentAlgo === 0 ? 'Round Robin' : 'Priority';
    termPrint('system', `[kernel] scheduler: switched to ${name}\n`);
  });

  document.getElementById('btn-set-quantum')?.addEventListener('click', () => {
    const q = parseInt(document.getElementById('quantum-input').value);
    if (q >= 1 && q <= 20) {
      sendCmd({ cmd: 'set_quantum', quantum: q });
      termPrint('system', `[kernel] scheduler: quantum set to ${q} ticks\n`);
    }
  });
}

/* ════════════════════════════════════════════════════════════════════
   HELPERS
   ════════════════════════════════════════════════════════════════════ */

function set(id, val) {
  const el = document.getElementById(id);
  if (el) el.textContent = val;
}

function bar(id, pct) {
  const el = document.getElementById(id);
  if (el) el.style.width = Math.min(Math.max(pct, 0), 100) + '%';
}

function formatBytes(b) {
  if (b >= 1073741824) return (b / 1073741824).toFixed(1) + ' GB';
  if (b >= 1048576)    return (b / 1048576).toFixed(1) + ' MB';
  if (b >= 1024)       return (b / 1024).toFixed(0) + ' KB';
  return b + ' B';
}

/* ════════════════════════════════════════════════════════════════════
   STOP OS (for logout)
   ════════════════════════════════════════════════════════════════════ */

let _tickInterval = null;
let _driverInterval = null;

function stopOS() {
  if (_tickInterval)   clearInterval(_tickInterval);
  if (_driverInterval) clearInterval(_driverInterval);
  if (ws && ws.readyState === WebSocket.OPEN) ws.close();
  ws = null;
  wsConnected = false;
}

/* ════════════════════════════════════════════════════════════════════
   DEVICE DRIVERS & IRQ SIMULATION
   ════════════════════════════════════════════════════════════════════ */

const IRQ_BASE = { 0: 1000000, 1: 50, 8: 64, 9: 120, 16: 8000, 24: 3200, 28: 4500 };
const irqCounts = { ...IRQ_BASE };

function tickDrivers() {
  // IRQ counts come from real /proc/interrupts via WebSocket
  const rs = (state.real_stats) || {};
  const irqReal = rs.irq_counts || {};

  // Update IRQ table rows from real /proc/interrupts data
  const irqIds = [0, 1, 8, 9, 16, 24, 28];
  irqIds.forEach(id => {
    const el = document.getElementById('irq-' + id);
    if (el) {
      const val = irqReal[String(id)];
      if (val !== undefined) {
        el.textContent = Number(val).toLocaleString();
      }
    }
  });

  // Render real kernel modules into the driver list
  updateDrivers();
}

/* ════════════════════════════════════════════════════════════════════
   MAIN INIT
   ════════════════════════════════════════════════════════════════════ */

function updateDrivers() {
  const rs = (state.real_stats) || {};
  const drivers = rs.drivers || [];
  if (!drivers.length) return;

  const list = document.getElementById('driver-list');
  if (!list) return;

  const TYPE_ICON = {
    filesystem: '📁',
    audio:      '🔊',
    network:    '🌐',
    usb:        '🔌',
    storage:    '💾',
    graphics:   '🖥',
    input:      '⌨',
    kernel:     '⚙',
  };

  list.innerHTML = drivers.slice(0, 12).map(d => {
    const stateClass = d.state === 'LOADED'   ? 'ds-ok'
                     : d.state === 'LOADING'  ? 'ds-load'
                     : 'ds-idle';
    const icon = TYPE_ICON[d.type] || '⚙';
    const sizeKb = d.size > 0 ? `${Math.round(d.size / 1024)} KB` : '';
    return `<div class="driver-item">
      <div>
        <div class="driver-name">${icon} ${d.name}</div>
        <div class="driver-mod">Module: ${d.name}.ko${sizeKb ? '  (' + sizeKb + ')' : ''}</div>
      </div>
      <span class="driver-status ${stateClass}">${d.state}</span>
    </div>`;
  }).join('');
}

/* ════════════════════════════════════════════════════════════════════
   MAIN INIT
   ════════════════════════════════════════════════════════════════════ */

function initOS() {
  initTabs();
  initTerminal();
  initSpawnForm();
  initSchedulerControls();
  startClock();
  connectWS();

  // Start drivers simulation
  tickDrivers();
  _driverInterval = setInterval(tickDrivers, 1000);

  // Print session info in terminal
  const user = (window.SESSION && window.SESSION.user) || 'root';
  const uid  = (window.SESSION && window.SESSION.uid)  || 0;
  setTimeout(() => {
    termPrint('info', `[login] Session started for ${user} (uid=${uid})\n`);
    termPrint('info', `[kernel] KernelViz OS 1.0 (CEP Edition) ready.\n`);
    termPrint('system', 'Type "help" for available commands.\n\n');
  }, 200);
}

// Start boot on load — login.js handles DOMContentLoaded first
window.addEventListener('DOMContentLoaded', () => {
  runBootSequence();
});
