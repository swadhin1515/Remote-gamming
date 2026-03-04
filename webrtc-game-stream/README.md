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
# Navigate to project directory
cd webrtc-game-stream

# Build and start all containers
docker compose up --build

# Access the web client
# Open http://localhost:8080 in your browser
```

### Using Your Own Game

#### Method 1: Upload Games to the `games/` Folder (Recommended)

Simply place your game executable or script in the `games/` folder:

```bash
# Copy your game to the games folder
cp /path/to/your-game games/

# Make it executable
chmod +x games/your-game

# Restart the game container
docker-compose restart game
```

The game launcher will automatically detect and run the first game it finds in the `games/` folder. Supported formats:
- Shell scripts (`.sh`)
- Python scripts (`.py`)
- Binary executables
- Any executable file

**Example with a shell script:**
```bash
# Create a simple game
cat > games/my-game.sh << 'EOF'
#!/bin/bash
echo "My game is running!"
xterm -e "echo 'Hello from my game'; bash"
EOF

# Make it executable
chmod +x games/my-game.sh

# Restart to run your game (runs first game found)
docker-compose restart game
```

**Selecting a specific game when you have multiple:**

Edit `docker-compose.yml` and set the `GAME_NAME` environment variable:

```yaml
game:
  environment:
    - GAME_NAME=my-game.sh  # Specify which game to run
```

Or run directly:
```bash
# Run a specific game
GAME_NAME=my-game.sh docker-compose up game

# Or restart with a specific game
docker-compose stop game
GAME_NAME=my-game.sh docker-compose up -d game
```

The launcher will:
- Run the specified game if `GAME_NAME` is set
- Run the first game found if `GAME_NAME` is not set
- Fall back to the default snake game if no games are in the folder

See `games/README.md` for detailed instructions and examples.

#### Method 2: Edit docker-compose.yml

Alternatively, edit `docker-compose.yml` and change the `GAME_CMD` environment variable:

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

1. Update `game/Dockerfile`:
```dockerfile
RUN apt-get install -y pulseaudio
```

2. Update GStreamer pipeline in `game/webrtc_streamer.py`:
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

1. Update `docker-compose.yml`:
```yaml
game:
  environment:
    - TURN_URL=turn:your-turn-server.com:3478
    - TURN_USER=username
    - TURN_PASS=password
```

2. Update `web/client.js`:
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

1. Check signaling connection:
   ```bash
   docker logs webrtc-game-stream-signaling-1
   ```

2. Check game container:
   ```bash
   docker logs webrtc-game-stream-game-1
   ```

3. Verify WebRTC connection in browser console (F12)

### Input not working

1. Ensure `/dev/uinput` is available on host:
   ```bash
   ls -l /dev/uinput
   ```

2. Check container has `privileged: true` or proper capabilities

3. Verify data channel is open (check browser console)

### High latency

1. Use hardware encoding (NVENC/VAAPI) instead of x264enc
2. Reduce bitrate in GStreamer pipeline
3. Add TURN server for better routing
4. Check network conditions

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

## Project Structure

```
webrtc-game-stream/
├── docker-compose.yml          # Orchestrates all containers
├── README.md                   # This file
├── signaling/
│   ├── Dockerfile             # Node.js signaling server
│   └── server.js              # WebSocket relay implementation
├── game/
│   ├── Dockerfile             # Ubuntu + GStreamer + Python
│   ├── requirements.txt       # Python dependencies
│   ├── start.sh               # Startup script (Xvfb + game + streamer)
│   └── webrtc_streamer.py     # Main streaming logic
└── web/
    ├── Dockerfile             # Nginx web server
    ├── nginx.conf             # Nginx configuration
    ├── index.html             # Web client UI
    └── client.js              # WebRTC client implementation
```

## Example: Running a Real Game

### Example 1: SuperTuxKart

```yaml
game:
  environment:
    - GAME_CMD=supertuxkart
  devices:
    - /dev/dri:/dev/dri  # GPU access
```

### Example 2: Doom (via PrBoom)

```yaml
game:
  environment:
    - GAME_CMD=prboom -iwad /path/to/doom.wad
```

### Example 3: Minecraft

```yaml
game:
  environment:
    - GAME_CMD=java -jar minecraft.jar
  devices:
    - /dev/dri:/dev/dri
```

## Development

### Testing Locally

```bash
# Start only signaling server
docker compose up signaling

# Start game container with logs
docker compose up game

# Start web container
docker compose up web
```

### Debugging

Enable verbose logging in `game/webrtc_streamer.py`:
```python
import logging
logging.basicConfig(level=logging.DEBUG)
```

Check GStreamer pipeline:
```bash
docker exec -it webrtc-game-stream-game-1 bash
GST_DEBUG=3 python3 /app/webrtc_streamer.py
```

## License

MIT License - feel free to modify and use for your projects.

## References

- [GStreamer WebRTC Documentation](https://gstreamer.freedesktop.org/documentation/webrtc/index.html)
- [WebRTC DataChannels](https://developer.mozilla.org/en-US/docs/Web/API/WebRTC_API/Using_data_channels)
- [Linux uinput](https://www.kernel.org/doc/html/latest/input/uinput.html)
- [WebRTC Samples](https://webrtc.github.io/samples/)

## Support

For issues or questions:
1. Check the troubleshooting section above
2. Review Docker logs for each container
3. Check browser console for WebRTC errors
4. Verify network connectivity between containers

## Future Enhancements

- [ ] Add audio streaming support
- [ ] Implement TURN server integration
- [ ] Add authentication and authorization
- [ ] Support multiple concurrent streams
- [ ] Add recording functionality
- [ ] Implement adaptive bitrate
- [ ] Add gamepad support
- [ ] Create mobile-friendly UI
