# Windows Graphics Capture Setup for dockerstream1.fyre.ibm.com

This guide shows how to stream from a Windows machine to your Docker container running on `dockerstream1.fyre.ibm.com`.

## Quick Setup

### 1. Start Docker Container (on dockerstream1.fyre.ibm.com)

```bash
cd /root/webrtc-game-stream
docker compose -f docker-compose.rtp.yml up -d
```

This starts:
- Signaling server on port 9000
- Game container listening for RTP on port 5000/udp
- Web interface on port 8080

### 2. Verify Firewall (on dockerstream1.fyre.ibm.com)

```bash
# Allow RTP input
sudo firewall-cmd --add-port=5000/udp --permanent

# Allow signaling server (if needed)
sudo firewall-cmd --add-port=9000/tcp --permanent

# Allow web interface (if needed)
sudo firewall-cmd --add-port=8080/tcp --permanent

# Reload firewall
sudo firewall-cmd --reload

# Verify
sudo firewall-cmd --list-ports
```

### 3. Setup Windows Machine

#### Download the Project
Copy the `windows_graphics_capture` folder to your Windows machine.

#### Install Prerequisites
- Visual Studio 2019 or later (with C++ and Windows 10 SDK)
- CUDA Toolkit (for NVENC)
- NVIDIA Video Codec SDK
- CMake 3.20+

#### Build the Streamer
```cmd
cd windows_graphics_capture
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### 4. Configure Windows Streamer

The configuration is already set for your environment in `config/config_rtp.json`:

```json
{
  "streaming": {
    "mode": "rtp",
    "rtpTargetHost": "dockerstream1.fyre.ibm.com",
    "rtpTargetPort": 5000
  }
}
```

**No changes needed!** Just copy this file to your build directory.

### 5. Run Windows Streamer

```cmd
cd build\bin\Release
copy ..\..\..\config\config_rtp.json .
WindowsGraphicsCapture.exe --config config_rtp.json
```

You should see:
```
[INFO] Windows Graphics Capture Streamer starting...
[INFO] RTP sender initialized
[INFO] Streaming to: dockerstream1.fyre.ibm.com:5000
[INFO] Capture started successfully!
```

### 6. Access from Browser

Open any browser and navigate to:
```
http://dockerstream1.fyre.ibm.com:8080
```

You should see your Windows screen streaming!

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Your Network                              │
│                                                                  │
│  ┌──────────────────┐                ┌─────────────────────┐   │
│  │  Windows PC      │   RTP/UDP      │  dockerstream1      │   │
│  │                  │   Port 5000    │  .fyre.ibm.com      │   │
│  │  WGC + NVENC     │───────────────→│                     │   │
│  │  (Capture &      │                │  Docker Container   │   │
│  │   Encode)        │                │  - Signaling :9000  │   │
│  └──────────────────┘                │  - RTP Input :5000  │   │
│                                       │  - Web UI :8080     │   │
│                                       └─────────────────────┘   │
│                                                 │                │
│                                                 │ WebRTC         │
│                                                 ▼                │
│                                       ┌─────────────────────┐   │
│                                       │   Browser           │   │
│                                       │   (Any Device)      │   │
│                                       └─────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

## Troubleshooting

### Can't connect from Windows to Docker VM

**Test connectivity:**
```cmd
# From Windows, ping the server
ping dockerstream1.fyre.ibm.com

# Test if port 5000 is reachable (requires telnet)
Test-NetConnection -ComputerName dockerstream1.fyre.ibm.com -Port 5000
```

**Check Docker container:**
```bash
# On dockerstream1.fyre.ibm.com
docker compose -f docker-compose.rtp.yml ps
docker compose -f docker-compose.rtp.yml logs game-rtp
```

### No video in browser

**Check Windows streamer logs:**
Look for errors in the console output or `wgc_capture.log`

**Check container is receiving RTP:**
```bash
# On dockerstream1.fyre.ibm.com
docker compose -f docker-compose.rtp.yml logs -f game-rtp

# Should see:
# [STREAMER] RTP MODE: Receiving H.264 stream from Windows on port 5000
```

**Monitor network traffic:**
```bash
# On dockerstream1.fyre.ibm.com
sudo tcpdump -i any udp port 5000
```

### High latency

**On Windows, use ultra-low latency preset:**
```json
{
  "encoder": {
    "preset": "ultra_low_latency",
    "gopSize": 30
  }
}
```

**Check network latency:**
```cmd
ping dockerstream1.fyre.ibm.com
```

### Firewall blocking

**On dockerstream1.fyre.ibm.com:**
```bash
# Check if firewall is active
sudo firewall-cmd --state

# List current rules
sudo firewall-cmd --list-all

# Add rules if needed (see step 2)
```

**On Windows:**
```cmd
# Check Windows Firewall
Get-NetFirewallRule | Where-Object {$_.DisplayName -like "*WGC*"}

# Add rule if needed
New-NetFirewallRule -DisplayName "WGC RTP Out" -Direction Outbound -Protocol UDP -LocalPort 5000 -Action Allow
```

## Performance Optimization

### For Best Quality
```json
{
  "encoder": {
    "codec": "hevc",
    "bitrate": 10000000,
    "preset": "quality"
  }
}
```

### For Lowest Latency
```json
{
  "encoder": {
    "codec": "h264",
    "bitrate": 5000000,
    "preset": "ultra_low_latency",
    "gopSize": 30
  }
}
```

### For Low Bandwidth
```json
{
  "encoder": {
    "codec": "hevc",
    "bitrate": 2000000,
    "preset": "low_latency"
  }
}
```

## Monitoring

### Check Statistics

**Windows streamer** prints statistics every 5 seconds:
```
=== Statistics ===
Capture:
  Frames captured: 3000
  Current FPS: 60.0
  Avg capture latency: 2.1 ms

Encoder:
  Frames encoded: 2995
  Avg encode latency: 5.3 ms

RTP:
  Frames sent: 2990
  Total bytes: 15728640
==================
```

**Docker container logs:**
```bash
docker compose -f docker-compose.rtp.yml logs -f game-rtp
```

## Multiple Windows Machines

To stream from multiple Windows machines:

1. **Use different rooms:**
   ```json
   // Windows PC 1
   {"streaming": {"rtpTargetPort": 5000}}
   
   // Windows PC 2
   {"streaming": {"rtpTargetPort": 5001}}
   ```

2. **Update docker-compose.rtp.yml:**
   ```yaml
   services:
     game-rtp-1:
       ports:
         - "5000:5000/udp"
       environment:
         - ROOM=room1
     
     game-rtp-2:
       ports:
         - "5001:5001/udp"
       environment:
         - ROOM=room2
         - RTP_PORT=5001
   ```

## Support

For issues:
1. Check Windows streamer logs: `wgc_capture.log`
2. Check Docker logs: `docker compose logs game-rtp`
3. Verify network connectivity: `ping dockerstream1.fyre.ibm.com`
4. Test firewall: `sudo firewall-cmd --list-ports`
5. Monitor traffic: `sudo tcpdump -i any udp port 5000`

## Next Steps

- ✅ Test with a simple game
- ✅ Optimize encoder settings for your network
- ✅ Monitor performance metrics
- 🔄 Consider adding authentication for production use
