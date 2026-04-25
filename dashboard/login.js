/**
 * login.js — KernelViz OS Login & Session Management
 * 24CS005 | MUET Jamshoro | CS-261 Operating Systems CEP
 *
 * Implements: Real Linux-style multi-user login, session tracking,
 * security audit log, privilege levels, and logout.
 */

'use strict';

/* ════════════════════════════════════════════════════════════════════
   USER DATABASE (simulated /etc/passwd + /etc/shadow)
   ════════════════════════════════════════════════════════════════════ */

const USER_DB = {
  root:    { uid: 0,     password: 'root',     role: 'root',  shell: '/bin/bash', home: '/root',        perms: ['all'] },
  admin:   { uid: 1000,  password: 'admin123', role: 'admin', shell: '/bin/bash', home: '/home/admin',  perms: ['proc_kill','mount','modprobe','net_raw'] },
  student: { uid: 1001,  password: 'muet2024', role: 'user',  shell: '/bin/sh',   home: '/home/student',perms: ['proc_own','read_proc'] },
  guest:   { uid: 65534, password: '',         role: 'guest', shell: '/bin/sh',   home: '/tmp/guest',   perms: ['read_only'] },
};

/* Current session state */
window.SESSION = {
  user: null,
  uid: null,
  role: null,
  loginTime: null,
  loginAttempts: 0,
};

let loginClockInterval = null;
let securityLog = [];

/* ════════════════════════════════════════════════════════════════════
   AUDIT LOG
   ════════════════════════════════════════════════════════════════════ */

function addSecurityLog(event, type = 'ok') {
  const now = new Date();
  const time = now.toTimeString().slice(0, 8);
  securityLog.unshift({ time, event, type });
  if (securityLog.length > 30) securityLog.pop();
  renderSecurityLog();
  // Update attempt counter
  const el = document.getElementById('sec-login-attempts');
  if (el) el.textContent = window.SESSION.loginAttempts;
}

function renderSecurityLog() {
  const el = document.getElementById('sec-log');
  if (!el) return;
  el.innerHTML = securityLog.map(e =>
    `<div class="sec-log-entry">
      <span class="sec-log-time">${e.time}</span>
      <span class="sec-log-event ${e.type === 'fail' ? 'fail' : e.type === 'warn' ? 'warn' : ''}">${e.event}</span>
    </div>`
  ).join('');
}

/* ════════════════════════════════════════════════════════════════════
   PERMISSIONS DISPLAY
   ════════════════════════════════════════════════════════════════════ */

const PERMISSION_DEFS = {
  root: [
    { name: 'Read /etc/shadow', allowed: true },
    { name: 'Write to /root', allowed: true },
    { name: 'Kill any process', allowed: true },
    { name: 'Mount filesystems', allowed: true },
    { name: 'Load kernel modules', allowed: true },
    { name: 'Network raw sockets', allowed: true },
  ],
  admin: [
    { name: 'Read /etc/shadow', allowed: false },
    { name: 'Write to /home/admin', allowed: true },
    { name: 'Kill own processes', allowed: true },
    { name: 'Mount filesystems', allowed: true },
    { name: 'Load kernel modules', allowed: true },
    { name: 'Network raw sockets', allowed: true },
  ],
  user: [
    { name: 'Read /etc/shadow', allowed: false },
    { name: 'Write to /home/student', allowed: true },
    { name: 'Kill own processes', allowed: true },
    { name: 'Mount filesystems', allowed: false },
    { name: 'Load kernel modules', allowed: false },
    { name: 'Network raw sockets', allowed: false },
  ],
  guest: [
    { name: 'Read /etc/shadow', allowed: false },
    { name: 'Write to /tmp', allowed: true },
    { name: 'Kill any processes', allowed: false },
    { name: 'Mount filesystems', allowed: false },
    { name: 'Load kernel modules', allowed: false },
    { name: 'Network raw sockets', allowed: false },
  ],
};

function updateSecurityPanel() {
  const role = window.SESSION.role || 'guest';
  const perms = PERMISSION_DEFS[role] || PERMISSION_DEFS.guest;

  const grid = document.getElementById('sec-perms-grid');
  const currentUserEl = document.getElementById('sec-current-user');
  if (currentUserEl) currentUserEl.textContent = window.SESSION.user || 'guest';
  if (grid) {
    grid.innerHTML = perms.map(p =>
      `<div class="perm-item">
        <div class="perm-dot ${p.allowed ? 'allowed' : 'denied'}"></div>
        ${p.name}
      </div>`
    ).join('');
  }
  renderSecurityLog();
}

/* ════════════════════════════════════════════════════════════════════
   LOGIN CLOCK
   ════════════════════════════════════════════════════════════════════ */

function startLoginClock() {
  const el = document.getElementById('login-clock');
  if (!el) return;
  const tick = () => {
    el.textContent = new Date().toTimeString().slice(0, 8);
  };
  tick();
  loginClockInterval = setInterval(tick, 1000);
}

function stopLoginClock() {
  clearInterval(loginClockInterval);
}

/* ════════════════════════════════════════════════════════════════════
   LOGIN FLOW
   ════════════════════════════════════════════════════════════════════ */

let selectedUser = null;

function showLoginScreen() {
  const ls = document.getElementById('login-screen');
  ls.classList.remove('login-hidden', 'fade-out');
  startLoginClock();
  resetLoginForm();
}

function hideLoginScreen(callback) {
  const ls = document.getElementById('login-screen');
  ls.classList.add('fade-out');
  setTimeout(() => {
    ls.classList.add('login-hidden');
    stopLoginClock();
    if (callback) callback();
  }, 600);
}

function resetLoginForm() {
  selectedUser = null;
  document.getElementById('login-username').value = '';
  document.getElementById('login-password').value = '';
  document.getElementById('login-msg').textContent = '';
  document.getElementById('login-msg').className = 'login-msg';
  document.getElementById('login-pass-field').classList.add('login-hidden');
  document.getElementById('login-btn').classList.add('login-hidden');
  document.getElementById('login-user-field').classList.remove('login-hidden');
  document.querySelectorAll('.login-user-card').forEach(c => c.classList.remove('selected'));
}

function setLoginMsg(msg, type = '') {
  const el = document.getElementById('login-msg');
  el.textContent = msg;
  el.className = 'login-msg ' + type;
}

function attemptLogin(username, password) {
  const user = USER_DB[username];
  if (!user) {
    window.SESSION.loginAttempts++;
    addSecurityLog(`LOGIN FAILED: unknown user "${username}"`, 'fail');
    setLoginMsg(`Login incorrect. No such user: ${username}`, 'error');
    document.getElementById('login-password').value = '';
    return false;
  }
  if (user.password !== password) {
    window.SESSION.loginAttempts++;
    addSecurityLog(`LOGIN FAILED: bad password for "${username}"`, 'fail');
    setLoginMsg('Login incorrect. Wrong password.', 'error');
    document.getElementById('login-password').value = '';
    return false;
  }

  // SUCCESS
  window.SESSION.user      = username;
  window.SESSION.uid       = user.uid;
  window.SESSION.role      = user.role;
  window.SESSION.loginTime = new Date();

  addSecurityLog(`LOGIN OK: ${username} (uid=${user.uid}) logged in`, 'ok');
  setLoginMsg(`Welcome, ${username}! Starting session…`, 'success');

  // Update taskbar user display
  const tbUser = document.getElementById('tb-user-display');
  if (tbUser) {
    const roleIcon = { root: '⬡', admin: '★', user: '◆', guest: '○' }[user.role] || '○';
    tbUser.textContent = `${roleIcon} ${username}`;
  }

  // Update terminal prompt style
  updateSecurityPanel();

  setTimeout(() => {
    hideLoginScreen(() => {
      const shell = document.getElementById('os-shell');
      shell.classList.remove('os-hidden');
      shell.classList.remove('hidden');
      if (typeof initOS === 'function') initOS();
    });
  }, 800);

  return true;
}

function logout() {
  if (window.SESSION.user) {
    addSecurityLog(`LOGOUT: ${window.SESSION.user} session ended`, 'warn');
  }

  // Stop OS
  if (typeof stopOS === 'function') stopOS();

  // Hide OS shell
  const shell = document.getElementById('os-shell');
  shell.classList.add('os-hidden');

  // Clear session
  window.SESSION = { user: null, uid: null, role: null, loginTime: null, loginAttempts: window.SESSION.loginAttempts };

  // Show login again with "logged out" message
  showLoginScreen();
  setTimeout(() => {
    setLoginMsg('You have been logged out.', '');
  }, 100);
}

/* ════════════════════════════════════════════════════════════════════
   INIT LOGIN UI
   ════════════════════════════════════════════════════════════════════ */

function initLogin() {
  const userInput    = document.getElementById('login-username');
  const passInput    = document.getElementById('login-password');
  const loginBtn     = document.getElementById('login-btn');
  const passField    = document.getElementById('login-pass-field');
  const userField    = document.getElementById('login-user-field');
  const logoutBtn    = document.getElementById('tb-logout-btn');

  // Click on user cards — quick select
  document.querySelectorAll('.login-user-card').forEach(card => {
    card.addEventListener('click', () => {
      document.querySelectorAll('.login-user-card').forEach(c => c.classList.remove('selected'));
      card.classList.add('selected');
      const uname = card.dataset.user;
      const upass = card.dataset.pass;
      selectedUser = uname;
      userInput.value = uname;
      userField.classList.add('login-hidden');

      if (upass === '') {
        // guest — no password needed
        setLoginMsg(`Logging in as ${uname} (no password required)…`, 'success');
        setTimeout(() => attemptLogin(uname, ''), 600);
      } else {
        passField.classList.remove('login-hidden');
        loginBtn.classList.remove('login-hidden');
        passInput.focus();
        setLoginMsg(`Password required for ${uname}`, '');
      }
    });
  });

  // Manual username entry
  userInput.addEventListener('keydown', e => {
    if (e.key === 'Enter') {
      const uname = userInput.value.trim();
      if (!uname) return;
      selectedUser = uname;
      userField.classList.add('login-hidden');
      passField.classList.remove('login-hidden');
      loginBtn.classList.remove('login-hidden');
      passInput.focus();
      setLoginMsg(`Password required for ${uname}`, '');
    }
  });

  // Password enter key
  passInput.addEventListener('keydown', e => {
    if (e.key === 'Enter') loginBtn.click();
  });

  // Login button
  loginBtn.addEventListener('click', () => {
    const uname = selectedUser || userInput.value.trim();
    const pass  = passInput.value;
    if (!uname) { setLoginMsg('Enter a username first.', 'error'); return; }
    attemptLogin(uname, pass);
  });

  // Logout button
  if (logoutBtn) {
    logoutBtn.addEventListener('click', () => {
      if (confirm('Log out of KernelViz OS?')) logout();
    });
  }

  // Add initial audit log entry
  addSecurityLog('SYSTEM BOOT: kernelviz-os started', 'ok');
  addSecurityLog('PAM: login prompt ready on tty1', 'ok');

  // Show login screen (not OS)
  showLoginScreen();
}

// Boot → Login (called from app.js after boot sequence)
window.showLogin = function() {
  showLoginScreen();
};

window.addEventListener('DOMContentLoaded', () => {
  // login.js runs first — just init the login UI
  initLogin();
});
