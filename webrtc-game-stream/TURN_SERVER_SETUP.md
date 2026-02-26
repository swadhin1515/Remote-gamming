# TURN Server Setup Guide - Fix ICE Connection Failed

## Problem

WebRTC ICE connection fails with error:
```
WebRTC: ICE failed, add a TURN server
[CLIENT] ICE connection state: failed
[CLIENT] ❌ Connection failed - check network/firewall
```

## Root Cause

Even with host network mode, the browser cannot establish a direct connection to the server because:
1. **Firewall blocking UDP ports** for WebRTC media
2. **NAT traversal issues** - browser and server are on different networks
3. **No TURN server** to relay traffic when direct connection fails

## Solution: Install and Configure Coturn TURN Server

### Step 1: Install Coturn on Your Server

```bash
# Install coturn
sudo yum install -y coturn

# Or on Ubuntu/Debian:
# sudo apt-get install -y coturn
```

### Step 2: Configure Coturn

Create/edit `/etc/turnserver.conf`:

```bash
sudo tee /etc/turnserver.conf > /dev/null <<'EOF'
# Listening port for TURN server
listening-port=3478

# Enable fingerprint
fingerprint

# Use long-term credentials
lt-cred-mech

# Create a user (username:password)
user=webrtc:SecurePassword123

# Realm (your server domain or IP)
realm=dockerstream1.fyre.ibm.com

# External IP (your server's public IP)
external-ip=9.30.249.236

# Verbose logging (for debugging)
verbose

# Log file
log-file=/var/log/turnserver.log

# Allow connections from any origin
no-cli

# Relay IP range (use your server's IP)
relay-ip=9.30.249.236

# Disable TLS (use plain TURN for testing)
no-tls
no-dtls
EOF
```

### Step 3: Open Firewall Ports

```bash
# Open TURN server port (TCP and UDP)
sudo firewall-cmd --permanent --add-port=3478/tcp
sudo firewall-cmd --permanent --add-port=3478/udp

# Open UDP port range for media relay
sudo firewall-cmd --permanent --add-port=49152-65535/udp

# Reload firewall
sudo firewall-cmd --reload

# Verify ports are open
sudo firewall-cmd --list-ports
```

### Step 4: Start Coturn Service

```bash
# Enable and start coturn
sudo systemctl enable coturn
sudo systemctl start coturn

# Check status
sudo systemctl status coturn

# View logs
sudo tail -f /var/log/turnserver.log
```

### Step 5: Update WebRTC Configuration

#### Update `web/client.js`:

```javascript
pc = new RTCPeerConnection({
  iceServers: [
    { urls: "stun:stun.l.google.com:19302" },
    { 
      urls: "turn:dockerstream1.fyre.ibm.com:3478",
      username: "webrtc",
      credential: "SecurePassword123"
    }
  ]
});
```

#### Update `game/webrtc_streamer.py`:

Add after creating webrtcbin:

```python
# Set STUN server
self.webrtc.set_property("stun-server", "stun://stun.l.google.com:19302")

# Set TURN server
self.webrtc.set_property("turn-server", "turn://webrtc:SecurePassword123@dockerstream1.fyre.ibm.com:3478")
```

### Step 6: Rebuild and Test

```bash
cd /root/webrtc-game-stream
docker compose down
docker compose up --build -d

# Watch logs
docker compose logs -f game
```

### Step 7: Test TURN Server

```bash
# Test if TURN server is accessible
nc -zv dockerstream1.fyre.ibm.com 3478

# Or use turnutils-uclient (if available)
turnutils_uclient -v -u webrtc -w SecurePassword123 dockerstream1.fyre.ibm.com
```

## Quick Fix (Alternative): Use Public TURN Server

If you don't want to set up your own TURN server, use a public one temporarily:

### Update `web/client.js`:

```javascript
pc = new RTCPeerConnection({
  iceServers: [
    { urls: "stun:stun.l.google.com:19302" },
    { 
      urls: "turn:openrelay.metered.ca:80",
      username: "openrelayproject",
      credential: "openrelayproject"
    },
    {
      urls: "turn:openrelay.metered.ca:443",
      username: "openrelayproject",
      credential: "openrelayproject"
    }
  ]
});
```

**Note**: Public TURN servers are for testing only. Use your own for production.

## Verification

After setup, check browser console:

```
[CLIENT] ICE connection state: checking
[CLIENT] ICE connection state: connected  ← Should succeed now!
✅ Connected! Click video to start playing
```

Check `chrome://webrtc-internals` to see:
- ICE candidates include relay candidates
- Connection type shows "relay" if using TURN

## Troubleshooting

### TURN Server Not Working

1. **Check if coturn is running**:
   ```bash
   sudo systemctl status coturn
   sudo netstat -tulpn | grep 3478
   ```

2. **Check firewall**:
   ```bash
   sudo firewall-cmd --list-all
   ```

3. **Check logs**:
   ```bash
   sudo tail -f /var/log/turnserver.log
   ```

4. **Test from browser machine**:
   ```bash
   telnet dockerstream1.fyre.ibm.com 3478
   ```

### Still Failing

If ICE still fails after TURN setup:

1. **Check SELinux** (if enabled):
   ```bash
   sudo setenforce 0  # Temporarily disable for testing
   ```

2. **Verify external IP** in turnserver.conf matches your public IP

3. **Try different TURN server** (use public one for testing)

4. **Check browser console** for specific ICE errors

## Production Recommendations

1. ✅ Use your own TURN server (don't rely on public ones)
2. ✅ Enable TLS for TURN (turns://)
3. ✅ Use strong passwords
4. ✅ Limit TURN server access by IP if possible
5. ✅ Monitor TURN server bandwidth usage
6. ✅ Set up log rotation for turnserver.log

## Next Steps

1. Install and configure coturn as shown above
2. Update client.js and webrtc_streamer.py with TURN configuration
3. Rebuild containers
4. Test connection - should now succeed!
