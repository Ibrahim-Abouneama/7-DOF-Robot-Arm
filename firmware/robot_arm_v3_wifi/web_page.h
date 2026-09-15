#ifndef WEB_PAGE_H
#define WEB_PAGE_H

#include <pgmspace.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Robot Arm Hotkey Controller v3</title>
  <style>
    /* ─── Reset & Base ─────────────────────────────────────────── */
    *, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }

    body {
      font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
      background: #0f172a;
      color: #e2e8f0;
      min-height: 100vh;
      overflow-x: hidden;
    }

    /* ─── Layout ───────────────────────────────────────────────── */
    .app-container {
      display: grid;
      grid-template-columns: 1fr 320px;
      grid-template-rows: auto 1fr;
      gap: 0;
      height: 100vh;
    }

    /* ─── Header ───────────────────────────────────────────────── */
    .header {
      grid-column: 1 / -1;
      background: #1e293b;
      border-bottom: 1px solid #334155;
      padding: 12px 24px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
    }

    .header h1 {
      font-size: 18px;
      font-weight: 700;
      color: #f8fafc;
      letter-spacing: -0.5px;
    }

    .header h1 span { color: #38bdf8; }

    .header-controls { display: flex; align-items: center; gap: 12px; }

    .status-pill {
      display: flex;
      align-items: center;
      gap: 8px;
      padding: 6px 14px;
      background: #0f172a;
      border-radius: 20px;
      font-size: 12px;
      color: #94a3b8;
    }

    .status-dot {
      width: 8px; height: 8px;
      border-radius: 50%;
      background: #ef4444;
      transition: background 0.3s;
    }
    .status-dot.connected { background: #22c55e; box-shadow: 0 0 8px #22c55e88; }

    /* ─── Buttons ──────────────────────────────────────────────── */
    .btn {
      border: none;
      border-radius: 8px;
      padding: 8px 16px;
      font-size: 13px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.15s;
      white-space: nowrap;
    }
    .btn:hover { filter: brightness(1.15); }
    .btn:active { transform: scale(0.97); }

    .btn-primary { background: #3b82f6; color: white; }
    .btn-danger  { background: #ef4444; color: white; }
    .btn-ghost   { background: #1e293b; color: #94a3b8; border: 1px solid #334155; }
    .btn-ghost:hover { color: #e2e8f0; border-color: #475569; }
    .btn-warning { background: #f59e0b; color: #0f172a; }

    .btn-estop {
      background: #dc2626;
      color: white;
      font-weight: 800;
      letter-spacing: 1px;
      padding: 8px 20px;
      text-transform: uppercase;
      font-size: 12px;
    }
    .btn-estop.frozen {
      background: #f59e0b;
      color: #0f172a;
      animation: pulse-warn 0.8s infinite alternate;
    }
    @keyframes pulse-warn {
      from { box-shadow: 0 0 0 0 #f59e0b88; }
      to   { box-shadow: 0 0 12px 4px #f59e0b44; }
    }

    /* ─── Main Content ─────────────────────────────────────────── */
    .main-content {
      display: flex;
      flex-direction: column;
      gap: 16px;
      padding: 16px 24px;
      overflow-y: auto;
    }

    /* ─── Action Ribbon ────────────────────────────────────────── */
    .action-ribbon {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
    }

    .ribbon-divider {
      width: 1px;
      height: 28px;
      background: #334155;
      margin: 0 4px;
    }

    .step-btn, .speed-btn {
      min-width: 40px;
      padding: 6px 10px;
      font-size: 12px;
      background: #1e293b;
      color: #64748b;
      border: 1px solid #334155;
      border-radius: 6px;
      cursor: pointer;
      transition: all 0.15s;
      font-weight: 600;
    }
    .step-btn:hover, .speed-btn:hover { color: #e2e8f0; border-color: #475569; }
    .step-btn.active, .speed-btn.active { background: #3b82f6; color: white; border-color: #3b82f6; }

    /* ─── Channel Cards ────────────────────────────────────────── */
    .channels-grid {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(340px, 1fr));
      gap: 12px;
    }

    .channel-card {
      background: #1e293b;
      border: 1px solid #334155;
      border-radius: 12px;
      padding: 14px 18px;
      transition: all 0.2s;
      cursor: pointer;
    }
    .channel-card:hover { border-color: #475569; }
    .channel-card.active {
      border-color: #3b82f6;
      box-shadow: 0 0 0 1px #3b82f688, 0 4px 12px #3b82f622;
    }

    .ch-header {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-bottom: 10px;
    }

    .ch-title-group { display: flex; align-items: center; gap: 10px; }

    .ch-badge {
      background: #3b82f6;
      color: white;
      font-size: 10px;
      font-weight: 800;
      padding: 3px 8px;
      border-radius: 4px;
      letter-spacing: 0.5px;
    }

    .ch-name { font-size: 13px; color: #cbd5e1; font-weight: 500; }

    .angle-display {
      font-size: 22px;
      font-weight: 800;
      color: #f8fafc;
      font-variant-numeric: tabular-nums;
      min-width: 56px;
      text-align: right;
    }

    /* ─── Slider Row ───────────────────────────────────────────── */
    .slider-row {
      display: flex;
      align-items: center;
      gap: 8px;
    }

    .nudge-btn {
      width: 32px; height: 32px;
      border: 1px solid #475569;
      background: #0f172a;
      color: #94a3b8;
      border-radius: 6px;
      font-size: 16px;
      font-weight: 700;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.1s;
    }
    .nudge-btn:hover { color: white; border-color: #3b82f6; }
    .nudge-btn:active { transform: scale(0.9); background: #3b82f6; }

    .servo-slider {
      flex: 1;
      -webkit-appearance: none;
      height: 6px;
      border-radius: 3px;
      background: #334155;
      outline: none;
    }
    .servo-slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 18px; height: 18px;
      border-radius: 50%;
      background: #3b82f6;
      cursor: pointer;
      border: 2px solid #1e293b;
      box-shadow: 0 0 6px #3b82f644;
    }

    /* ─── Hotkey Badges ────────────────────────────────────────── */
    .hotkey-row {
      display: flex;
      align-items: center;
      justify-content: space-between;
      margin-top: 8px;
      font-size: 11px;
      color: #64748b;
    }

    .key-badge {
      display: inline-block;
      padding: 2px 8px;
      background: #0f172a;
      border: 1px solid #334155;
      border-radius: 4px;
      font-size: 11px;
      font-weight: 700;
      color: #94a3b8;
      font-family: 'Consolas', 'Courier New', monospace;
      transition: all 0.12s;
      margin: 0 2px;
    }
    .key-badge.pressed {
      background: #3b82f6;
      color: white;
      border-color: #3b82f6;
      box-shadow: 0 0 10px #3b82f666;
      transform: scale(1.1);
    }

    /* ─── Sidebar ──────────────────────────────────────────────── */
    .sidebar {
      background: #1e293b;
      border-left: 1px solid #334155;
      display: flex;
      flex-direction: column;
      overflow: hidden;
    }

    .sidebar-section {
      padding: 16px;
      border-bottom: 1px solid #334155;
    }

    .sidebar-title {
      font-size: 11px;
      font-weight: 700;
      text-transform: uppercase;
      letter-spacing: 1px;
      color: #64748b;
      margin-bottom: 12px;
    }

    /* ─── Keyboard Reference ───────────────────────────────────── */
    .kb-table { width: 100%; font-size: 12px; }
    .kb-table td { padding: 3px 0; }
    .kb-table td:first-child { color: #94a3b8; }
    .kb-table td:last-child { text-align: right; font-family: monospace; color: #e2e8f0; }

    /* ─── Console ──────────────────────────────────────────────── */
    .console-section { flex: 1; display: flex; flex-direction: column; min-height: 0; }

    .console-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 16px 8px;
    }

    .console-box {
      flex: 1;
      background: #0f172a;
      margin: 0 8px 8px;
      border-radius: 8px;
      padding: 8px;
      overflow-y: auto;
      font-family: 'Consolas', 'Courier New', monospace;
      font-size: 11px;
      line-height: 1.6;
      border: 1px solid #1e293b;
    }

    .console-entry { padding: 1px 0; word-break: break-all; }
    .console-entry.tx  { color: #38bdf8; }
    .console-entry.rx  { color: #a78bfa; }
    .console-entry.err { color: #f87171; }
    .console-entry.info { color: #64748b; }
    .console-entry.ok  { color: #4ade80; }

    /* ─── Last ACK indicator ───────────────────────────────────── */
    .last-ack {
      font-size: 10px;
      color: #475569;
      margin-top: 4px;
    }
  </style>
</head>
<body>
  <div class="app-container">
    <!-- ═══ Header ═══ -->
    <div class="header">
      <h1>🦾 Robot Arm <span>Hotkey Controller</span> v3</h1>
      <div class="header-controls">
        <div class="status-pill">
          <div class="status-dot" id="statusDot"></div>
          <span id="statusText">Disconnected</span>
        </div>
        <button class="btn btn-primary" id="btnConnect">⚡ USB Serial</button>
        <div style="display:flex;align-items:center;background:#1e293b;border:1px solid #334155;border-radius:8px;padding:2px 6px;gap:4px;">
          <input type="text" id="wifiIpInput" value="192.168.4.1" title="ESP32 IP Address"
                 style="width:95px;background:transparent;border:none;color:#38bdf8;font-family:monospace;font-size:12px;font-weight:700;outline:none;">
          <button class="btn btn-ghost" id="btnConnectWifi" style="padding:5px 10px;font-size:12px;color:#38bdf8;border:none;">📶 WiFi</button>
        </div>
        <button class="btn btn-estop" id="btnEstop">⏹ E-STOP (Esc)</button>
      </div>
    </div>

    <!-- ═══ Main Content ═══ -->
    <div class="main-content">
      <!-- Action Ribbon -->
      <div class="action-ribbon">
        <button class="btn btn-primary" id="btnHome" style="font-weight:700;letter-spacing:0.5px;background:#0284c7;">🏠 Home</button>
        <button class="btn btn-ghost" id="btnAllCenter">⊙ All 90°</button>
        <button class="btn btn-ghost" id="btnSweepBase">↔ Sweep Base</button>
        <button class="btn btn-ghost" id="btnIsoShoulder">🔬 Iso Shoulder</button>
        <button class="btn btn-ghost" id="btnIsoElbow">🔬 Iso Elbow</button>
        <button class="btn btn-ghost" id="btnRestoreAll">↩ Restore All</button>
        <button class="btn btn-ghost" id="btnDiag">📊 Diagnostics</button>
        <div class="ribbon-divider"></div>
        <span style="font-size:12px;color:#64748b;">Step:</span>
        <button class="step-btn" data-step="1">1°</button>
        <button class="step-btn active" data-step="5">5°</button>
        <button class="step-btn" data-step="10">10°</button>
        <button class="step-btn" data-step="20">20°</button>
        <div class="ribbon-divider"></div>
        <span style="font-size:12px;color:#64748b;">Speed:</span>
        <button class="speed-btn" data-speed="40">40°/s</button>
        <button class="speed-btn active" data-speed="80">80°/s</button>
        <button class="speed-btn" data-speed="150">150°/s</button>
        <button class="speed-btn" data-speed="300">Max</button>
      </div>

      <!-- Channel Cards Grid -->
      <div class="channels-grid" id="channelsGrid"></div>
    </div>

    <!-- ═══ Sidebar ═══ -->
    <div class="sidebar">
      <div class="sidebar-section">
        <div class="sidebar-title">Keyboard Shortcuts</div>
        <table class="kb-table">
          <tr><td>Base Yaw</td><td><span class="key-badge" id="key_q">Q</span> <span class="key-badge" id="key_a">A</span></td></tr>
          <tr><td>Shoulder</td><td><span class="key-badge" id="key_w">W</span> <span class="key-badge" id="key_s">S</span></td></tr>
          <tr><td>Elbow</td><td><span class="key-badge" id="key_e">E</span> <span class="key-badge" id="key_d">D</span></td></tr>
          <tr><td>Forearm</td><td><span class="key-badge" id="key_r">R</span> <span class="key-badge" id="key_f">F</span></td></tr>
          <tr><td>Wrist Roll</td><td><span class="key-badge" id="key_t">T</span> <span class="key-badge" id="key_g">G</span></td></tr>
          <tr><td>Wrist Pitch</td><td><span class="key-badge" id="key_y">Y</span> <span class="key-badge" id="key_h">H</span></td></tr>
          <tr><td>Gripper</td><td><span class="key-badge" id="key_u">U</span> <span class="key-badge" id="key_j">J</span></td></tr>
          <tr><td colspan="2" style="padding-top:8px;border-top:1px solid #334155;"></td></tr>
          <tr><td>Active ch nudge</td><td>← →</td></tr>
          <tr><td>Step size</td><td>[ ]</td></tr>
          <tr><td>Home pose</td><td>Space</td></tr>
          <tr><td>Select ch</td><td>1-7</td></tr>
          <tr><td>E-Stop</td><td>Esc</td></tr>
        </table>
      </div>

      <div class="console-section">
        <div class="console-header">
          <div class="sidebar-title" style="margin:0;">Serial Console</div>
          <button class="btn btn-ghost" id="btnClearLog" style="padding:4px 10px;font-size:11px;">Clear</button>
        </div>
        <div class="console-box" id="consoleBox"></div>
        <div class="last-ack" id="lastAckDisplay" style="padding:4px 16px 8px;">Last ACK: —</div>
      </div>
    </div>
  </div>

  <script>
    // ═════════════════════════════════════════════════════════════
    //  1. CHANNEL DEFINITIONS
    // ═════════════════════════════════════════════════════════════
    const channels = [
      { id: 15, name: 'Base Turntable Yaw',     min: 0,   max: 180, default: 90,  keyUp: 'q', keyDown: 'a', numKey: '1' },
      { id: 14, name: 'Shoulder Pitch (Grey)',   min: 10,  max: 170, default: 90,  keyUp: 'w', keyDown: 's', numKey: '2' },
      { id: 13, name: 'Elbow Pitch (Blue)',      min: 10,  max: 170, default: 60,  keyUp: 'e', keyDown: 'd', numKey: '3' },
      { id: 12, name: 'Forearm Pitch (Grey)',    min: 10,  max: 170, default: 170, keyUp: 'r', keyDown: 'f', numKey: '4' },
      { id: 11, name: 'Wrist Roll',             min: 0,   max: 180, default: 90,  keyUp: 't', keyDown: 'g', numKey: '5' },
      { id: 10, name: 'Wrist Pitch',            min: 10,  max: 170, default: 125, keyUp: 'y', keyDown: 'h', numKey: '6' },
      { id: 9,  name: 'Gripper Claw',           min: 0,   max: 90,  default: 45,  keyUp: 'u', keyDown: 'j', numKey: '7' }
    ];

    const currentAngles = {};
    channels.forEach(ch => currentAngles[ch.id] = ch.default);

    let activeChannelId = 15;
    let stepSize = 5;
    let isFrozen = false;

    // ═════════════════════════════════════════════════════════════
    //  2. BUILD UI CARDS
    // ═════════════════════════════════════════════════════════════
    const grid = document.getElementById('channelsGrid');

    channels.forEach(ch => {
      const card = document.createElement('div');
      card.className = `channel-card ${ch.id === activeChannelId ? 'active' : ''}`;
      card.id = `card_${ch.id}`;
      card.innerHTML = `
        <div class="ch-header">
          <div class="ch-title-group">
            <span class="ch-badge">CH ${ch.id}</span>
            <span class="ch-name">${ch.name}</span>
          </div>
          <div class="angle-display" id="val_${ch.id}">${ch.default}°</div>
        </div>
        <div class="slider-row">
          <button class="nudge-btn" id="btnDec_${ch.id}">−</button>
          <input type="range" class="servo-slider" id="slider_${ch.id}"
                 min="${ch.min}" max="${ch.max}" value="${ch.default}">
          <button class="nudge-btn" id="btnInc_${ch.id}">+</button>
        </div>
        <div class="hotkey-row">
          <span>Direct Keys:</span>
          <div>
            <span class="key-badge" id="badge_up_${ch.id}">+ ${ch.keyUp.toUpperCase()}</span>
            <span class="key-badge" id="badge_down_${ch.id}">− ${ch.keyDown.toUpperCase()}</span>
          </div>
          <span style="color:#475569;">[Select: <b>${ch.numKey}</b>]</span>
        </div>
      `;
      grid.appendChild(card);

      // Slider input
      card.querySelector(`#slider_${ch.id}`).addEventListener('input', e => {
        setAngle(ch.id, parseInt(e.target.value));
      });

      // Nudge buttons
      card.querySelector(`#btnDec_${ch.id}`).addEventListener('click', e => {
        e.stopPropagation();
        nudgeAngle(ch.id, -stepSize);
      });
      card.querySelector(`#btnInc_${ch.id}`).addEventListener('click', e => {
        e.stopPropagation();
        nudgeAngle(ch.id, +stepSize);
      });

      // Click to select
      card.addEventListener('click', () => selectChannel(ch.id));
    });

    // ═════════════════════════════════════════════════════════════
    //  3. CHANNEL SELECTION & ANGLE LOGIC
    // ═════════════════════════════════════════════════════════════
    function selectChannel(chId) {
      activeChannelId = chId;
      channels.forEach(ch => {
        const card = document.getElementById(`card_${ch.id}`);
        if (card) card.classList.toggle('active', ch.id === chId);
      });
    }

    function setAngle(chId, angle, send = true) {
      const cfg = channels.find(c => c.id === chId);
      if (!cfg) return;

      const clamped = Math.max(cfg.min, Math.min(cfg.max, Math.round(angle)));
      currentAngles[chId] = clamped;

      const slider = document.getElementById(`slider_${chId}`);
      const valText = document.getElementById(`val_${chId}`);
      if (slider) slider.value = clamped;
      if (valText) valText.textContent = `${clamped}°`;

      if (send && !isFrozen) {
        queueCommand(chId, clamped);
      }
    }

    function nudgeAngle(chId, delta) {
      const cur = currentAngles[chId] !== undefined ? currentAngles[chId] : 90;
      setAngle(chId, cur + delta);
    }

    // ═════════════════════════════════════════════════════════════
    //  4. WEBSERIAL COMMUNICATION
    // ═════════════════════════════════════════════════════════════
    let port = null;
    let writer = null;
    let reader = null;
    let isConnected = false;
    let connectionType = 'none'; // 'serial' or 'wifi'
    let ws = null;

    const btnConnect = document.getElementById('btnConnect');
    const btnConnectWifi = document.getElementById('btnConnectWifi');
    const wifiIpInput = document.getElementById('wifiIpInput');
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');
    const consoleBox = document.getElementById('consoleBox');
    const lastAckDisplay = document.getElementById('lastAckDisplay');

    function log(msg, type = 'info') {
      const entry = document.createElement('div');
      entry.className = `console-entry ${type}`;
      const time = new Date().toLocaleTimeString('en-US', { hour12: false });
      entry.textContent = `[${time}] ${msg}`;
      consoleBox.appendChild(entry);
      consoleBox.scrollTop = consoleBox.scrollHeight;

      while (consoleBox.children.length > 500) {
        consoleBox.removeChild(consoleBox.firstChild);
      }
    }

    document.getElementById('btnClearLog').addEventListener('click', () => {
      consoleBox.innerHTML = '';
    });

    // ── 1. USB WebSerial Mode ──
    async function connectSerial() {
      if (!('serial' in navigator)) {
        alert('WebSerial not supported. Use Chrome, Edge, or Opera.');
        return;
      }

      if (isConnected) {
        disconnectAll();
        return;
      }

      try {
        port = await navigator.serial.requestPort();
        await port.open({ baudRate: 115200 });

        writer = port.writable.getWriter();
        isConnected = true;
        connectionType = 'serial';

        statusDot.className = 'status-dot connected';
        statusText.textContent = 'USB Connected (115200)';
        btnConnect.textContent = '⏏ Disconnect';
        btnConnect.className = 'btn btn-danger';
        btnConnectWifi.disabled = true;
        log('USB Serial port connected.', 'ok');

        readSerialLoop();

        setTimeout(() => {
          sendRawString('$QUERY*\n');
          log('Querying hardware positions...', 'info');
        }, 800);
      } catch (err) {
        log(`Connection failed: ${err.message}`, 'err');
      }
    }

    // ── 2. WiFi WebSocket Mode ──
    function connectWifi() {
      if (isConnected) {
        disconnectAll();
        return;
      }

      const ip = (wifiIpInput ? wifiIpInput.value.trim() : '') || '192.168.4.1';
      const wsUrl = `ws://${ip}:81`;
      log(`Connecting to WiFi WebSocket: ${wsUrl}...`, 'info');

      try {
        ws = new WebSocket(wsUrl);

        ws.onopen = () => {
          isConnected = true;
          connectionType = 'wifi';
          statusDot.className = 'status-dot connected';
          statusText.textContent = `WiFi (${ip})`;
          btnConnectWifi.textContent = '⏏ Disconnect';
          btnConnectWifi.style.color = '#ef4444';
          btnConnect.disabled = true;
          log(`Connected to WiFi WebSocket (${wsUrl})!`, 'ok');

          setTimeout(() => {
            sendRawString('$QUERY*\n');
          }, 300);
        };

        ws.onmessage = (event) => {
          const lines = event.data.split('\n');
          for (const line of lines) {
            const trimmed = line.trim();
            if (trimmed) processRxLine(trimmed);
          }
        };

        ws.onerror = (err) => {
          log(`WiFi WebSocket error. Ensure device is connected to "RobotArm-Hotkeys".`, 'err');
        };

        ws.onclose = () => {
          if (isConnected && connectionType === 'wifi') {
            log('WiFi WebSocket disconnected.', 'warn');
          }
          disconnectAll();
        };
      } catch (err) {
        log(`WiFi connection failed: ${err.message}`, 'err');
      }
    }

    async function disconnectAll() {
      if (connectionType === 'serial') {
        try {
          if (writer) { writer.releaseLock(); writer = null; }
          if (reader) { await reader.cancel(); reader.releaseLock(); reader = null; }
          if (port) { await port.close(); port = null; }
        } catch (e) { /* ignore */ }
      }
      if (ws) {
        try { ws.close(); } catch (e) {}
        ws = null;
      }

      isConnected = false;
      connectionType = 'none';
      statusDot.className = 'status-dot';
      statusText.textContent = 'Disconnected';
      btnConnect.textContent = '⚡ USB Serial';
      btnConnect.className = 'btn btn-primary';
      btnConnect.disabled = false;
      btnConnectWifi.textContent = '📶 WiFi';
      btnConnectWifi.style.color = '#38bdf8';
      btnConnectWifi.disabled = false;
      log('Disconnected.', 'info');
    }

    btnConnect.addEventListener('click', connectSerial);
    btnConnectWifi.addEventListener('click', connectWifi);

    // ── Rate-limited TX Queue (50Hz max) ──
    let pendingQueue = {};
    let isTxRunning = false;

    function queueCommand(ch, angle) {
      pendingQueue[ch] = angle;
      if (!isTxRunning) processTxQueue();
    }

    async function processTxQueue() {
      if (isTxRunning || !isConnected) return;
      isTxRunning = true;

      try {
        while (Object.keys(pendingQueue).length > 0) {
          const entries = Object.entries(pendingQueue);
          const [ch, angle] = entries[0];
          delete pendingQueue[ch];

          const packet = `$SET,${ch},${angle}*\n`;
          log(`TX >> ${packet.trim()}`, 'tx');

          if (connectionType === 'serial' && writer) {
            const encoder = new TextEncoder();
            await writer.write(encoder.encode(packet));
          } else if (connectionType === 'wifi' && ws && ws.readyState === WebSocket.OPEN) {
            ws.send(packet);
          }

          // 20ms spacing = 50Hz max rate
          await new Promise(r => setTimeout(r, 20));
        }
      } catch (err) {
        log(`TX Error: ${err.message}`, 'err');
      } finally {
        isTxRunning = false;
      }
    }

    async function sendRawString(str) {
      if (!isConnected) return;
      try {
        log(`TX >> ${str.trim()}`, 'tx');
        if (connectionType === 'serial' && writer) {
          const encoder = new TextEncoder();
          await writer.write(encoder.encode(str));
        } else if (connectionType === 'wifi' && ws && ws.readyState === WebSocket.OPEN) {
          ws.send(str);
        }
      } catch (e) {
        log(`TX Error: ${e.message}`, 'err');
      }
    }

    // ── Line-Buffered Serial Reader ──
    // Accumulates bytes until newline, then processes complete lines.
    // This fixes the split-message display bug from v2.
    let rxBuffer = '';

    async function readSerialLoop() {
      const decoder = new TextDecoder();
      try {
        while (port && port.readable && isConnected) {
          reader = port.readable.getReader();
          try {
            while (true) {
              const { value, done } = await reader.read();
              if (done) break;
              if (value) {
                rxBuffer += decoder.decode(value, { stream: true });

                // Process complete lines
                let newlineIdx;
                while ((newlineIdx = rxBuffer.indexOf('\n')) !== -1) {
                  const line = rxBuffer.substring(0, newlineIdx).trim();
                  rxBuffer = rxBuffer.substring(newlineIdx + 1);

                  if (line.length > 0) {
                    processRxLine(line);
                  }
                }
              }
            }
          } finally {
            reader.releaseLock();
          }
        }
      } catch (e) {
        if (isConnected) log(`RX Error: ${e.message}`, 'err');
      }
    }

    // Process a complete received line
    function processRxLine(line) {
      // Parse POS response: "POS:15:90:14:85:13:120:12:90:11:90:10:90:9:45"
      if (line.startsWith('POS')) {
        log(`RX << ${line}`, 'ok');
        const parts = line.substring(3).split(':').filter(s => s.length > 0);
        // Parts are alternating: ch, angle, ch, angle, ...
        for (let i = 0; i < parts.length - 1; i += 2) {
          const ch = parseInt(parts[i]);
          const angle = parseInt(parts[i + 1]);
          if (!isNaN(ch) && !isNaN(angle)) {
            setAngle(ch, angle, false);   // Update UI without re-sending
          }
        }
        log('Position sync complete.', 'ok');
        return;
      }

      // Parse ACK responses
      if (line.startsWith('ACK:')) {
        log(`RX << ${line}`, 'rx');
        lastAckDisplay.textContent = `Last ACK: ${new Date().toLocaleTimeString('en-US', { hour12: false })} — ${line}`;
        return;
      }

      // Error responses
      if (line.startsWith('ERR:')) {
        log(`RX << ${line}`, 'err');
        return;
      }

      // Everything else (boot messages, diagnostics, etc.)
      log(`RX << ${line}`, 'rx');
    }

    // ═════════════════════════════════════════════════════════════
    //  5. E-STOP
    // ═════════════════════════════════════════════════════════════
    const btnEstop = document.getElementById('btnEstop');

    function toggleEstop() {
      if (isFrozen) {
        // Unfreeze
        isFrozen = false;
        btnEstop.textContent = '⏹ E-STOP (Esc)';
        btnEstop.classList.remove('frozen');
        sendRawString('$GO*\n');
        log('E-Stop released. Resuming control.', 'ok');
      } else {
        // Freeze
        isFrozen = true;
        // Clear any pending commands immediately
        pendingQueue = {};
        btnEstop.textContent = '▶ RESUME (Esc)';
        btnEstop.classList.add('frozen');
        sendRawString('$STOP*\n');
        log('🛑 E-STOP ACTIVATED — All servos frozen!', 'err');
      }
    }

    btnEstop.addEventListener('click', toggleEstop);

    // ═════════════════════════════════════════════════════════════
    //  6. KEYBOARD HOTKEY ENGINE
    // ═════════════════════════════════════════════════════════════
    const pressedTimers = {};

    function flashBadge(elId) {
      const el = document.getElementById(elId);
      if (!el) return;
      el.classList.add('pressed');
      clearTimeout(pressedTimers[elId]);
      pressedTimers[elId] = setTimeout(() => el.classList.remove('pressed'), 120);
    }

    window.addEventListener('keydown', (e) => {
      // Don't capture when typing in text inputs
      if (e.target.tagName === 'INPUT' && e.target.type !== 'range') return;

      const key = e.key.toLowerCase();

      // Escape: E-Stop toggle
      if (e.key === 'Escape') {
        toggleEstop();
        return;
      }

      // If frozen, ignore all other keys
      if (isFrozen) return;

      // Step size: [ and ]
      if (key === '[') {
        setStepSize(stepSize === 20 ? 10 : stepSize === 10 ? 5 : 1);
        return;
      }
      if (key === ']') {
        setStepSize(stepSize === 1 ? 5 : stepSize === 5 ? 10 : 20);
        return;
      }

      // Space: Home Pose (from default.png)
      if (e.code === 'Space') {
        e.preventDefault();
        goHome();
        return;
      }

      // Number keys 1-7: select channel
      const numMatch = channels.find(ch => ch.numKey === key);
      if (numMatch) {
        selectChannel(numMatch.id);
        return;
      }

      // Arrow keys: nudge active channel
      if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') {
        e.preventDefault();
        nudgeAngle(activeChannelId, -stepSize);
        return;
      }
      if (e.key === 'ArrowRight' || e.key === 'ArrowUp') {
        e.preventDefault();
        nudgeAngle(activeChannelId, +stepSize);
        return;
      }

      // Direct per-channel hotkeys
      for (const ch of channels) {
        if (key === ch.keyUp) {
          flashBadge(`key_${ch.keyUp}`);
          flashBadge(`badge_up_${ch.id}`);
          selectChannel(ch.id);
          nudgeAngle(ch.id, +stepSize);
          return;
        }
        if (key === ch.keyDown) {
          flashBadge(`key_${ch.keyDown}`);
          flashBadge(`badge_down_${ch.id}`);
          selectChannel(ch.id);
          nudgeAngle(ch.id, -stepSize);
          return;
        }
      }
    });

    // ═════════════════════════════════════════════════════════════
    //  7. ACTION RIBBON HANDLERS
    // ═════════════════════════════════════════════════════════════
    function setStepSize(s) {
      stepSize = s;
      document.querySelectorAll('.step-btn').forEach(b => {
        b.classList.toggle('active', parseInt(b.dataset.step) === stepSize);
      });
    }

    document.querySelectorAll('.step-btn').forEach(b => {
      b.addEventListener('click', () => setStepSize(parseInt(b.dataset.step)));
    });

    let currentSpeed = 80;

    function setSpeed(spd) {
      currentSpeed = spd;
      document.querySelectorAll('.speed-btn').forEach(b => {
        b.classList.toggle('active', parseInt(b.dataset.speed) === currentSpeed);
      });
      sendRawString(`$SPEED,${spd}*\n`);
      log(`Motion speed set to ${spd}°/s`, 'info');
    }

    document.querySelectorAll('.speed-btn').forEach(b => {
      b.addEventListener('click', () => setSpeed(parseInt(b.dataset.speed)));
    });

    const HOME_POSE = {
      15: 90,
      14: 90,
      13: 60,
      12: 170,
      11: 90,
      10: 125,
      9: 45
    };

    function goHome() {
      sendRawString('$HOME*\n');
      Object.entries(HOME_POSE).forEach(([chId, angle]) => {
        setAngle(parseInt(chId), angle, false);
      });
      log('Moved to Home Pose: [90°, 90°, 60°, 170°, 90°, 125°, 45°]', 'ok');
    }

    document.getElementById('btnHome').addEventListener('click', goHome);

    document.getElementById('btnAllCenter').addEventListener('click', () => {
      sendRawString('$ALL,90*\n');
      channels.forEach(ch => setAngle(ch.id, 90, false));
      log('All channels → 90° center', 'info');
    });

    document.getElementById('btnSweepBase').addEventListener('click', () => {
      sendRawString('$SWEEP,15*\n');
      log('Running non-blocking sweep on Base (CH 15)...', 'info');
    });

    document.getElementById('btnIsoShoulder').addEventListener('click', () => {
      sendRawString('$ISO,14*\n');
      log('Isolating Shoulder (CH 14). Other channels OFF.', 'info');
    });

    document.getElementById('btnIsoElbow').addEventListener('click', () => {
      sendRawString('$ISO,13*\n');
      log('Isolating Elbow (CH 13). Other channels OFF.', 'info');
    });

    document.getElementById('btnRestoreAll').addEventListener('click', () => {
      sendRawString('$ON*\n');
      log('Restoring all channels.', 'info');
    });

    document.getElementById('btnDiag').addEventListener('click', () => {
      sendRawString('$DIAG*\n');
      log('Requesting hardware diagnostics...', 'info');
    });

    // ── 8. Auto-Connect if Served Directly by ESP32 ──
    window.addEventListener('DOMContentLoaded', () => {
      if (window.location.protocol.startsWith('http') && window.location.hostname && window.location.hostname !== 'localhost' && window.location.hostname !== '127.0.0.1') {
        if (wifiIpInput) wifiIpInput.value = window.location.hostname;
        log(`Auto-connecting to ESP32 at ${window.location.hostname}...`, 'info');
        setTimeout(connectWifi, 400);
      }
    });
  </script>
</body>
</html>

)rawliteral";

#endif // WEB_PAGE_H
