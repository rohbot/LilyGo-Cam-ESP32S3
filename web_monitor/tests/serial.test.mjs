import { test } from 'node:test';
import assert from 'node:assert/strict';
import { SerialLines, SerialConnection, parseReceiverLine } from '../serial.mjs';

const eventLine = 'SOUND EVENT from 10:20:BA:58:5F:A4 #12 level=-40.00 dBFS floor=-60.00 dBFS rise=20.00 dB duration=64 ms uptime=8000 ms';
const encoder = new TextEncoder();

test('parses exactly the receiver event and heartbeat formats', () => {
  assert.deepEqual(parseReceiverLine(eventLine), { type: 'event', mac: '10:20:BA:58:5F:A4', sequence: 12, level: -40, floor: -60, rise: 20, duration: 64, uptime: 8000 });
  assert.deepEqual(parseReceiverLine('ESP-NOW receiver F0:F5:BD:76:DB:30, channel 6'), { type: 'status', mac: 'F0:F5:BD:76:DB:30', channel: 6 });
  for (const text of ['boot:0x2b', 'SOUND EVENT malformed', '<script>alert(1)</script>', '']) assert.equal(parseReceiverLine(text), null);
});

test('serial framing handles fragmented UTF-8, CRLF and multiple lines', () => {
  const result = [];
  const lines = new SerialLines((line) => result.push(line));
  const bytes = encoder.encode('First\r\nµ sound\n' + eventLine + '\r\nTail');
  for (const byte of bytes) lines.push(new Uint8Array([byte]));
  lines.finish();
  assert.deepEqual(result, ['First', 'µ sound', eventLine, 'Tail']);
});

test('framing bounds long lines and keeps later valid events readable', () => {
  const result = [];
  const lines = new SerialLines((line) => result.push(line));
  lines.push(encoder.encode('x'.repeat(10000)));
  lines.push(encoder.encode('\n' + eventLine + '\n'));
  assert.ok(result.every((line) => line.length <= 8192));
  assert.equal(result.at(-1), eventLine);
});

function mockPort() {
  let controller;
  return {
    opened: 0, closed: 0, options: null,
    async open(options) {
      this.opened++;
      this.options = options;
      this.readable = new ReadableStream({ start(value) { controller = value; } });
    },
    async close() { assert.equal(this.readable.locked, false); this.closed++; },
    send(text) { controller.enqueue(encoder.encode(text)); },
    fail() { controller.error(new Error('USB disconnected')); },
    end() { controller.close(); }
  };
}

test('disconnect cancels a blocked read, flushes partial data and releases the port', async () => {
  const result = [];
  let closed;
  const port = mockPort();
  const session = new SerialConnection((line) => result.push(line), (...args) => { closed = args; });
  await session.open(port);
  assert.equal(port.options.baudRate, 115200);
  port.send('hello\npartial');
  await new Promise((resolve) => setTimeout(resolve, 0));
  await session.close();
  assert.deepEqual(result, ['hello', 'partial']);
  assert.deepEqual(closed, [null, true]);
  assert.equal(port.closed, 1);
  assert.equal(session.port, null);
  await session.open(port);  // Reconnection must not leave the old stream locked.
  await session.close();
  assert.equal(port.closed, 2);
});

test('USB/read errors clean up and report the failure', async () => {
  let closed;
  const port = mockPort();
  const session = new SerialConnection(() => {}, (...args) => { closed = args; });
  await session.open(port);
  port.fail();
  await session.task;
  assert.equal(closed[0].message, 'USB disconnected');
  assert.equal(closed[1], false);
  assert.equal(port.closed, 1);
  assert.equal(session.reader, null);
});

test('an unsuccessful port open does not leave a connection behind', async () => {
  const session = new SerialConnection(() => {}, () => {});
  await assert.rejects(session.open({ async open() { throw new Error('Port in use'); } }), /Port in use/);
  assert.equal(session.port, null);
});
