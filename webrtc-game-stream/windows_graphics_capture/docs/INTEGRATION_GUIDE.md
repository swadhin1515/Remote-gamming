# Integration Guide: Windows Graphics Capture with WebRTC Game Streaming

This guide explains how to integrate the Windows Graphics Capture streamer with your existing WebRTC game streaming infrastructure.

## Overview

The Windows Graphics Capture (WGC) streamer acts as a **Windows-native capture source** that replaces the Linux game container when streaming from Windows machines. It captures the screen using Microsoft's official WGC API and streams via WebRTC to your existing signaling server and web clients.

## Architecture Integration

```
┌─────────────────────────────────────────────────────────────────┐
│                    Existing Infrastructure                       │
│                                                                  │
│  ┌──────────────┐         ┌──────────────┐                     │
│  │  Signaling   │◄────────┤  Web Client  │                     │
│  │  Server      │────────►│  (Browser)   │                     │
│  │  (Port 9000) │         │  (Port 8080) │                     │
│  └──────────────┘         └──────────────┘                     │
│         ▲                                                        │
└─────────┼────────────────────────────────────────────────────────┘
          │
          │ WebSocket + WebRTC
          │
┌─────────┼────────────────────────────────────────────────────────┐
│         │              Windows Machine                           │
│         ▼                                                        │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  Windows Graphics Capture Streamer                        │  │
│  │  - Captures screen/window using WGC                       │  │
│  │  - Encodes with NVENC (H.264/HEVC)                        │  │
│  │  - Streams via WebRTC                                     │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                      │
│                           ▼                                      │
│                  ┌─────────────────┐                            │
│                  │  Windows Game   │                            │
│                  │  (Fullscreen)   │                            │
│                  └─────────────────┘                            │
└─────────────────────────────────────────────────────────────────┘
```

## Deployment Scenarios

### Scenario 1: Single Windows Machine

**Use Case**: Stream games from a single Windows PC

**Setup**:
1. Run signaling server (Docker or native)
2. Run WGC streamer on Windows
3. Access from web browser

```bash
# On Linux server (or Windows with Docker)
cd /root/webrtc-game-stream
docker compose up signaling web

# On Windows machine
cd windows_graphics_capture\build\bin\Release
WindowsGraphicsCapture.exe
```

### Scenario 2: Multiple Windows Machines

**Use Case**: Stream from multiple Windows gaming PCs to a central server

**Setup**:
1. Central signaling server (accessible from all machines)
2. WGC streamer on each Windows machine
3. Web clients connect to central server

**Configuration** (on each Windows machine):
```json
{
  "webrtc": {
    "signalingServerUrl": "ws://central-server.example.com:9000",
    "stunServer": "stun:stun.l.google.com:19302",
    "turnServers": [
      "turn:turn-server.example.com:3478"
    ]
  }
}
```

### Scenario 3: Hybrid Linux + Windows

**Use Case**: Support both Linux and Windows game streaming

**Setup**:
- Linux games: Use existing Docker game container
- Windows games: Use WGC streamer
- Same signaling server and web client for both

## Step-by-Step Integration

### 1. Prepare Signaling Server

Ensure your signaling server is accessible from the Windows machine:

```bash
# Check if signaling server is running
curl http://localhost:9000

# Or test WebSocket connection
wscat -c ws://localhost:9000
```

**For remote access**, update firewall rules:
```bash
# Allow port 9000 (signaling)
sudo ufw allow 9000/tcp

# Allow WebRTC ports (if using TURN)
sudo ufw allow 3478/tcp
sudo ufw allow 3478/udp
```

### 2. Configure WGC Streamer

Edit `config.json` on Windows machine:

```json
{
  "webrtc": {
    "signalingServerUrl": "ws://YOUR_SERVER_IP:9000",
    "stunServer": "stun:stun.l.google.com:19302"
  }
}
```

Replace `YOUR_SERVER_IP` with:
- `localhost` for local testing
- LAN IP (e.g., `192.168.1.100`) for local network
- Public IP or domain for internet access

### 3. Start the Streamer

```cmd
WindowsGraphicsCapture.exe --config config.json
```

The streamer will:
1. Connect to signaling server
2. Show window/monitor picker (if configured)
3. Start capturing and encoding
4. Wait for WebRTC peer connection

### 4. Connect from Web Client

Open browser to your web interface:
```
http://YOUR_SERVER_IP:8080
```

The web client will:
1. Connect to signaling server
2. Establish WebRTC connection with WGC streamer
3. Display the video stream

## Network Configuration

### Local Network (LAN)

**No special configuration needed** - WebRTC will use direct peer-to-peer connection.

```json
{
  "webrtc": {
    "signalingServerUrl": "ws://192.168.1.100:9000",
    "stunServer": "stun:stun.l.google.com:19302"
  }
}
```

### Internet (WAN)

**Requires TURN server** for NAT traversal:

```json
{
  "webrtc": {
    "signalingServerUrl": "ws://your-domain.com:9000",
    "stunServer": "stun:stun.l.google.com:19302",
    "turnServers": [
      "turn:your-turn-server.com:3478?transport=udp",
      "turn:your-turn-server.com:3478?transport=tcp"
    ]
  }
}
```

**Setup TURN server** (optional, for internet streaming):
```bash
# Install coturn
sudo apt-get install coturn

# Configure /etc/turnserver.conf
listening-port=3478
realm=your-domain.com
server-name=your-domain.com
lt-cred-mech
user=username:password

# Start coturn
sudo systemctl start coturn
```

## Signaling Protocol

The WGC streamer uses the same signaling protocol as your existing system:

### Message Types

**1. Offer (from WGC streamer)**
```json
{
  "type": "offer",
  "sdp": "v=0\r\no=- ...\r\n..."
}
```

**2. Answer (from web client)**
```json
{
  "type": "answer",
  "sdp": "v=0\r\no=- ...\r\n..."
}
```

**3. ICE Candidate**
```json
{
  "type": "ice-candidate",
  "candidate": {
    "candidate": "candidate:...",
    "sdpMid": "video",
    "sdpMLineIndex": 0
  }
}
```

## Modifying Existing Signaling Server

If your signaling server needs updates to support WGC streamer:

### Add Session Management

```javascript
// Track active sessions
const sessions = new Map();

ws.on('message', (message) => {
  const data = JSON.parse(message);
  
  if (data.type === 'offer') {
    // Store session
    const sessionId = generateSessionId();
    sessions.set(sessionId, {
      streamer: ws,
      client: null,
      offer: data.sdp
    });
    
    // Broadcast to clients
    broadcastToClients({ type: 'offer', sessionId, sdp: data.sdp });
  }
});
```

### Handle Multiple Streamers

```javascript
// Support multiple concurrent streams
const streamers = new Map();

function registerStreamer(ws, streamerId) {
  streamers.set(streamerId, {
    socket: ws,
    clients: new Set()
  });
}

function routeToStreamer(clientWs, streamerId) {
  const streamer = streamers.get(streamerId);
  if (streamer) {
    streamer.clients.add(clientWs);
    // Forward messages between client and streamer
  }
}
```

## Web Client Integration

Your existing web client should work with minimal changes:

### Update Connection Logic

```javascript
// Connect to signaling server
const ws = new WebSocket('ws://localhost:9000');

ws.onmessage = async (event) => {
  const data = JSON.parse(event.data);
  
  if (data.type === 'offer') {
    // Create peer connection
    const pc = new RTCPeerConnection({
      iceServers: [
        { urls: 'stun:stun.l.google.com:19302' }
      ]
    });
    
    // Set remote description
    await pc.setRemoteDescription({
      type: 'offer',
      sdp: data.sdp
    });
    
    // Create answer
    const answer = await pc.createAnswer();
    await pc.setLocalDescription(answer);
    
    // Send answer
    ws.send(JSON.stringify({
      type: 'answer',
      sdp: answer.sdp
    }));
    
    // Handle video track
    pc.ontrack = (event) => {
      videoElement.srcObject = event.streams[0];
    };
  }
};
```

## Performance Tuning

### Optimize for Low Latency

```json
{
  "encoder": {
    "preset": "ultra_low_latency",
    "bitrate": 5000000,
    "gopSize": 30,
    "enableBFrames": false
  },
  "performance": {
    "maxLatencyMs": 30
  }
}
```

### Optimize for Quality

```json
{
  "encoder": {
    "preset": "quality",
    "bitrate": 10000000,
    "gopSize": 60,
    "enableBFrames": true
  }
}
```

### Optimize for Bandwidth

```json
{
  "encoder": {
    "codec": "hevc",
    "bitrate": 3000000,
    "preset": "low_latency"
  }
}
```

## Monitoring and Debugging

### Enable Detailed Logging

```json
{
  "logging": {
    "enabled": true,
    "logFilePath": "wgc_capture_debug.log"
  }
}
```

### Monitor Statistics

The streamer outputs statistics every 5 seconds:

```
=== Statistics ===
Capture:
  Frames captured: 3000
  Frames dropped: 5
  Current FPS: 60.0
  Avg capture latency: 2.1 ms

Encoder:
  Frames encoded: 2995
  Total bytes: 15728640
  Avg encode latency: 5.3 ms

WebRTC:
  Frames sent: 2990
  Avg network latency: 12.5 ms
==================
```

### Debug WebRTC Connection

```javascript
// In web client
pc.oniceconnectionstatechange = () => {
  console.log('ICE state:', pc.iceConnectionState);
};

pc.onconnectionstatechange = () => {
  console.log('Connection state:', pc.connectionState);
};
```

## Troubleshooting Integration

### Streamer Can't Connect to Signaling Server

1. Check firewall on server
2. Verify signaling server is running
3. Test with `telnet SERVER_IP 9000`
4. Check signaling server logs

### WebRTC Connection Fails

1. Check STUN/TURN configuration
2. Verify NAT traversal
3. Test with local network first
4. Check browser console for errors

### No Video in Browser

1. Verify codec support (H.264 widely supported)
2. Check browser WebRTC support
3. Inspect SDP offer/answer
4. Monitor network traffic

## Best Practices

1. **Use HEVC for better compression** (if browser supports)
2. **Enable hardware acceleration** for best performance
3. **Configure TURN server** for internet streaming
4. **Monitor statistics** to optimize settings
5. **Test on local network** before deploying to internet
6. **Use secure WebSocket (wss://)** in production
7. **Implement session management** for multiple users
8. **Add authentication** to signaling server

## Example: Complete Integration

See the main project's `docker-compose.yml` for reference:

```yaml
services:
  signaling:
    # Your existing signaling server
    ports:
      - "9000:9000"
  
  web:
    # Your existing web client
    ports:
      - "8080:80"
```

Then on Windows:
```cmd
WindowsGraphicsCapture.exe --config config.json
```

Access from browser:
```
http://localhost:8080
```

## Support

For integration issues:
1. Check signaling server logs
2. Review WGC streamer logs
3. Inspect browser console
4. Verify network connectivity
5. Test with simplified configuration

## Next Steps

- Set up TURN server for internet access
- Implement authentication
- Add multi-user support
- Monitor performance metrics
- Deploy to production
