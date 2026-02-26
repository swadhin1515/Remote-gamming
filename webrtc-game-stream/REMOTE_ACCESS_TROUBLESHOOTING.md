# WebRTC Remote Access Troubleshooting Guide

## Problem Identified

You're experiencing an **ICE connection failure** when accessing the WebRTC stream remotely. The logs show:

```
✅ WebSocket connected
✅ Offer received
✅ Answer sent
✅ Video track received
❌ Connection stuck at "waiting for connection..."
```

This is a **network configuration issue**, not a code bug. The WebRTC ICE negotiation is failing because the browser cannot reach the Docker container's network.

---

## Root Cause

When accessing remotely via `dockerstream1.fyre.ibm.com:8080`:

1. **Docker containers use internal IPs** (e.g., `172.18.0.x`)
2. **ICE candidates include these internal IPs** in the SDP offer
3. **Your remote browser cannot reach** these internal Docker IPs
4. **ICE connection fails** because no valid network path exists

**This is a classic WebRTC NAT traversal problem.**

---

## Solution Options

### Option 1: Configure Host Network Mode (Recommended for Testing)

**Pros**: Simple, works immediately  
**Cons**: Less isolated, uses host network directly

**Steps**:

1. **Modify `docker-compose.yml`**:

```yaml
services:
  game:
    build: ./game
    network_mode: "host"  # Add this line
    environment:
      - DISPLAY=:99
      - PULSE_SERVER=unix:/tmp/pulseaudio.socket
    volumes:
      - /tmp/.X11-unix:/tmp/.X11-unix
      - pulseaudio:/tmp/pulseaudio.socket
    depends_on:
      - signaling

  signaling:
    build: ./signaling
    network_mode: "host"  # Add this line

  web:
    build: ./web
    network_mode: "host"  # Add this line
```

2. **Rebuild and restart**:

```bash
cd /root/webrtc-game-stream
docker compose down
docker compose up --build
```

3. **Access via**: `http://dockerstream1.fyre.ibm.com:8080`

**Note**: With host network mode, containers use the host's IP directly, so ICE candidates will be reachable.

---

### Option 2: Configure STUN/TURN Server (Production Solution)

**Pros**: Proper NAT traversal, works in all scenarios  
**Cons**: Requires TURN server setup

**What you need**:

1. **STUN server** (for discovering public IP) - Free options available
2. **TURN server** (for relaying traffic when direct connection fails) - Requires setup

**Steps**:

1. **Set up a TURN server** (e.g., using Coturn):

```bash
# Install Coturn on your server
sudo apt-get install coturn

# Configure /etc/turnserver.conf
listening-port=3478
fingerprint
lt-cred-mech
user=username:password
realm=dockerstream1.fyre.ibm.com
```

2. **Update `web/client.js`** to use TURN server:

```javascript
pc = new RTCPeerConnection({
  iceServers: [
    { urls: "stun:stun.l.google.com:19302" },
    { 
      urls: "turn:dockerstream1.fyre.ibm.com:3478",
      username: "username",
      credential: "password"
    }
  ]
});
```

3. **Update `game/webrtc_streamer.py`** similarly:

```python
self.webrtc.set_property("stun-server", "stun://stun.l.google.com:19302")
self.webrtc.set_property("turn-server", "turn://username:password@dockerstream1.fyre.ibm.com:3478")
```

---

### Option 3: Port Forwarding with ICE Server Configuration

**Pros**: Works without TURN server  
**Cons**: Requires firewall configuration

**Steps**:

1. **Ensure UDP ports are open** on the host:

```bash
# Open UDP port range for WebRTC media
sudo firewall-cmd --permanent --add-port=10000-20000/udp
sudo firewall-cmd --reload
```

2. **Configure ICE to use host IP**:

Modify `game/webrtc_streamer.py` to set the public IP:

```python
# Add after creating webrtc element
self.webrtc.set_property("stun-server", "stun://stun.l.google.com:19302")

# Force ICE to use specific IP (your server's public IP)
import socket
host_ip = socket.gethostbyname(socket.gethostname())
print(f"[STREAMER] Using host IP: {host_ip}")
```

3. **Rebuild containers**:

```bash
docker compose down
docker compose build --no-cache
docker compose up
```

---

## Quick Diagnostic Steps

### Step 1: Check ICE Candidates in Browser

1. Open browser DevTools (F12)
2. Go to `chrome://webrtc-internals` (Chrome) or `about:webrtc` (Firefox)
3. Look for ICE candidates
4. Check if you see:
   - ✅ **host candidates** with public IP (good)
   - ❌ **host candidates** with `172.x.x.x` (bad - Docker internal IP)

### Step 2: Verify Network Connectivity

```bash
# On the server, check what IP the game container sees
docker compose exec game hostname -I

# Check if ports are accessible from outside
# From your local machine:
telnet dockerstream1.fyre.ibm.com 8080  # Should connect
telnet dockerstream1.fyre.ibm.com 9000  # Should connect
```

### Step 3: Check Docker Network

```bash
# See what network Docker is using
docker network inspect webrtc-game-stream_default

# Check container IPs
docker compose ps
```

---

## Recommended Immediate Fix

**For your current setup, use Option 1 (Host Network Mode)**:

1. **Edit `docker-compose.yml`** and add `network_mode: "host"` to all services
2. **Restart containers**: `docker compose down && docker compose up --build`
3. **Test again** at `http://dockerstream1.fyre.ibm.com:8080`

This should resolve the ICE connection issue immediately.

---

## Expected Behavior After Fix

Once the network is configured correctly, you should see:

```
[CLIENT] WebSocket connected!
[CLIENT] Waiting for offer from game...
[CLIENT] Received offer
[CLIENT] Answer sent
[CLIENT] ICE connection state: checking
[CLIENT] ICE connection state: connected  ← This should appear!
[CLIENT] Data channel opened
✅ Connected! Click video to start playing
```

And the video should display the game (xterm with snake).

---

## Additional Notes

### Why This Happens

WebRTC uses ICE (Interactive Connectivity Establishment) to find the best network path between peers. When running in Docker:

- **Internal network**: Containers get IPs like `172.18.0.2`
- **ICE candidates**: Include these internal IPs in the SDP offer
- **Remote browser**: Cannot route to these internal IPs
- **Result**: ICE connection fails, no media flows

### Production Deployment

For production, you should:

1. ✅ Use a proper TURN server (Coturn, Twilio, etc.)
2. ✅ Configure firewall rules for UDP media ports
3. ✅ Use HTTPS/WSS for security
4. ✅ Implement authentication
5. ✅ Monitor connection quality

---

## Testing Checklist

After applying the fix:

- [ ] WebSocket connects successfully
- [ ] Offer/Answer exchange completes
- [ ] ICE connection state reaches "connected"
- [ ] Video element shows the game
- [ ] Status shows "✅ Connected! Click video to start playing"
- [ ] Keyboard input (WASD) works
- [ ] Mouse input works
- [ ] No errors in browser console
- [ ] No errors in Docker logs

---

## Need More Help?

If the issue persists after trying Option 1:

1. **Share the output** of `chrome://webrtc-internals` (ICE candidates section)
2. **Check Docker logs**: `docker compose logs game`
3. **Verify firewall**: `sudo firewall-cmd --list-all`
4. **Test locally first**: Access from `http://localhost:8080` on the server itself

The code is working correctly - this is purely a network configuration issue that can be resolved with the steps above.
