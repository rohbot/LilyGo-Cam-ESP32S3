import { parseReceiverLine, SerialConnection } from './serial.mjs';

const $ = (id) => document.getElementById(id);
const MAX_LINES = 1000;
let lines = [];
let totalLines = 0;
let eventCount = 0;
let connectionState = 'disconnected';
let renderPending = false;
let audioContext = null;
let activeTone = null;
const AudioContextClass = window.AudioContext || window.webkitAudioContext;

function updateAudioStatus() {
  $('audio-status').textContent = !AudioContextClass ? 'Audio unavailable in this browser'
    : !$('beep-enabled').checked ? 'Event beeps muted'
    : audioContext?.state === 'running' ? 'Event beeps ready'
    : audioContext ? 'Sound paused · click Test beep' : 'Sound activates when you connect';
}

// Create/resume audio directly from a click, before opening the serial picker.
async function prepareAudio() {
  try {
    if (!AudioContextClass) return false;
    if (!audioContext || audioContext.state === 'closed') {
      audioContext = new AudioContextClass();
      audioContext.addEventListener('statechange', updateAudioStatus);
    }
    if (audioContext.state !== 'running') await audioContext.resume();
    updateAudioStatus();
    return audioContext.state === 'running';
  } catch {
    $('audio-status').textContent = 'Sound unavailable · click Test beep to retry';
    return false;
  }
}

function beep(test = false) {
  if ((!test && !$('beep-enabled').checked) || activeTone) return;
  if (audioContext?.state !== 'running') { updateAudioStatus(); return; }
  try {
    const tone = audioContext.createOscillator();
    const gain = audioContext.createGain();
    const now = audioContext.currentTime;
    tone.type = 'sine';
    tone.frequency.setValueAtTime(880, now);
    gain.gain.setValueAtTime(0, now);
    gain.gain.linearRampToValueAtTime(0.12, now + 0.01);
    gain.gain.setValueAtTime(0.12, now + 0.14);
    gain.gain.linearRampToValueAtTime(0, now + 0.2);
    tone.connect(gain);
    gain.connect(audioContext.destination);
    tone.onended = () => {
      tone.disconnect();
      gain.disconnect();
      if (activeTone === tone) activeTone = null;
    };
    activeTone = tone;
    tone.start(now);
    tone.stop(now + 0.21);
  } catch {
    activeTone = null;
    $('audio-status').textContent = 'Could not play sound · click Test beep to retry';
  }
}

function notice(message, error = false) {
  $('notice').textContent = message;
  $('notice').classList.toggle('error', error);
}

function setState(state) {
  connectionState = state;
  const label = { disconnected: 'Disconnected', connecting: 'Connecting…', connected: 'Connected', closing: 'Disconnecting…' }[state];
  $('connection-state').textContent = label;
  $('connection-state').dataset.state = state;
  $('connect').disabled = state !== 'disconnected' || !supported;
  $('disconnect').disabled = state !== 'connected';
  if (state === 'disconnected') $('receiver-details').textContent = 'No receiver connected';
  scheduleRender();
}

function resetSummary() {
  eventCount = 0;
  $('event-count').textContent = '0';
  for (const id of ['sound-level', 'noise-floor', 'rise-level']) $(id).textContent = '—';
  $('latest-event').textContent = 'Waiting for the first event';
  $('event-duration').textContent = 'No event received yet';
}

function receive(text) {
  const now = new Date();
  const parsed = parseReceiverLine(text);
  const kind = parsed?.type === 'event' ? 'event' : /failed|error|invalid|panic|^E \(/i.test(text) ? 'warning' : 'normal';
  lines.push({ text, time: now.toLocaleTimeString([], { hour12: false }), iso: now.toISOString(), kind });
  if (lines.length > MAX_LINES) lines.splice(0, lines.length - MAX_LINES);
  totalLines++;
  if (connectionState === 'connected') notice('Receiving serial messages. Event cards update when a sound alert arrives.');
  $('last-message').textContent = `Last message ${now.toLocaleTimeString()}`;
  if (parsed?.type === 'status') {
    $('receiver-details').textContent = `${parsed.mac} · CH ${parsed.channel}`;
  } else if (parsed?.type === 'event') {
    beep();
    eventCount++;
    $('event-count').textContent = String(eventCount);
    $('sound-level').textContent = parsed.level.toFixed(1);
    $('noise-floor').textContent = parsed.floor.toFixed(1);
    $('rise-level').textContent = `${parsed.rise >= 0 ? '+' : ''}${parsed.rise.toFixed(1)}`;
    $('latest-event').textContent = `#${parsed.sequence} from ${parsed.mac}`;
    $('event-duration').textContent = `${parsed.duration} ms confirmation · ${now.toLocaleTimeString()}`;
  }
  scheduleRender();
}

function scheduleRender() {
  if (renderPending) return;
  renderPending = true;
  requestAnimationFrame(render);
}

function render() {
  renderPending = false;
  const shown = $('events-only').checked ? lines.filter((line) => line.kind === 'event') : lines;
  const terminal = $('terminal');
  const scroll = terminal.scrollTop;
  const fragment = document.createDocumentFragment();
  for (const line of shown) {
    const row = document.createElement('div');
    row.className = `log-line ${line.kind}`;
    const time = document.createElement('time');
    time.textContent = line.time;
    time.dateTime = line.iso;
    const message = document.createElement('span');
    message.className = 'message';
    message.textContent = line.text;
    row.append(time, message);
    fragment.append(row);
  }
  $('log-lines').replaceChildren(fragment);
  $('empty-state').hidden = shown.length > 0;
  $('empty-title').textContent = lines.length ? 'No sound events in the retained log.' : connectionState === 'connected' ? 'Connected. Waiting for data.' : 'Ready when you are.';
  $('empty-description').textContent = lines.length ? 'Switch off “Events only” to see all serial messages.' : connectionState === 'connected' ? 'The receiver should identify itself within five seconds. Power the sender to receive sound alerts.' : 'Connect the ESP32 receiver by USB, then select it using “Connect receiver”.';
  $('line-count').textContent = `${totalLines.toLocaleString()} lines${totalLines > MAX_LINES ? ` · ${MAX_LINES} retained` : ''}`;
  $('clear').disabled = lines.length === 0;
  $('download').disabled = lines.length === 0;
  terminal.scrollTop = $('autoscroll').checked ? terminal.scrollHeight : scroll;
}

const supported = window.isSecureContext && 'serial' in navigator;
const connection = new SerialConnection(receive, (error, intentional) => {
  setState('disconnected');
  if (error) notice(`Serial connection stopped: ${error.message}. Reconnect the receiver to try again.`, true);
  else notice(intentional ? 'Disconnected. The log is kept until you clear or close this tab.' : 'The serial stream ended. Check the USB connection and reconnect.');
});

$('connect').addEventListener('click', async () => {
  if ($('beep-enabled').checked) void prepareAudio();
  setState('connecting');
  try {
    const port = await navigator.serial.requestPort();
    // Keep prior log entries, but start fresh statistics for the new connection.
    resetSummary();
    $('receiver-details').textContent = 'Waiting for receiver identity…';
    await connection.open(port);
    if (!connection.port) return;
    setState('connected');
    notice('Connected at 115200 baud. Waiting for receiver messages.');
  } catch (error) {
    setState('disconnected');
    notice(error.name === 'NotFoundError' ? 'No port selected. Click Connect receiver when ready.' : `Could not open the port: ${error.message}. Close other serial monitors and try again.`, error.name !== 'NotFoundError');
  }
});

$('disconnect').addEventListener('click', async () => {
  setState('closing');
  await connection.close();
});

if ('serial' in navigator) {
  navigator.serial.addEventListener('disconnect', (event) => {
    if (event.target === connection.port || event.port === connection.port) void connection.close();
  });
}

$('clear').addEventListener('click', () => {
  lines = [];
  totalLines = 0;
  resetSummary();
  $('last-message').textContent = 'Log cleared';
  scheduleRender();
});
$('events-only').addEventListener('change', scheduleRender);
$('autoscroll').addEventListener('change', scheduleRender);
$('beep-enabled').addEventListener('change', () => {
  if ($('beep-enabled').checked) void prepareAudio();
  else {
    activeTone?.stop();
    updateAudioStatus();
  }
});
$('test-beep').addEventListener('click', async () => {
  if (await prepareAudio()) beep(true);
});
$('download').addEventListener('click', () => {
  const content = lines.map((line) => `${line.iso}\t${line.text}`).join('\n') + '\n';
  const url = URL.createObjectURL(new Blob([content], { type: 'text/plain;charset=utf-8' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = `sound-events-${new Date().toISOString().replace(/[:.]/g, '-')}.txt`;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});
window.addEventListener('pagehide', () => {
  void connection.close();
  if (audioContext && audioContext.state !== 'closed') void audioContext.close().catch(() => {});
});

updateAudioStatus();
$('beep-enabled').disabled = !AudioContextClass;
$('test-beep').disabled = !AudioContextClass;
setState('disconnected');
if (!window.isSecureContext) notice('Open this page on http://localhost or HTTPS to enable Web Serial.', true);
else if (!supported) notice('Web Serial is unavailable. Open this page in desktop Chrome or Edge.', true);
else notice('Ready to connect. Close any other serial monitor using the receiver’s USB port first.');
