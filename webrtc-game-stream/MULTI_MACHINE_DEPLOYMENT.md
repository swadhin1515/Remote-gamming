# Multi-Machine WebRTC Game Streaming Setup

This guide explains how to deploy the WebRTC game streaming system across multiple machines for remote game streaming.

## Architecture Overview

```
┌─────────────────┐         ┌──────────────────┐         ┌─────────────────┐
│  Game Machine   │         │ Signaling Server │         │  Web Client     │
│                 │         │                  │         │                 │
│  - Game runs    │◄───────►│  - WebSocket     │◄───────►│  - Browser UI   │
│  - Captures     │  WS:9000│  - SDP exchange  │  WS:9000│  - Receives     │
│    display      │         │  - ICE relay     │         │    stream       │
│  - Streams via  │         │                  │         │                 │
│    WebRTC P2P   │◄────────────────────────────────────►│                 │
│                 │      Direct WebRTC Connection         │                 │
└─────────────────┘         └──────────────────┘         └─────────────────┘
```

**Key Points:**
- **Signaling Server**: Only handles WebRTC negotiation (SDP/ICE), NOT the actual video stream
- **WebRTC Connection**: Direct peer-to-peer between game machine and client browser
- **TURN Server**: Optional relay server for NAT traversal when direct connection fails

## Prerequisites

### All Machines
- Docker and Docker Compose installed
- Network connectivity between machines
- Open firewall ports (see Network Requirements below)

### Game Machine
- Linux with X11 display server
- GPU for game rendering (optional but recommended)
- `/dev/uinput` device for input injection

## Network Requirements

### Signaling Server Machine
**Required Ports:**
- `9000/tcp` - WebSocket signaling (must be accessible from game + web clients)

### Game Machine
**Required Ports:**
- Outbound to signaling server port 9000
- **WebRTC Ports** (for direct P2P connection to clients):
  - UDP ports `10000-20000` (configurable, used for media streams)
  - Or configure TURN server for relay

### Web Client Machine
**Required Ports:**
- `8080/tcp` - HTTP web server (or any port you choose)
- Outbound to signaling server port 9000
- **WebRTC Ports**: Outbound UDP to game machine

### Firewall Configuration Examples

**Ubuntu/Debian (ufw):**
```bash
# On signaling server
sudo ufw allow 9000/tcp

# On game machine (if direct P2P)
sudo ufw allow 10000:20000/udp

# On web server
sudo ufw allow 8080/tcp
```

**CentOS/RHEL (firewalld):**
```bash
# On signaling server
sudo firewall-cmd --permanent --add-port=9000/tcp
sudo firewall-cmd --reload

# On game machine
sudo firewall-cmd --permanent --add-port=10000-20000/udp
sudo firewall-cmd --reload
```

## Deployment Steps

### 1. Deploy Signaling Server

On the **signaling server machine**:

```bash
# Clone repository
git clone <your-repo-url>
cd webrtc-game-stream

# Build and start signaling server
docker compose -f docker-compose.signaling.yml up -d

# Verify it's running
docker compose -f docker-compose.signaling.yml ps
curl http://localhost:9000  # Should get "Upgrade Required" response
```

**Note the IP address** of this machine (e.g., `192.168.1.100`)

### 2. Deploy Game Machine

On the **game machine**:

```bash
# Clone repository
git clone <your-repo-url>
cd webrtc-game-stream

# Set signaling server address
export SIGNALING_SERVER=192.168.1.100  # Replace with your signaling server IP

# Optional: Set room name and game
export ROOM=room1
export GAME_CMD="xterm -maximized -e /app/game_launcher.sh"

# Build and start game service
docker compose -f docker-compose.game.yml up -d

# Check logs
docker compose -f docker-compose.game.yml logs -f
```

**Expected log output:**
```
[STREAMER] Connecting to signaling server...
[STREAMER] Connected to signaling server
[STREAMER] Building GStreamer pipeline...
[STREAMER] Pipeline ready, waiting for browser connection...
```

### 3. Deploy Web Client

On the **web client machine** (can be same as signaling server):

```bash
# Clone repository
git clone <your-repo-url>
cd webrtc-game-stream

# Set signaling server address
export SIGNALING_SERVER=192.168.1.100  # Replace with your signaling server IP

# Build and start web server
docker compose -f docker-compose.web.yml up -d

# Verify
curl http://localhost:8080
```

### 4. Access from Browser

Open a web browser and navigate to:
```
http://<web-server-ip>:8080/?signaling=<signaling-server-ip>&room=room1
```

Example:
```
http://192.168.1.101:8080/?signaling=192.168.1.100&room=room1
```

**URL Parameters:**
- `signaling` - IP/hostname of signaling server (optional if same as web server)
- `room` - Room name to join (default: room1)

## Configuration Options

### Game Machine Environment Variables

Edit `docker-compose.game.yml` or set environment variables:

```bash
# Required
export SIGNALING_SERVER=192.168.1.100

# Optional
export ROOM=room1                    # Room name
export DISPLAY=:99                   # X11 display number
export GAME_CMD="your-game-command"  # Command to launch game
export GAME_NAME="My Game"           # Game display name
```

### Multiple Game Instances

Run multiple games on different rooms:

```bash
# Terminal 1 - Game 1
export SIGNALING_SERVER=192.168.1.100
export ROOM=game1
export GAME_CMD="xterm -e /games/snake.sh"
docker compose -f docker-compose.game.yml up

# Terminal 2 - Game 2
export SIGNALING_SERVER=192.168.1.100
export ROOM=game2
export GAME_CMD="xterm -e /games/tetris.sh"
docker compose -f docker-compose.game.yml up
```

Clients connect to different rooms:
- `http://web-server:8080/?room=game1`
- `http://web-server:8080/?room=game2`

## Troubleshooting

### Connection Issues

**Problem:** Client shows "Connection timeout"
- **Check:** Signaling server is running and accessible
- **Test:** `telnet <signaling-ip> 9000`
- **Fix:** Check firewall rules, ensure port 9000 is open

**Problem:** "No offer received — is the game running?"
- **Check:** Game service logs: `docker compose -f docker-compose.game.yml logs`
- **Verify:** Game connected to signaling server
- **Fix:** Check SIGNALING_SERVER environment variable

**Problem:** WebRTC connection fails (ICE failed)
- **Cause:** NAT/firewall blocking direct P2P connection
- **Solution:** Configure TURN server (see below)

### TURN Server Setup

If direct P2P fails due to NAT/firewall, configure a TURN server:

1. **Install coturn** on a publicly accessible server:
```bash
sudo apt-get install coturn
```

2. **Configure** `/etc/turnserver.conf`:
```
listening-port=3478
fingerprint
lt-cred-mech
user=webrtc:SecurePassword123
realm=yourdomain.com
```

3. **Update client.js** ICE servers:
```javascript
const ICE_SERVERS = [
  { urls: "stun:stun.l.google.com:19302" },
  {
    urls: "turn:your-turn-server.com:3478",
    username: "webrtc",
    credential: "SecurePassword123"
  }
];
```

### Performance Optimization

**Low FPS / Lag:**
- Reduce video bitrate in `webrtc_streamer.py`:
  ```python
  target-bitrate=3000000  # Lower from 6000000
  ```
- Adjust framerate:
  ```python
  framerate=30/1  # Lower from 60/1
  ```

**High CPU Usage:**
- Use hardware encoding (if available):
  ```python
  vaapih264enc ! rtph264pay  # Instead of vp8enc
  ```

## Security Considerations

1. **Use HTTPS/WSS in production:**
   - Configure nginx with SSL certificates
   - Change WebSocket to `wss://` instead of `ws://`

2. **Implement authentication:**
   - Add authentication to signaling server
   - Restrict room access

3. **Firewall rules:**
   - Only open required ports
   - Use VPN for internal network access

4. **TURN server credentials:**
   - Use strong passwords
   - Rotate credentials regularly
   - Use time-limited credentials

## Monitoring

### Check Service Status

```bash
# Signaling server
docker compose -f docker-compose.signaling.yml ps
docker compose -f docker-compose.signaling.yml logs -f

# Game machine
docker compose -f docker-compose.game.yml ps
docker compose -f docker-compose.game.yml logs -f

# Web server
docker compose -f docker-compose.web.yml ps
docker compose -f docker-compose.web.yml logs -f
```

### Health Checks

```bash
# Signaling server
curl http://<signaling-ip>:9000

# Web server
curl http://<web-ip>:8080

# Check WebSocket connection (from browser console)
# Should see: "Connected to signaling server"
```

## Scaling

### Multiple Game Servers

Deploy multiple game machines pointing to the same signaling server:

```bash
# Game Server 1
export SIGNALING_SERVER=192.168.1.100
export ROOM=server1
docker compose -f docker-compose.game.yml up -d

# Game Server 2
export SIGNALING_SERVER=192.168.1.100
export ROOM=server2
docker compose -f docker-compose.game.yml up -d
```

### Load Balancing

For high traffic, use nginx to load balance web clients:

```nginx
upstream web_backends {
    server 192.168.1.101:8080;
    server 192.168.1.102:8080;
}

server {
    listen 80;
    location / {
        proxy_pass http://web_backends;
    }
}
```

## Support

For issues or questions:
1. Check logs: `docker compose logs -f`
2. Verify network connectivity: `ping`, `telnet`
3. Review firewall rules
4. Check browser console for WebRTC errors
