# Sound event web serial monitor

A web serial page for the USB serial output of `examples/SoundEventReceiver`.
Source preview needs no package installation or firmware changes. Production
deployment uses a small Node.js build to produce a self-contained HTML file.

The layout adapts to phones, tablets, and desktops. Small screens use full-width
connection controls, two-column event cards, larger touch targets, and wrapped
log messages with timestamps above the text. The log height adapts to the viewport,
including landscape orientation; safe-area padding accommodates screen cutouts.
The responsive layout does not add USB serial support to a browser that lacks
Web Serial. Direct receiver connections still require a browser with that API;
the page reports unsupported browsers rather than pretending to connect.

From the repository root:

```sh
python3 -m http.server 8000 --bind 127.0.0.1 --directory web_monitor
```

Open **http://localhost:8000** in desktop Chrome or Edge on the computer with
the receiver plugged in. Close PlatformIO's serial monitor (Ctrl+C) first, then
click **Connect receiver** and choose its USB serial port. The receiver uses
**115200 baud, 8 data bits, no parity, 1 stop bit**. The browser requires a port
selection/permission from this button; it cannot silently open `/dev/ttyACM0`.

The receiver should print its station MAC and channel every five seconds. Power
the T-Camera sender, allow five seconds to learn the background, then make a
loud sound. Each received event updates the level, noise-floor, and rise cards.
These are **event measurements**, not continuous microphone telemetry. A quiet
sender produces no event updates, while receiver heartbeats still appear in the log.

Controls:

- **Disconnect** releases the port for another application or a later connection.
- **Events only** filters the display; raw messages remain in the retained log.
- **Auto-scroll** follows incoming messages. Turn it off to inspect earlier lines.
- **Beep on events** plays a short 880 Hz tone for each received sound event.
  It is enabled by default and activates when you click **Connect receiver**.
  Heartbeats and other serial messages do not beep. Uncheck it to mute alerts.
- **Test beep** plays the tone without needing an incoming event, even when event
  beeps are muted. It also reactivates audio if the browser has suspended it.
- **Download log** saves all retained messages with browser receipt timestamps.
- **Clear** clears the log and event statistics. A new connection also resets
  event statistics, while retaining earlier log lines.

Only the latest 1,000 lines are retained in memory. There is no automatic disk
storage or network upload. Unrecognized serial messages, boot logs, and errors
are displayed as plain text. Refreshing or closing the tab clears its data.

Web Serial requires a supported browser and a secure context (localhost or
HTTPS). Serve this directory instead of opening `index.html` as a `file://` URL,
because the source page uses JavaScript modules. See the official
[Web Serial guide](https://developer.chrome.com/docs/capabilities/serial).

## Build and deploy

With Node.js 18+ and npm installed, run from the repository root:

```sh
./web_monitor/deploy.sh
```

The script works regardless of the current directory. It first installs the
locked build dependencies with `npm ci`, then runs the build, and only then
copies the contents of `web_monitor/build/` to
`roh@rohbot.cc:/home/roh/hab/` using SCP. A failed install or build stops deployment.
It does not upload `deploy.sh`, source modules, tests, npm dependencies, or docs.

The generated `build/index.html` contains minified CSS and a bundled classic
JavaScript script. It makes no separate `.mjs`, `.js`, or CSS requests, avoiding
the VPS's `application/octet-stream` MIME response for `.mjs` modules. Source
modules remain unchanged for development and tests. Serve the hosted page over
HTTPS so Web Serial is available.

To build and preview without uploading:

```sh
npm --prefix web_monitor ci --include=dev
npm --prefix web_monitor run build
python3 -m http.server 8000 --bind 127.0.0.1 --directory web_monitor/build
```

Only `build/index.html` is generated; the build directory and `node_modules`
are ignored by Git. Deployment overwrites matching output files without deleting
other files already on the server.

If connecting fails, close other terminal applications using the port, check
USB permissions, and reconnect. USB removal or a stream error releases the
connection; use **Connect receiver** to reconnect. A browser connection can reset
some ESP32 USB serial boards; any boot messages remain visible in the stream.

Run the parser, framing, and connection lifecycle tests:

```sh
node --test web_monitor/tests/serial.test.mjs
```

These tests use simulated serial streams. Real device selection needs a browser
user gesture and its native permission dialog.

Alert tones use the browser's Web Audio API, with no audio files or network
requests. Browser audio needs a user interaction; see the official
[Chrome autoplay policy](https://developer.chrome.com/blog/autoplay/#web-audio).
If the page reports paused sound, click **Test beep**. The tab and system output
must be unmuted to hear it. Rapid events that arrive during a tone share that
tone rather than overlapping or queuing delayed alerts.
