# RTP Quick Start Guide

This guide shows you how to quickly get Windows Graphics Capture streaming to your existing infrastructure using RTP.

## Architecture

```
Windows PC                    Docker Container              Browser
┌─────────────┐              ┌──────────────┐            ┌─────────┐
│ WGC Capture │              │  GStreamer   │            │ WebRTC  │
│     ↓       │   RTP/UDP    │  (modified)  │  WebRTC    │ Client  │
│   NVENC     │─────────────→│  webrtcbin   │───────────→│         │
│  H.264      │  Port 5000   │              │            │         │
└─────────────┘              └──────────────┘            └─────────┘
```

## What Changed

✅ **Updated `game/webrtc_streamer.py`** to support RTP input mode
✅ **Created `docker-compose.rtp.yml`** for RTP-enabled deployment
✅ **Maintains backward compatibility** with X11 capture mode

## Quick Setup

### 1. Start Infrastructure (Linux/Docker)

```bash
cd /root/webrtc-game-stream

# Start with RTP mode enabled
docker compose -f docker-compose.rtp.yml up

# Or use environment variables with regular docker-compose
RTP_MODE=true RTP_PORT=5000 RTP_CODEC=H264 docker compose up
```

This starts:
- Signaling server on port 9000
- Game container listening for RTP on port 5000/udp
- Web client on port 8080

### 2. Configure Windows Streamer

Edit `windows_graphics_capture/config/config.json`:

```json
{
  "capture": {
    "mode": "primary",
    "width": 1920,
    "height": 1080,
    "fps": 60
  },
  "encoder": {
    "codec": "h264",
    "preset": "low_latency",
    "bitrate": 5000000
  },
  "streaming": {
    "mode": "rtp",
    "target_host": "192.168.1.100",
    "target_port": 5000
  }
}
```

Replace `192.168.1.100` with your Docker host IP.

### 3. Build Windows Streamer (if not done)

```cmd
cd windows_graphics_capture
mkdir build
cd build
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release
```

### 4. Run Windows Streamer

```cmd
cd build\bin\Release
WindowsGraphicsCapture.exe
```

### 5. Open Browser

```
http://192.168.1.100:8080
```

You should see the Windows screen streaming!

## Environment Variables

The game container now supports these environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `RTP_MODE` | `false` | Enable RTP input mode |
| `RTP_PORT` | `5000` | UDP port to receive RTP |
| `RTP_CODEC` | `H264` | Codec: `H264` or `VP8` |

## Pipeline Modes

### X11 Capture Mode (Default)
```python
RTP_MODE=false  # or not set
```
Captures from X11 display (original behavior)

### RTP Input Mode (Windows)
```python
RTP_MODE=true
RTP_PORT=5000
RTP_CODEC=H264
```
Receives RTP stream from Windows Graphics Capture

## Testing

### Test RTP Reception

```bash
# Check if container is listening
docker exec -it webrtc-game-stream-game-rtp-1 netstat -uln | grep 5000

# Send test RTP stream (from another machine)
gst-launch-1.0 videotestsrc ! \
  x264enc ! rtph264pay ! \
  udpsink host=192.168.1.100 port=5000
```

### Monitor Logs

```bash
# Watch game container logs
docker compose -f docker-compose.rtp.yml logs -f game-rtp

# Should see:
# [STREAMER] RTP MODE: Receiving H.264 stream from Windows on port 5000
```

## Troubleshooting

### No video in browser

1. **Check Windows streamer is sending:**
   ```cmd
   # In Windows, check if RTP packets are being sent
   netstat -an | findstr 5000
   ```

2. **Check Docker container is receiving:**
   ```bash
   docker compose logs game-rtp | grep RTP
   ```

3. **Verify network connectivity:**
   ```bash
   # From Windows, ping Docker host
   ping 192.168.1.100
   
   # Test UDP connectivity
   # On Linux: nc -ul 5000
   # On Windows: Test-NetConnection -ComputerName 192.168.1.100 -Port 5000
   ```

### Firewall Issues

**On Linux (Docker host):**
```bash
sudo ufw allow 5000/udp
sudo ufw allow 9000/tcp
sudo ufw allow 8080/tcp
```

**On Windows:**
```cmd
# Allow outbound UDP on port 5000
netsh advfirewall firewall add rule name="WGC RTP Out" dir=out action=allow protocol=UDP localport=5000
```

### High Latency

1. **Reduce encoding latency on Windows:**
   ```json
   {
     "encoder": {
       "preset": "ultra_low_latency",
       "gopSize": 30
     }
   }
   ```

2. **Check network latency:**
   ```bash
   ping 192.168.1.100
   ```

3. **Use wired connection** instead of WiFi

## Performance Tips

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

## Network Configuration

### Local Network (LAN)
- Use private IP (e.g., 192.168.1.100)
- No special configuration needed
- Lowest latency

### Different Subnets
- Ensure routing is configured
- May need to adjust firewall rules
- Test with ping first

### Internet (Not Recommended for RTP)
- RTP over internet is unreliable
- Use full WebRTC with TURN server instead
- Or use VPN for secure tunnel

## Switching Between Modes

### Use X11 Capture (Linux games)
```bash
docker compose up
```

### Use RTP Input (Windows games)
```bash
docker compose -f docker-compose.rtp.yml up
```

### Both at the same time
Run two separate containers with different ports:
```yaml
# docker-compose.hybrid.yml
services:
  game-x11:
    # ... X11 capture on room1
  game-rtp:
    # ... RTP input on room2
```

## Next Steps

1. ✅ Test with simple game
2. ✅ Optimize encoder settings
3. ✅ Monitor performance metrics
4. 🔄 Consider migrating to full WebRTC for production

## Support

If you encounter issues:
1. Check logs: `docker compose logs -f game-rtp`
2. Verify network connectivity
3. Test with videotestsrc first
4. Check firewall rules
5. Monitor Windows streamer logs

---

**Note:** This RTP mode is designed for quick testing and local network use. For production internet streaming, consider implementing full WebRTC in the Windows streamer.
