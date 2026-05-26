// ---- Canvas setup ----
const W = 32, H = 32;
const CELL = 18, GAP = 2, STEP = CELL + GAP;

const canvas = document.getElementById('gameCanvas');
canvas.width = W * STEP - GAP;
canvas.height = H * STEP - GAP;
const ctx = canvas.getContext('2d');

const TRAIL_LEN = 8;
const trail = [];
let prevScore = 0;
let collisionFlash = 0;
let prevPx = -1, prevPy = -1;
let state = { playerX: 15, playerY: 15, targetX: 25, targetY: 5, score: 0, direction: 'NEUTRU' };
const MOVE_INTERVAL = 140; // ms, should match ESP's moveInterval

// Spawn a new target at a random location not colliding with player
function spawnTarget() {
  state.targetX = Math.floor(Math.random() * W);
  state.targetY = Math.floor(Math.random() * H);
  if (state.targetX === state.playerX && state.targetY === state.playerY) {
    state.targetX = (state.targetX + 8) % W;
  }
}

// Move the player according to current direction
function applyMovement(direction) {
  if (direction === 'NEUTRU') return;
  if      (direction === 'FATA')          state.playerY--;
  else if (direction === 'SPATE')         state.playerY++;
  else if (direction === 'STANGA')        state.playerX--;
  else if (direction === 'DREAPTA')       state.playerX++;
  else if (direction === 'FATA-DREAPTA')  { state.playerY--; state.playerX++; }
  else if (direction === 'FATA-STANGA')   { state.playerY--; state.playerX--; }
  else if (direction === 'SPATE-DREAPTA') { state.playerY++; state.playerX++; }
  else if (direction === 'SPATE-STANGA')  { state.playerY++; state.playerX--; }

  if (state.playerX < 0) state.playerX = 0;
  if (state.playerX >= W) state.playerX = W - 1;
  if (state.playerY < 0) state.playerY = 0;
  if (state.playerY >= H) state.playerY = H - 1;
}

function sendState() {
  if (!ws || ws.readyState !== WebSocket.OPEN) return;
  ws.send(JSON.stringify({
    direction: state.direction,
    score: state.score
  }));
}

// Local game tick: move player and handle collisions/score
setInterval(() => {
  const dir = state.direction;
  // previous position for trail
  if (dir !== 'NEUTRU') {
    const prev = { x: state.playerX, y: state.playerY };
    applyMovement(dir);
    if (prev.x !== state.playerX || prev.y !== state.playerY) {
      trail.push(prev);
      if (trail.length > TRAIL_LEN) trail.shift();
    }

    if (state.playerX === state.targetX && state.playerY === state.targetY) {
      state.score++;
      collisionFlash = 8;
      spawnTarget();
      sendState();
    }

    sendState();
  }
}, MOVE_INTERVAL);

// ---- Render loop ----
function renderCanvas() {
  ctx.fillStyle = '#0b1520';
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  // Grid lines
  ctx.strokeStyle = 'rgba(0,245,255,0.04)';
  ctx.lineWidth = 1;
  for (let x = 0; x < W; x++)
    for (let y = 0; y < H; y++)
      ctx.strokeRect(x * STEP + 0.5, y * STEP + 0.5, CELL - 1, CELL - 1);

  const d = state;
  const isCollision = d.playerX === d.targetX && d.playerY === d.targetY;

  // Trail
  for (let i = 0; i < trail.length; i++) {
    const a = (i / TRAIL_LEN) * 0.18;
    ctx.fillStyle = `rgba(0,245,255,${a})`;
    ctx.shadowBlur = 0;
    ctx.beginPath();
    ctx.roundRect(trail[i].x * STEP, trail[i].y * STEP, CELL, CELL, 3);
    ctx.fill();
  }

  // Target
  if (!isCollision) {
    const pulse = 0.65 + 0.35 * Math.sin(Date.now() / 350);
    ctx.shadowColor = `rgba(255,230,0,${pulse * 0.8})`;
    ctx.shadowBlur = 16 + pulse * 10;
    ctx.fillStyle = '#1a0f00';
    ctx.beginPath();
    ctx.roundRect(d.targetX * STEP, d.targetY * STEP, CELL, CELL, 3);
    ctx.fill();
    ctx.fillStyle = '#ffe600';
    ctx.shadowBlur = 8;
    ctx.beginPath();
    ctx.arc(d.targetX * STEP + CELL / 2, d.targetY * STEP + CELL / 2, 4, 0, Math.PI * 2);
    ctx.fill();
    ctx.shadowBlur = 0;
  }

  // Player
  if (isCollision || collisionFlash > 0) {
    collisionFlash = Math.max(0, collisionFlash - 1);
    ctx.shadowColor = 'rgba(0,255,136,0.9)';
    ctx.shadowBlur = 24;
    ctx.fillStyle = '#152800';
  } else {
    ctx.shadowColor = 'rgba(0,245,255,0.85)';
    ctx.shadowBlur = 20;
    ctx.fillStyle = '#002535';
  }
  ctx.beginPath();
  ctx.roundRect(d.playerX * STEP, d.playerY * STEP, CELL, CELL, 3);
  ctx.fill();
  ctx.shadowBlur = 0;

  // Crosshair
  if (!isCollision && collisionFlash === 0) {
    ctx.strokeStyle = '#00f5ff';
    ctx.lineWidth = 1;
    ctx.shadowColor = '#00f5ff';
    ctx.shadowBlur = 4;
    const px = d.playerX * STEP, py = d.playerY * STEP;
    ctx.beginPath();
    ctx.moveTo(px + CELL / 2, py + 3);
    ctx.lineTo(px + CELL / 2, py + CELL - 3);
    ctx.moveTo(px + 3, py + CELL / 2);
    ctx.lineTo(px + CELL - 3, py + CELL / 2);
    ctx.stroke();
    ctx.shadowBlur = 0;
  }

  requestAnimationFrame(renderCanvas);
}
requestAnimationFrame(renderCanvas);

// ---- Data handler ----
function applyData(data) {
  // Expect only { direction: '...' } from ESP
  if (!data || typeof data.direction !== 'string') return;
  state.direction = data.direction;
  document.getElementById('dirVal').textContent = state.direction;

  const map = {
    'NEUTRU': 'd-N',
    'FATA': 'd-F',
    'SPATE': 'd-B',
    'STANGA': 'd-L',
    'DREAPTA': 'd-R',
    'FATA-STANGA': 'd-FL',
    'FATA-DREAPTA': 'd-FR',
    'SPATE-STANGA': 'd-BL',
    'SPATE-DREAPTA': 'd-BR'
  };
  document.querySelectorAll('.dir-cell').forEach(el => el.classList.remove('active'));
  document.getElementById(map[state.direction] || 'd-N').classList.add('active');

  // update score display if changed
  const sv = document.getElementById('scoreVal');
  if (state.score !== prevScore) {
    prevScore = state.score;
    sv.classList.remove('bump'); void sv.offsetWidth; sv.classList.add('bump');
    setTimeout(() => sv.classList.remove('bump'), 260);
    const pop = document.getElementById('scorePop'); pop.classList.remove('show'); void pop.offsetWidth; pop.classList.add('show');
  }
  sv.textContent = state.score;
}

// ---- WebSocket ----
let ws = null;
const dot = document.getElementById('connDot');
const label = document.getElementById('connLabel');

// Load saved IP from localStorage
const savedIp = localStorage.getItem('esp_ip');
if (savedIp) document.getElementById('espIp').value = savedIp;

function connect() {
  const ip = document.getElementById('espIp').value.trim();
  if (!ip) {
    label.textContent = 'Introdu un IP valid!';
    return;
  }

  // Save IP for next time
  localStorage.setItem('esp_ip', ip);

  if (ws) {
    ws.close();
    ws = null;
  }

  label.textContent = 'Se conecteaza la ' + ip + '...';
  dot.classList.remove('online');

  ws = new WebSocket('ws://' + ip + ':81/');
  ws.onopen = () => {
    dot.classList.add('online');
    label.textContent = 'Conectat la ' + ip;
    sendState();
  };
  ws.onclose = () => {
    dot.classList.remove('online');
    label.textContent = 'Deconectat. Reapasa CONECTARE.';
  };
  ws.onerror = () => {
    label.textContent = 'Eroare conectare. Verifica IP-ul.';
  };
  ws.onmessage = (e) => {
    try {
      applyData(JSON.parse(e.data));
    } catch (_) {}
  };
}

// Auto-connect if we have saved IP
if (savedIp) connect();
