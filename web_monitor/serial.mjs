export function parseReceiverLine(line) {
  const status = /^ESP-NOW receiver ([\da-f]{2}(?::[\da-f]{2}){5}), channel (\d+)$/i.exec(line);
  if (status) return { type: 'status', mac: status[1].toUpperCase(), channel: Number(status[2]) };
  const event = /^SOUND EVENT from ([\da-f]{2}(?::[\da-f]{2}){5}) #(\d+) level=(-?\d+(?:\.\d+)?) dBFS floor=(-?\d+(?:\.\d+)?) dBFS rise=(-?\d+(?:\.\d+)?) dB duration=(\d+) ms uptime=(\d+) ms$/i.exec(line);
  if (!event) return null;
  return { type: 'event', mac: event[1].toUpperCase(), sequence: Number(event[2]),
    level: Number(event[3]), floor: Number(event[4]), rise: Number(event[5]),
    duration: Number(event[6]), uptime: Number(event[7]) };
}

// A serial read can contain half a line, several lines, or half a UTF-8 character.
export class SerialLines {
  constructor(onLine) { this.onLine = onLine; this.decoder = new TextDecoder(); this.pending = ''; }
  push(bytes) { this.consume(this.decoder.decode(bytes, { stream: true })); }
  consume(text) {
    const parts = (this.pending + text).split('\n');
    this.pending = parts.pop();
    for (const part of parts) this.emit(part);
    // Bound memory even when a noisy device never supplies a newline.
    if (this.pending.length > 8192) {
      this.emit(this.pending);
      this.pending = '';
    }
  }
  emit(text) {
    const line = text.replace(/\r$/, '');
    if (line) this.onLine(line.length > 8192 ? line.slice(0, 8175) + ' [line truncated]' : line);
  }
  finish() { this.consume(this.decoder.decode()); if (this.pending) this.emit(this.pending); this.pending = ''; }
}

// Own the stream lock so disconnects, USB removal and read failures always close it.
export class SerialConnection {
  constructor(onLine, onClosed) { this.onLine = onLine; this.onClosed = onClosed; this.port = null; this.reader = null; this.task = null; this.stopping = false; }
  async open(port) {
    if (this.port) throw new Error('A serial connection is already open.');
    await port.open({ baudRate: 115200, dataBits: 8, stopBits: 1, parity: 'none', flowControl: 'none' });
    this.port = port;
    this.stopping = false;
    this.task = this.read(port);
  }
  async read(port) {
    const lines = new SerialLines(this.onLine);
    let failure = null;
    try {
      if (!port.readable) throw new Error('This port has no readable stream.');
      this.reader = port.readable.getReader();
      while (!this.stopping) {
        const { value, done } = await this.reader.read();
        if (done) break;
        if (value) lines.push(value);
      }
    } catch (error) {
      if (!this.stopping) failure = error;
    } finally {
      lines.finish();
      this.reader?.releaseLock();
      this.reader = null;
      try { await port.close(); } catch (error) { if (!this.stopping && !failure) failure = error; }
      this.port = null;
      this.onClosed(failure, this.stopping);
    }
  }
  async close() {
    this.stopping = true;
    try { await this.reader?.cancel(); } catch { /* USB removal may have already cancelled it. */ }
    await this.task;
  }
}
