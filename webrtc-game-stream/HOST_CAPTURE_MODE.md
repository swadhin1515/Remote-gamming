# Host Capture Mode

Stream games and applications running on your **host machine** instead of inside the Docker container.

## Overview

The WebRTC Game Streaming System now supports two capture modes:

1. **Container Mode (Default)**: Games run inside the Docker container on a virtual display
2. **Host Mode**: Capture and stream games already running on your host machine

## Quick Start - Host Capture Mode

### Step 1: Set Environment Variables

Edit `docker-compose.yml` or create a `.env` file:

```yaml
game:
  environment:
    - CAPTURE_MODE=host
    - DISPLAY=${DISPLAY}  # Use host display
```

Or use environment variables directly:

```bash
CAPTURE_MODE=host docker-compose up
```

### Step 2: Allow X11 Access (Linux)

Allow Docker to access your X11 display:

```bash
# Allow local connections (simple but less secure)
xhost +local:docker

# Or allow specific user (more secure)
xhost +SI:localuser:$(whoami)
```

### Step 3: Start the System

```bash
# Start with host capture mode
CAPTURE_MODE=host docker-compose up

# Or if you edited docker-compose.yml
docker-compose up
```

### Step 4: Run Your Game

Run your game on the host machine as normal:

```bash
# Example: Run Minecraft
java -jar minecraft.jar

# Example: Run a Python game
python3 my_game.py

# Example: Run any application
./my-application
```

### Step 5: Open Browser

Open `http://localhost:8080` in your browser to see the stream.

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CAPTURE_MODE` | `container` | Set to `host` to capture from host display |
| `DISPLAY` | `:0` | X11 display to capture (usually `:0` for host) |
| `SIGNALING_URL` | `ws://localhost:9000` | WebSocket signaling server URL |
| `ROOM` | `room1` | WebRTC room name |

### Docker Compose Configuration

```yaml
services:
  game:
    environment:
      - CAPTURE_MODE=host           # Enable host capture
      - DISPLAY=${DISPLAY:-:0}      # Use host display
    volumes:
      - /tmp/.X11-unix:/tmp/.X11-unix:rw  # Mount X11 socket
      - ${XAUTHORITY:-$HOME/.Xauthority}:/root/.Xauthority:ro  # X11 auth
```

## Comparison: Container vs Host Mode

| Feature | Container Mode | Host Mode |
|---------|---------------|-----------|
| **Game Location** | Inside Docker | On host machine |
| **Display** | Virtual (Xvfb) | Real X11 display |
| **Setup** | Upload to `games/` folder | Run game normally on host |
| **Dependencies** | Must be in container | Already on host |
| **Isolation** | Fully isolated | Shares host resources |
| **Performance** | Good | Better (no virtualization) |
| **Use Case** | Portable, reproducible | Quick testing, existing games |

## Use Cases

### Host Mode is Best For:

✅ **Testing existing games** - Stream games already installed on your machine  
✅ **Development** - Test your game while developing  
✅ **Heavy games** - Games that need full GPU access  
✅ **Quick demos** - No need to containerize the game  
✅ **Python/Java games** - Games with complex dependencies already on host  

### Container Mode is Best For:

✅ **Production deployment** - Reproducible, isolated environment  
✅ **Multiple games** - Easy game switching via `GAME_NAME`  
✅ **Portability** - Works on any machine with Docker  
✅ **Security** - Isolated from host system  
✅ **Headless servers** - No X11 display required  

## Examples

### Example 1: Stream Minecraft (Host Mode)

```bash
# 1. Allow X11 access
xhost +local:docker

# 2. Start streaming system in host mode
CAPTURE_MODE=host docker-compose up -d

# 3. Launch Minecraft on your host
java -jar minecraft.jar

# 4. Open browser to http://localhost:8080
```

### Example 2: Stream Python/Pygame Game (Host Mode)

```bash
# 1. Allow X11 access
xhost +local:docker

# 2. Start streaming system
CAPTURE_MODE=host docker-compose up -d

# 3. Run your Python game
python3 my_pygame_game.py

# 4. View stream in browser
```

### Example 3: Stream Any Application

```bash
# Stream your entire desktop or any window
CAPTURE_MODE=host docker-compose up -d

# The stream will capture whatever is on your display
```

## Input Control - Remote Browser to Host Game

**How Remote Control Works:**

```
┌─────────────────┐
│  Remote Browser │  (Anywhere on internet)
│  (Chrome/Edge)  │
└────────┬────────┘
         │ Keyboard/Mouse Events
         │ (via WebRTC DataChannel)
         ▼
┌─────────────────┐
│ Docker Container│
│ webrtc_streamer │
└────────┬────────┘
         │ X11 XTEST Extension
         │ inject() function
         ▼
┌─────────────────┐
│  Host Display   │  (Your Linux machine)
│   DISPLAY=:0    │
└────────┬────────┘
         │ Real Input Events
         ▼
┌─────────────────┐
│   Your Game     │  (Minecraft, Python game, etc.)
│  Running on Host│
└─────────────────┘
```

**Simple Flow:**
```
Remote Browser → WebRTC DataChannel → Docker Container → X11 XTEST → Host Display (:0) → Your Game
```

In host mode, keyboard and mouse input from **any remote browser** is injected into your host's X11 display, allowing you to control games running on your host machine remotely.

**Input Flow:**
1. **User interacts** with the video stream in their browser (anywhere on the internet)
2. **Browser captures** keyboard/mouse events
3. **Events sent** via WebRTC DataChannel to the container
4. **Container receives** events in `webrtc_streamer.py`
5. **X11 XTEST injects** input into host display (`DISPLAY=:0`)
6. **Your game receives** the input as if you were using your local keyboard/mouse

**Supported Input:**
- ✅ Keyboard (all keys)
- ✅ Mouse movement
- ✅ Mouse clicks (left, right, middle)
- ✅ Mouse wheel

**Example Scenario:**
- Your game (Minecraft, Python game, etc.) runs on your Linux machine
- A friend opens `http://your-ip:8080` in their browser
- They see your game streaming in real-time
- They click and type in the browser
- Your game responds to their input instantly
- They are controlling your game remotely!

**This works because:**
- The container has access to your host's X11 display via `/tmp/.X11-unix`
- The `inject()` function uses X11 XTEST extension to simulate keyboard/mouse events
- These events are injected into your host display where your game is running
- Your game receives them as real input events

## Troubleshooting

### Issue: "Cannot open display"

**Solution:** Allow X11 access:
```bash
xhost +local:docker
```

### Issue: Black screen in browser

**Possible causes:**
1. No application running on host display
2. X11 permissions not granted
3. Wrong DISPLAY variable

**Solutions:**
```bash
# Check your display
echo $DISPLAY

# Verify X11 is accessible
xdpyinfo

# Grant X11 access
xhost +local:docker

# Check container logs
docker-compose logs game
```

### Issue: Input not working

**Solution:** Ensure container has proper permissions:
```yaml
game:
  privileged: true
  devices:
    - /dev/uinput:/dev/uinput
```

### Issue: Permission denied on .Xauthority

**Solution:** Make sure the file is readable:
```bash
chmod 644 ~/.Xauthority
```

Or mount it with proper permissions in docker-compose.yml.

## Security Considerations

### Host Mode Security

⚠️ **Warning:** Host mode gives the container access to your X11 display, which can be a security risk.

**Recommendations:**

1. **Use `xhost` carefully:**
   ```bash
   # Safer: Allow specific user
   xhost +SI:localuser:$(whoami)
   
   # Less safe: Allow all local connections
   xhost +local:docker
   
   # Remove access when done
   xhost -local:docker
   ```

2. **Use in trusted environments only** - Don't expose to untrusted networks

3. **Consider using Xpra or x11docker** for better isolation

4. **Revoke access after use:**
   ```bash
   xhost -local:docker
   ```

## Advanced Configuration

### Capture Specific Window

To capture a specific window instead of the entire display, you can modify the GStreamer pipeline in `webrtc_streamer.py`:

```python
# Use ximagesrc with specific window ID
pipeline_str = f"""
webrtcbin name=webrtc bundle-policy=max-bundle
ximagesrc xid=0x1234567 use-damage=false !
...
"""
```

Find window ID with:
```bash
xwininfo  # Click on the window
```

### Adjust Capture Resolution

Modify the pipeline in `webrtc_streamer.py`:

```python
pipeline_str = f"""
...
video/x-raw,framerate=60/1,width=1920,height=1080 !
...
"""
```

### Change Capture Framerate

```python
pipeline_str = f"""
...
video/x-raw,framerate=30/1 !  # 30 FPS instead of 60
...
"""
```

## Switching Between Modes

### Switch to Host Mode

```bash
# Stop container mode
docker-compose down

# Start in host mode
CAPTURE_MODE=host docker-compose up
```

### Switch to Container Mode

```bash
# Stop host mode
docker-compose down

# Start in container mode (default)
docker-compose up
```

Or edit `docker-compose.yml`:

```yaml
environment:
  - CAPTURE_MODE=container  # or remove this line
```

## Platform Support

| Platform | Host Mode Support | Notes |
|----------|------------------|-------|
| **Linux** | ✅ Full support | Native X11 support |
| **macOS** | ⚠️ Limited | Requires XQuartz, may have issues |
| **Windows** | ❌ Not supported | No native X11 (use WSL2 + X server) |

### macOS Setup (XQuartz)

```bash
# Install XQuartz
brew install --cask xquartz

# Start XQuartz and allow network connections
# XQuartz → Preferences → Security → "Allow connections from network clients"

# Set DISPLAY
export DISPLAY=host.docker.internal:0

# Allow connections
xhost +localhost
```

### Windows Setup (WSL2)

```bash
# In WSL2, install X server on Windows (VcXsrv, Xming, etc.)
# Set DISPLAY to Windows host
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0

# Start with host mode
CAPTURE_MODE=host docker-compose up
```

## Performance Tips

1. **Lower framerate** for better performance: `framerate=30/1`
2. **Reduce bitrate** if network is slow: `target-bitrate=3000000`
3. **Use hardware encoding** if available (NVENC, VAAPI)
4. **Close unnecessary applications** to reduce CPU load

## FAQ

**Q: Can I stream my entire desktop?**  
A: Yes, host mode captures the entire display by default.

**Q: Can I stream just one window?**  
A: Yes, modify the pipeline to use `ximagesrc xid=WINDOW_ID`.

**Q: Does this work with Wayland?**  
A: Limited support. Wayland has stricter security. Consider using XWayland or container mode.

**Q: Can I use this for screen sharing?**  
A: Yes! Host mode is perfect for screen sharing applications.

**Q: Will this work over the internet?**  
A: Yes, but you'll need a TURN server for NAT traversal. See main README.md.

## Related Documentation

- [Main README](README.md) - Full system documentation
- [Games Folder README](games/README.md) - Container mode game upload
- [Docker Compose Reference](docker-compose.yml) - Configuration options
