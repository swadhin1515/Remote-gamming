# WebRTC Game Streaming Implementation Plan

## Overview
This document provides a complete implementation plan for a WebRTC-based game streaming system with bidirectional control. The system consists of 3 Docker containers that work together to stream video from a game and receive input commands back.

## Architecture

```
┌─────────────────┐
│   Signaling     │  WebSocket Server (Port 9000)
│   Container     │  - Relays SDP/ICE between peers
└────────┬────────┘  - Room-based message routing
         │
    ┌────┴────┐
    │         │
┌───▼─────────▼───┐
│  Game Container │  Container A
│  - Xvfb (X11)   │  - Captures display via ximagesrc
│  - Game Process │  - Encodes H.264 with x264enc
│  - GStreamer    │  - Sends via WebRTC (webrtcbin)
│  - Input Inject │  - Receives input via DataChannel
└─────────────────┘  - Injects via uinput
         │
         │ WebRTC (video + data channel)
         │
┌────────▼─────────┐
│  Web Container   │  Container B (Port 8080)
│  - Nginx         │  - Serves HTML/JS client
│  - HTML Client   │  - Receives WebRTC stream
│  - RTCPeerConn   │  - Sends keyboard/mouse input
└──────────────────┘
```

## Project Structure

```
webrtc-game-stream/
├── docker-compose.yml
├── README.md
├── signaling/
│   ├── Dockerfile
│   └── server.js
├── game/
│   ├── Dockerfile
│   ├── start.sh
│   ├── webrtc_streamer.py
│   └── requirements.txt
└── web/
    ├── Dockerfile
    ├── nginx.conf
    ├── index.html
    └── client.js
```

## Implementation Steps

### Step 1: Create Project Directory Structure
```bash
mkdir -p webrtc-game-stream/{signaling,game,web}
cd webrtc-game-stream
```

### Step 2: Docker Compose Configuration

**File: `docker-compose.yml`**
```yaml
services:
  signaling:
    build: ./signaling
    ports:
      - "9000:9000"
    networks: [qa-net]

  game:
    build: ./game
    depends_on: [signaling]
    networks: [qa-net]
    environment:
      - SIGNALING_URL=ws://signaling:9000
      - ROOM=room1
      - DISPLAY=:99
      # Replace with your real game command later:
      - GAME_CMD=glxgears
    # Needed for input injection via uinput:
    privileged: true
    devices:
      - /dev/uinput:/dev/uinput

  web:
    build: ./web
    depends_on: [signaling]
    ports:
      - "8080:80"
    networks: [qa-net]

networks:
  qa-net:
    driver: bridge
```

### Step 3: Signaling Server Implementation

**File: `signaling/Dockerfile`**
```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY server.js .
RUN npm init -y && npm i ws
EXPOSE 9000
CMD ["node", "server.js"]
```

**File: `signaling/server.js`**
```javascript
const WebSocket = require("ws");
const wss = new WebSocket.Server({ port: 9000 });

/**
 * Minimal signaling: broadcast messages to others in same room.
 * Message format:
 * { room: "room1", type: "offer|answer|ice", data: ... }
 */
const rooms = new Map(); // room -> Set(ws)

function join(room, ws) {
  if (!rooms.has(room)) rooms.set(room, new Set());
  rooms.get(room).add(ws);
  ws._room = room;
}

function leave(ws) {
  const room = ws._room;
  if (!room) return;
  const set = rooms.get(room);
  if (set) {
    set.delete(ws);
    if (set.size === 0) rooms.delete(room);
  }
}

wss.on("connection", (ws) => {
  ws.on("message", (msg) => {
    let m;
    try { m = JSON.parse(msg.toString()); } catch { return; }
    if (!m.room || !m.type) return;

    if (!ws._room) join(m.room, ws);

    // Relay to others in the room
    const peers = rooms.get(m.room) || new Set();
    for (const p of peers) {
      if (p !== ws && p.readyState === WebSocket.OPEN) {
        p.send(JSON.stringify(m));
      }
    }
  });

  ws.on("close", () => leave(ws));
});

console.log("Signaling server running on ws://0.0.0.0:9000");
```

### Step 4: Game Container Implementation

**File: `game/Dockerfile`**
```dockerfile
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
  python3 python3-pip \
  xvfb x11-xserver-utils xauth \
  mesa-utils \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav \
  python3-gi gir1.2-gstreamer-1.0 gir1.2-gst-plugins-bad-1.0 \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY requirements.txt .
RUN pip3 install --no-cache-dir -r requirements.txt

COPY webrtc_streamer.py start.sh ./
RUN chmod +x /app/start.sh

CMD ["/app/start.sh"]
```

**File: `game/requirements.txt`**
```
websockets==12.0
evdev==1.7.1
```

**File: `game/start.sh`**
```bash
#!/usr/bin/env bash
set -e

echo "[game] Starting Xvfb on ${DISPLAY}..."
Xvfb ${DISPLAY} -screen 0 1280x720x24 +extension RANDR &
sleep 0.5

echo "[game] Launching game: ${GAME_CMD}"
bash -lc "${GAME_CMD}" &

echo "[game] Starting WebRTC streamer..."
python3 /app/webrtc_streamer.py
```

**File: `game/webrtc_streamer.py`**
```python
import asyncio
import json
import os
import websockets

import gi
gi.require_version("Gst", "1.0")
gi.require_version("GstWebRTC", "1.0")
from gi.repository import Gst, GObject, GstWebRTC

from evdev import UInput, ecodes as e

Gst.init(None)
GObject.threads_init()

SIGNALING_URL = os.environ.get("SIGNALING_URL", "ws://signaling:9000")
ROOM = os.environ.get("ROOM", "room1")
DISPLAY = os.environ.get("DISPLAY", ":99")

# Simple virtual input device
UI_CAPS = {
    e.EV_KEY: [
        e.KEY_W, e.KEY_A, e.KEY_S, e.KEY_D,
        e.KEY_UP, e.KEY_DOWN, e.KEY_LEFT, e.KEY_RIGHT,
        e.KEY_SPACE, e.KEY_ENTER, e.KEY_ESC,
    ],
    e.EV_REL: [e.REL_X, e.REL_Y, e.REL_WHEEL],
    e.EV_KEY: [e.BTN_LEFT, e.BTN_RIGHT] + [
        e.KEY_W, e.KEY_A, e.KEY_S, e.KEY_D,
        e.KEY_UP, e.KEY_DOWN, e.KEY_LEFT, e.KEY_RIGHT,
        e.KEY_SPACE, e.KEY_ENTER, e.KEY_ESC,
    ],
}

ui = UInput(UI_CAPS, name="webrtc-remote-input", bustype=e.BUS_USB)

def inject(evt: dict):
    """evt: {"t":"key","code":"KeyW","down":true} or {"t":"mouse","dx":1,"dy":-2,"btn":...}"""
    t = evt.get("t")
    if t == "key":
        code = evt.get("code")
        down = 1 if evt.get("down") else 0
        keymap = {
            "KeyW": e.KEY_W, "KeyA": e.KEY_A, "KeyS": e.KEY_S, "KeyD": e.KEY_D,
            "ArrowUp": e.KEY_UP, "ArrowDown": e.KEY_DOWN, "ArrowLeft": e.KEY_LEFT, "ArrowRight": e.KEY_RIGHT,
            "Space": e.KEY_SPACE, "Enter": e.KEY_ENTER, "Escape": e.KEY_ESC,
        }
        if code in keymap:
            ui.write(e.EV_KEY, keymap[code], down)
            ui.syn()

    elif t == "mouse":
        dx = int(evt.get("dx", 0))
        dy = int(evt.get("dy", 0))
        if dx or dy:
            ui.write(e.EV_REL, e.REL_X, dx)
            ui.write(e.EV_REL, e.REL_Y, dy)
            ui.syn()
        btn = evt.get("btn")
        if btn in ("left", "right"):
            pressed = 1 if evt.get("down") else 0
            ui.write(e.EV_KEY, e.BTN_LEFT if btn == "left" else e.BTN_RIGHT, pressed)
            ui.syn()

class WebRTCStreamer:
    def __init__(self):
        self.pipe = None
        self.webrtc = None
        self.ws = None
        self.data_channel = None

    async def connect_signaling(self):
        self.ws = await websockets.connect(SIGNALING_URL)
        # Join room (first message will auto-join on server)
        await self.ws.send(json.dumps({"room": ROOM, "type": "join", "data": "game"}))

    def build_pipeline(self):
        # ximagesrc capture from Xvfb; encode with x264enc tuned for low latency
        desc = f"""
          webrtcbin name=webrtc bundle-policy=max-bundle
          ximagesrc display-name={DISPLAY} use-damage=0 !
            video/x-raw,framerate=60/1 !
            videoconvert !
            queue !
            x264enc tune=zerolatency speed-preset=ultrafast bitrate=6000 key-int-max=60 !
            rtph264pay config-interval=1 pt=96 !
            application/x-rtp,media=video,encoding-name=H264,payload=96 !
            webrtc.
        """
        self.pipe = Gst.parse_launch(desc)
        self.webrtc = self.pipe.get_by_name("webrtc")

        self.webrtc.connect("on-negotiation-needed", self.on_negotiation_needed)
        self.webrtc.connect("on-ice-candidate", self.on_ice_candidate)

        # Create a data channel for inputs (supported by webrtcbin)
        self.data_channel = self.webrtc.emit("create-data-channel", "input", None)
        self.data_channel.connect("on-message-string", self.on_data_message)

    def on_data_message(self, channel, msg):
        try:
            evt = json.loads(msg)
            inject(evt)
        except Exception:
            pass

    def on_ice_candidate(self, element, mlineindex, candidate):
        asyncio.get_event_loop().create_task(
            self.ws.send(json.dumps({
                "room": ROOM,
                "type": "ice",
                "data": {"mlineindex": mlineindex, "candidate": candidate}
            }))
        )

    def on_negotiation_needed(self, element):
        promise = Gst.Promise.new_with_change_func(self.on_offer_created, element, None)
        element.emit("create-offer", None, promise)

    def on_offer_created(self, promise, element, _):
        promise.wait()
        reply = promise.get_reply()
        offer = reply.get_value("offer")
        element.emit("set-local-description", offer, Gst.Promise.new())

        sdp_text = offer.sdp.as_text()
        asyncio.get_event_loop().create_task(
            self.ws.send(json.dumps({"room": ROOM, "type": "offer", "data": sdp_text}))
        )

    async def signaling_loop(self):
        async for msg in self.ws:
            m = json.loads(msg)
            t = m.get("type")
            if t == "answer":
                sdp = GstWebRTC.WebRTCSessionDescription.new(
                    GstWebRTC.WebRTCSDPType.ANSWER,
                    Gst.SDPMessage.new_from_text(m["data"])[1]
                )
                self.webrtc.emit("set-remote-description", sdp, Gst.Promise.new())

            elif t == "offer":
                # If browser offers instead (depending on your client flow)
                sdp = GstWebRTC.WebRTCSessionDescription.new(
                    GstWebRTC.WebRTCSDPType.OFFER,
                    Gst.SDPMessage.new_from_text(m["data"])[1]
                )
                self.webrtc.emit("set-remote-description", sdp, Gst.Promise.new())
                promise = Gst.Promise.new_with_change_func(self.on_answer_created, self.webrtc, None)
                self.webrtc.emit("create-answer", None, promise)

            elif t == "ice":
                data = m["data"]
                self.webrtc.emit("add-ice-candidate", data["mlineindex"], data["candidate"])

    def on_answer_created(self, promise, element, _):
        promise.wait()
        reply = promise.get_reply()
        answer = reply.get_value("answer")
        element.emit("set-local-description", answer, Gst.Promise.new())
        sdp_text = answer.sdp.as_text()
        asyncio.get_event_loop().create_task(
            self.ws.send(json.dumps({"room": ROOM, "type": "answer", "data": sdp_text}))
        )

    def start(self):
        self.pipe.set_state(Gst.State.PLAYING)

async def main():
    s = WebRTCStreamer()
    await s.connect_signaling()
    s.build_pipeline()
    s.start()

    # WebRTC requires exchanging SDP/ICE over signaling before it can connect
    await s.signaling_loop()

if __name__ == "__main__":
    asyncio.run(main())
```

### Step 5: Web Container Implementation

**File: `web/Dockerfile`**
```dockerfile
FROM nginx:alpine
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY index.html /usr/share/nginx/html/index.html
COPY client.js /usr/share/nginx/html/client.js
```

**File: `web/nginx.conf`**
```nginx
server {
  listen 80;
  server_name _;
  location / {
    root /usr/share/nginx/html;
    index index.html;
  }
}
```

**File: `web/index.html`**
```html
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <title>WebRTC Game Stream</title>
  <style>
    body {
      margin: 0;
      padding: 20px;
      background: #1a1a1a;
      color: #fff;
      font-family: Arial, sans-serif;
    }
    #container {
      max-width: 1280px;
      margin: 0 auto;
    }
    h1 {
      text-align: center;
      color: #4CAF50;
    }
    #v {
      width: 100%;
      max-width: 1280px;
      background: #000;
      display: block;
      margin: 20px auto;
      border: 2px solid #4CAF50;
      border-radius: 8px;
    }
    #status {
      text-align: center;
      padding: 10px;
      background: #333;
      border-radius: 4px;
      margin: 10px 0;
    }
    .info {
      background: #2a2a2a;
      padding: 15px;
      border-radius: 4px;
      margin: 10px 0;
    }
    .info h3 {
      margin-top: 0;
      color: #4CAF50;
    }
    .controls {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
      margin: 10px 0;
    }
    .control-item {
      background: #333;
      padding: 8px;
      border-radius: 4px;
    }
  </style>
</head>
<body>
  <div id="container">
    <h1>🎮 WebRTC Game Stream</h1>
    <div id="status">Connecting...</div>
    <video id="v" autoplay playsinline muted></video>
    
    <div class="info">
      <h3>Controls</h3>
      <div class="controls">
        <div class="control-item"><strong>WASD:</strong> Movement</div>
        <div class="control-item"><strong>Arrow Keys:</strong> Alternative movement</div>
        <div class="control-item"><strong>Space:</strong> Jump/Action</div>
        <div class="control-item"><strong>Enter:</strong> Confirm</div>
        <div class="control-item"><strong>Escape:</strong> Menu</div>
        <div class="control-item"><strong>Mouse:</strong> Look/Aim</div>
      </div>
    </div>

    <div class="info">
      <h3>Instructions</h3>
      <ul>
        <li>Click on the video to focus and enable input</li>
        <li>Your keyboard and mouse inputs will be sent to the game</li>
        <li>Low latency streaming via WebRTC</li>
      </ul>
    </div>
  </div>
  <script src="client.js"></script>
</body>
</html>
```

**File: `web/client.js`**
```javascript
const video = document.getElementById("v");
const status = document.getElementById("status");
const room = "room1";
const ws = new WebSocket(`ws://${location.hostname}:9000`);

let pc, dc;

function setStatus(msg) {
  status.textContent = msg;
  console.log(msg);
}

ws.onopen = async () => {
  setStatus("Connected to signaling server");
  await start();
};

ws.onerror = () => {
  setStatus("❌ Failed to connect to signaling server");
};

async function start() {
  pc = new RTCPeerConnection({
    iceServers: [{ urls: "stun:stun.l.google.com:19302" }]
  });

  // Receive media
  pc.ontrack = (ev) => {
    setStatus("✅ Receiving video stream");
    video.srcObject = ev.streams[0];
  };

  pc.onconnectionstatechange = () => {
    setStatus(`Connection: ${pc.connectionState}`);
  };

  // Data channel for inputs (either side can create; here we'll create)
  dc = pc.createDataChannel("input");
  dc.onopen = () => {
    setStatus("✅ Data channel open - inputs enabled");
  };
  dc.onclose = () => {
    setStatus("⚠️ Data channel closed");
  };

  pc.onicecandidate = (ev) => {
    if (ev.candidate) {
      ws.send(JSON.stringify({ room, type: "ice", data: ev.candidate }));
    }
  };

  // We wait for offer from the game container, then answer.
  ws.onmessage = async (msg) => {
    const m = JSON.parse(msg.data);
    if (m.room !== room) return;

    if (m.type === "offer") {
      setStatus("Received offer, creating answer...");
      await pc.setRemoteDescription({ type: "offer", sdp: m.data });
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);
      ws.send(JSON.stringify({ room, type: "answer", data: answer.sdp }));
    } else if (m.type === "ice") {
      await pc.addIceCandidate(m.data);
    }
  };

  // Tell signaling we're in the room
  ws.send(JSON.stringify({ room, type: "join", data: "web" }));

  installInputHandlers();
}

function send(evt) {
  if (dc && dc.readyState === "open") {
    dc.send(JSON.stringify(evt));
  }
}

function installInputHandlers() {
  video.addEventListener("click", () => {
    video.focus();
    setStatus("✅ Video focused - inputs active");
  });

  window.addEventListener("keydown", (e) => {
    send({ t: "key", code: e.code, down: true });
    e.preventDefault();
  });

  window.addEventListener("keyup", (e) => {
    send({ t: "key", code: e.code, down: false });
    e.preventDefault();
  });

  // Mouse relative movement
  let lastX = null, lastY = null;
  window.addEventListener("mousemove", (e) => {
    if (lastX === null) { lastX = e.clientX; lastY = e.clientY; return; }
    const dx = e.clientX - lastX;
    const dy = e.clientY - lastY;
    lastX = e.clientX; lastY = e.clientY;
    if (dx || dy) send({ t: "mouse", dx, dy });
  });

  window.addEventListener("mousedown", (e) => {
    if (e.button === 0) send({ t: "mouse", btn: "left", down: true });
    if (e.button === 2) send({ t: "mouse", btn: "right", down: true });
    e.preventDefault();
  });

  window.addEventListener("mouseup", (e) => {
    if (e.button === 0) send({ t: "mouse", btn: "left", down: false });
    if (e.button === 2) send({ t: "mouse", btn: "right", down: false });
    e.preventDefault();
  });

  // Prevent context menu
  window.addEventListener("contextmenu", (e) => e.preventDefault());
}
```

### Step 6: README Documentation

**File: `README.md`**
```markdown
# WebRTC Game Streaming System

A complete Docker-based solution for streaming games over WebRTC with bidirectional input control.

## Architecture

- **Signaling Container**: WebSocket server for SDP/ICE exchange
- **Game Container**: Runs game, captures video, streams via WebRTC, receives input
- **Web Container**: Serves HTML client for viewing stream and sending input

## Features

- ✅ Low-latency video streaming via WebRTC
- ✅ H.264 encoding with x264enc (ultrafast preset)
- ✅ Bidirectional communication via WebRTC DataChannel
- ✅ Keyboard and mouse input injection via Linux uinput
- ✅ Headless X11 server (Xvfb) for game rendering
- ✅ Room-based signaling for multiple sessions

## Quick Start

### Prerequisites

- Docker and Docker Compose
- Linux host (for uinput support)

### Build and Run

```bash
# Clone or create the project structure
cd webrtc-game-stream

# Build and start all containers
docker compose up --build

# Access the web client
open http://localhost:8080
```

### Using Your Own Game

Edit `docker-compose.yml` and change the `GAME_CMD` environment variable:

```yaml
environment:
  - GAME_CMD=/path/to/your/game --args
```

## Configuration

### Environment Variables (Game Container)

- `SIGNALING_URL`: WebSocket URL for signaling server (default: `ws://signaling:9000`)
- `ROOM`: Room name for WebRTC session (default: `room1`)
- `DISPLAY`: X11 display number (default: `:99`)
- `GAME_CMD`: Command to launch the game (default: `glxgears`)

### Ports

- `9000`: Signaling server (WebSocket)
- `8080`: Web client (HTTP)

## Input Controls

- **WASD**: Movement keys
- **Arrow Keys**: Alternative movement
- **Space**: Jump/Action
- **Enter**: Confirm
- **Escape**: Menu
- **Mouse**: Look/Aim/Click

## Advanced Configuration

### GPU Acceleration

For better performance with real games, enable GPU passthrough:

**NVIDIA GPU:**
```yaml
game:
  runtime: nvidia
  environment:
    - NVIDIA_VISIBLE_DEVICES=all
    - NVIDIA_DRIVER_CAPABILITIES=all
```

**AMD/Intel GPU:**
```yaml
game:
  devices:
    - /dev/dri:/dev/dri
```

### Audio Support

Add PulseAudio to the game container:

```dockerfile
RUN apt-get install -y pulseaudio
```

Update GStreamer pipeline to include audio:
```python
desc = f"""
  webrtcbin name=webrtc bundle-policy=max-bundle
  ximagesrc display-name={DISPLAY} use-damage=0 !
    video/x-raw,framerate=60/1 !
    videoconvert ! queue !
    x264enc tune=zerolatency speed-preset=ultrafast bitrate=6000 key-int-max=60 !
    rtph264pay config-interval=1 pt=96 !
    application/x-rtp,media=video,encoding-name=H264,payload=96 !
    webrtc.
  pulsesrc ! audioconvert ! audioresample !
    opusenc ! rtpopuspay pt=97 !
    application/x-rtp,media=audio,encoding-name=OPUS,payload=97 !
    webrtc.
"""
```

### TURN Server (for WAN access)

For internet access, add a TURN server:

```yaml
game:
  environment:
    - TURN_URL=turn:your-turn-server.com:3478
    - TURN_USER=username
    - TURN_PASS=password
```

Update client.js:
```javascript
pc = new RTCPeerConnection({
  iceServers: [
    { urls: "stun:stun.l.google.com:19302" },
    { 
      urls: "turn:your-turn-server.com:3478",
      username: "username",
      credential: "password"
    }
  ]
});
```

### Security Hardening

1. **Use WSS (WebSocket Secure)**:
   - Add TLS certificates to signaling server
   - Update `SIGNALING_URL` to `wss://`

2. **Add Authentication**:
   - Implement JWT tokens for room access
   - Validate tokens in signaling server

3. **Remove privileged mode**:
   ```yaml
   game:
     cap_add:
       - SYS_ADMIN
     devices:
       - /dev/uinput:/dev/uinput
   ```

## Troubleshooting

### No video stream

1. Check signaling connection: `docker logs webrtc-game-stream-signaling-1`
2. Check game container: `docker logs webrtc-game-stream-game-1`
3. Verify WebRTC connection in browser console

### Input not working

1. Ensure `/dev/uinput` is available on host
2. Check container has `privileged: true` or proper capabilities
3. Verify data channel is open (check browser console)

### High latency

1. Use hardware encoding (NVENC/VAAPI) instead of x264enc
2. Reduce bitrate in GStreamer pipeline
3. Add TURN server for better routing

## Technical Details

### WebRTC Pipeline

The game container uses GStreamer's `webrtcbin` element which implements:
- SDP offer/answer negotiation
- ICE candidate gathering
- DTLS/SRTP encryption
- RTP packet handling
- DataChannel support

### Input Injection

Input events are:
1. Captured in browser (keyboard/mouse events)
2. Sent via WebRTC DataChannel (JSON format)
3. Received by Python script
4. Injected via Linux uinput kernel module
5. Delivered to game as real input events

### Signaling Protocol

Messages exchanged via WebSocket:
```json
{
  "room": "room1",
  "type": "offer|answer|ice|join",
  "data": "<SDP or ICE candidate>"
}
```

## Performance Optimization

1. **Encoder Settings**:
   - `tune=zerolatency`: Minimize encoding delay
   - `speed-preset=ultrafast`: Fastest encoding
   - `key-int-max=60`: Keyframe every 60 frames

2. **Network**:
   - Use STUN for NAT traversal
   - Use TURN for restrictive networks
   - Enable bundle-policy=max-bundle

3. **Display**:
   - Match Xvfb resolution to stream resolution
   - Use 60fps for smooth gameplay

## License

MIT License - feel free to modify and use for your projects.

## References

- [GStreamer WebRTC Documentation](https://gstreamer.freedesktop.org/documentation/webrtc/index.html)
- [WebRTC DataChannels](https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API/Using_data_channels)
- [Linux uinput](https://www.kernel.org/doc/html/latest/input/uinput.html)
```

## Next Steps

To implement this system, you need to:

1. **Switch to 'code' mode** to create the actual files
2. Create the directory structure
3. Create all the file