# WebRTC Game Streaming - Implementation Plan

## Executive Summary

This document provides a comprehensive plan to enable users to play the Snake game running in a Docker container via WebRTC streaming. The system consists of three Docker containers that work together to stream video and handle input over WebRTC.

## Current Status

**🔴 CRITICAL ISSUE IDENTIFIED**: The Docker build currently fails due to a Python package dependency issue.

### Build Error
```
ERROR: Could not find a version that satisfies the requirement evdev==1.7.1
WARNING: Generating metadata for package evdev produced metadata for project name unknown
```

**Root Cause**: The `evdev==1.7.1` package has a metadata bug where it reports its name as "unknown" instead of "evdev", causing pip to reject it.

**Solution**: Update `game/requirements.txt` to use a working version of evdev.

---

## System Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────────┐
│                         User's Browser                       │
│  ┌────────────────────────────────────────────────────┐    │
│  │  Web Client (http://localhost:8080)                │    │
│  │  - Displays video stream                           │    │
│  │  - Captures keyboard/mouse input                   │    │
│  │  - Sends input via WebRTC DataChannel              │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                            ↕ WebRTC (video + data)
┌─────────────────────────────────────────────────────────────┐
│                    Signaling Server Container                │
│  - WebSocket server on port 9000                            │
│  - Relays SDP offers/answers and ICE candidates            │
│  - Room-based message routing                               │
└─────────────────────────────────────────────────────────────┘
                            ↕ WebSocket
┌─────────────────────────────────────────────────────────────┐
│                      Game Container                          │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Xvfb (Virtual Display :99)                          │  │
│  │    └─> xterm running snake.sh                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                            ↕                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  GStreamer Pipeline                                   │  │
│  │  - Captures screen from Xvfb                         │  │
│  │  - Encodes with x264 (H.264)                         │  │
│  │  - Streams via WebRTC                                │  │
│  └──────────────────────────────────────────────────────┘  │
│                            ↕                                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │  Python WebRTC Streamer (webrtc_streamer.py)        │  │
│  │  - Receives input from DataChannel                   │  │
│  │  - Injects input via uinput kernel module            │  │
│  └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Data Flow

**Video Stream (Game → Browser)**:
1. Snake game runs in xterm on Xvfb virtual display
2. GStreamer captures the display using `ximagesrc`
3. Video is encoded with x264enc (H.264)
4. Encoded stream sent via WebRTC to browser
5. Browser displays video in `<video>` element

**Input Control (Browser → Game)**:
1. User presses keys or moves mouse in browser
2. JavaScript captures events and sends JSON via WebRTC DataChannel
3. Python script receives input events
4. Events injected into Linux kernel via uinput
5. Game receives input as if from real keyboard/mouse

---

## Implementation Steps

### Step 1: Fix Build Error ✅ REQUIRED

**File**: `game/requirements.txt`

**Current Content**:
```
websockets==12.0
evdev==1.7.1
```

**Action**: Replace with a working evdev version:

**Option A** (Recommended - Latest stable):
```
websockets==12.0
evdev==1.9.3
```

**Option B** (Conservative - Known working):
```
websockets==12.0
evdev==1.6.1
```

**Option C** (Alternative):
```
websockets==12.0
evdev>=1.6.0,!=1.7.1
```

### Step 2: Build Docker Containers

```bash
cd /root/webrtc-game-stream
docker compose build
```

**Expected Output**: All three containers (signaling, game, web) should build successfully without errors.

**Verification**: Check that the build completes with:
```
=> [game] exporting to image
=> => naming to docker.io/library/webrtc-game-stream-game:latest
```

### Step 3: Start the System

```bash
docker compose up
```

**What Happens**:
- Signaling server starts on port 9000
- Game container starts Xvfb, launches snake game, starts WebRTC streamer
- Web server starts on port 8080

**Expected Logs**:
```
signaling-1  | Signaling server running on ws://0.0.0.0:9000
game-1       | Starting Xvfb on display :99
game-1       | Starting game: xterm -maximized -e /app/snake.sh
web-1        | /docker-entrypoint.sh: Configuration complete; ready for start up
```

### Step 4: Access the Game

1. **Open Browser**: Navigate to `http://localhost:8080`
2. **Wait for Connection**: Status should show "✅ Receiving video stream"
3. **Click Video**: Click on the video area to focus it
4. **Play**: Use arrow keys or WASD to control the snake

---

## Game Controls

### Snake Game Controls

| Input | Action |
|-------|--------|
| ↑ / W | Move Up |
| ↓ / S | Move Down |
| ← / A | Move Left |
| → / D | Move Right |
| P | Pause/Unpause |
| Q | Quit Game |

### Browser Controls

- **Click video area**: Focus for input capture
- **Check status bar**: Shows connection state and score

---

## Troubleshooting Guide

### Issue: Build Fails with "evdev" Error

**Symptom**:
```
ERROR: No matching distribution found for evdev==1.7.1
```

**Solution**: Follow Step 1 to update requirements.txt

### Issue: No Video Stream

**Symptoms**:
- Black screen in browser
- Status shows "Connection: connecting" or "Connection: failed"

**Checks**:
1. Verify all containers are running:
   ```bash
   docker compose ps
   ```
   All should show "Up" status

2. Check signaling server logs:
   ```bash
   docker compose logs signaling
   ```
   Should show "Signaling server running"

3. Check game container logs:
   ```bash
   docker compose logs game
   ```
   Look for errors in GStreamer pipeline

4. Check browser console (F12):
   - Look for WebRTC connection errors
   - Verify WebSocket connection to port 9000

**Common Fixes**:
- Restart containers: `docker compose restart`
- Rebuild: `docker compose down && docker compose up --build`

### Issue: Input Not Working

**Symptoms**:
- Video displays but keys don't control snake
- Status shows "⚠️ Data channel closed"

**Checks**:
1. Verify data channel is open:
   - Browser console should show "✅ Data channel open - inputs enabled"

2. Check game container has uinput access:
   ```bash
   docker compose exec game ls -l /dev/uinput
   ```
   Should show the device exists

3. Verify privileged mode in docker-compose.yml:
   ```yaml
   game:
     privileged: true
     devices:
       - /dev/uinput:/dev/uinput
   ```

**Common Fixes**:
- Click on video area to focus it
- Reload browser page
- Restart game container: `docker compose restart game`

### Issue: High Latency / Lag

**Symptoms**:
- Noticeable delay between input and video response
- Choppy video playback

**Solutions**:
1. Check network conditions
2. Reduce video bitrate in `game/webrtc_streamer.py`:
   ```python
   x264enc tune=zerolatency speed-preset=ultrafast bitrate=3000
   ```
   (Change from 6000 to 3000)

3. Check CPU usage:
   ```bash
   docker stats
   ```

### Issue: Container Exits Immediately

**Check logs**:
```bash
docker compose logs game
```

**Common Causes**:
- Missing dependencies (should be fixed after Step 1)
- Port conflicts (9000 or 8080 already in use)
- Permission issues with /dev/uinput

---

## Testing Checklist

### Pre-Launch Verification

- [ ] requirements.txt updated with working evdev version
- [ ] Docker containers build without errors
- [ ] All three containers start successfully
- [ ] Signaling server accessible on port 9000
- [ ] Web interface accessible on port 8080

### Functional Testing

- [ ] Browser displays video stream from game
- [ ] Status shows "✅ Receiving video stream"
- [ ] Status shows "✅ Data channel open - inputs enabled"
- [ ] Arrow keys control snake movement
- [ ] WASD keys control snake movement
- [ ] Snake grows when eating food (@)
- [ ] Score increases when eating food
- [ ] Game over on collision with wall or self
- [ ] Pause (P) and Quit (Q) work correctly

### Performance Testing

- [ ] Video latency < 500ms
- [ ] Input response feels immediate
- [ ] No frame drops during gameplay
- [ ] CPU usage reasonable (< 80% per container)

---

## Advanced Configuration

### Changing the Game

To run a different game instead of snake:

1. Edit `docker-compose.yml`:
   ```yaml
   environment:
     - GAME_CMD=your-game-command-here
   ```

2. Ensure game accepts input from X11
3. Rebuild and restart

### Network Configuration

**For LAN Access**:
- Change `ws://signaling:9000` to `ws://<host-ip>:9000` in client.js
- Update CORS settings if needed

**For Internet Access**:
- Set up TURN server for NAT traversal
- Use WSS (secure WebSocket) for signaling
- Configure firewall rules

### Performance Tuning

**Lower Latency**:
```python
# In webrtc_streamer.py, reduce bitrate and keyframe interval
x264enc tune=zerolatency speed-preset=ultrafast bitrate=3000 key-int-max=30
```

**Higher Quality**:
```python
# Increase bitrate and use better preset
x264enc tune=zerolatency speed-preset=fast bitrate=10000 key-int-max=60
```

---

## Security Considerations

### Current Setup (Development Only)

⚠️ **WARNING**: This configuration is for local development only!

**Security Issues**:
- No authentication on signaling server
- No encryption (HTTP/WS instead of HTTPS/WSS)
- Privileged container mode
- No input validation

### Production Recommendations

1. **Add Authentication**:
   - Implement JWT tokens for room access
   - Validate tokens in signaling server

2. **Use TLS/SSL**:
   - Configure HTTPS for web server
   - Use WSS for signaling
   - Add SSL certificates

3. **Restrict Permissions**:
   - Remove privileged mode
   - Use specific capabilities: `cap_add: [SYS_ADMIN]`
   - Limit device access

4. **Input Validation**:
   - Sanitize all input events
   - Rate limit input commands
   - Validate event types

---

## Next Steps

### Immediate Actions (Required)

1. **Fix Build Error**: Update `game/requirements.txt` as described in Step 1
2. **Build Containers**: Run `docker compose build`
3. **Start System**: Run `docker compose up`
4. **Test Gameplay**: Access http://localhost:8080 and play

### Recommended Improvements

1. **Add Health Checks**: Monitor container health
2. **Implement Logging**: Structured logging for debugging
3. **Add Metrics**: Track latency, frame rate, connection quality
4. **Create Tests**: Automated testing for WebRTC connection
5. **Documentation**: User guide for different games

### Future Enhancements

1. **Audio Support**: Add game audio streaming
2. **Multiple Players**: Support concurrent game sessions
3. **Game Selection**: UI to choose different games
4. **Recording**: Save gameplay sessions
5. **Mobile Support**: Touch controls for mobile devices

---

## Success Criteria

The implementation is successful when:

✅ Docker containers build without errors  
✅ All three containers start and run stably  
✅ Browser displays live video stream from game  
✅ User can control snake with keyboard  
✅ Game responds to input with < 500ms latency  
✅ System runs for extended periods without crashes  

---

## Support Resources

### Documentation
- **GStreamer WebRTC**: https://gstreamer.freedesktop.org/documentation/webrtc/
- **WebRTC API**: https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API
- **Linux uinput**: https://www.kernel.org/doc/html/latest/input/uinput.html

### Debugging Commands

```bash
# View all container logs
docker compose logs -f

# View specific container logs
docker compose logs -f game

# Check container status
docker compose ps

# Restart specific container
docker compose restart game

# Rebuild and restart
docker compose down && docker compose up --build

# Execute command in running container
docker compose exec game bash

# Check GStreamer pipeline
docker compose exec game gst-inspect-1.0 webrtcbin
```

---

## Conclusion

This system provides a complete WebRTC-based game streaming solution. The critical first step is fixing the evdev package version issue, after which the system should build and run successfully. Follow the implementation steps in order, use the troubleshooting guide for any issues, and refer to the testing checklist to verify functionality.

**Estimated Time to Working System**: 15-30 minutes (after fixing requirements.txt)

**Key Success Factor**: Ensuring the evdev package installs correctly is essential for input handling to work.
