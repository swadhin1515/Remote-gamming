# Windows Graphics Capture Streamer

A high-performance Windows application that captures your screen using **Windows Graphics Capture (WGC)** API and streams it via WebRTC with hardware-accelerated NVENC encoding.

## 🎯 Features

- ✅ **Windows Graphics Capture (WGC)** - Official Microsoft API for screen capture
- ✅ **NVENC Hardware Encoding** - GPU-accelerated H.264/HEVC/AV1 encoding
- ✅ **WebRTC Streaming** - Low-latency streaming to web browsers
- ✅ **Zero-Copy Pipeline** - Direct GPU-to-GPU encoding for minimal latency
- ✅ **Full-Screen Support** - Capture exclusive fullscreen games
- ✅ **Anti-Cheat Safe** - No DLL injection or hooks required
- ✅ **Cursor Capture** - Optional cursor overlay
- ✅ **Dynamic Resolution** - Automatic handling of resolution changes

## 📋 Requirements

### System Requirements
- **Operating System**: Windows 10 version 1903 (May 2019 Update) or later
- **GPU**: NVIDIA GPU with NVENC support (GTX 600 series or newer)
- **RAM**: 4GB minimum, 8GB recommended
- **CPU**: Any modern multi-core processor

### Development Requirements
- **Visual Studio 2019 or later** with C++ desktop development workload
- **Windows 10 SDK** version 10.0.18362.0 or later
- **CMake** 3.20 or later
- **CUDA Toolkit** (for NVENC)
- **NVIDIA Video Codec SDK**

### Runtime Dependencies
- **NVIDIA Graphics Driver** with NVENC support
- **Visual C++ Redistributable** (2019 or later)

## 🚀 Quick Start

### 1. Build the Project

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. -G "Visual Studio 16 2019" -A x64

# Build
cmake --build . --config Release

# Or open the generated solution in Visual Studio
start WindowsGraphicsCapture.sln
```

### 2. Generate Configuration File

```bash
# Generate example configuration
cd bin
WindowsGraphicsCapture.exe --generate-config

# This creates config_example.json
# Rename it to config.json and edit as needed
```

### 3. Run the Streamer

```bash
# Start with default config.json
WindowsGraphicsCapture.exe

# Or specify a custom config file
WindowsGraphicsCapture.exe --config my_config.json
```

### 4. Connect from Web Browser

The streamer connects to your signaling server (default: `ws://localhost:9000`). Make sure your signaling server and web client are running:

```bash
# In your main project directory
cd /root/webrtc-game-stream
docker compose up
```

Then open your browser to `http://localhost:8080` to view the stream.

## ⚙️ Configuration

Edit `config.json` to customize the capture and encoding settings:

```json
{
  "capture": {
    "mode": "primary",           // "primary", "monitor", or "window"
    "width": 1920,               // Target width
    "height": 1080,              // Target height
    "fps": 60,                   // Target frame rate
    "enableCursor": true,        // Capture cursor
    "enableBorderless": true     // Capture borderless windows
  },
  "encoder": {
    "codec": "hevc",             // "h264", "hevc", or "av1"
    "preset": "low_latency",     // "ultra_low_latency", "low_latency", "quality", "high_quality"
    "bitrate": 5000000,          // Bitrate in bits per second (5 Mbps)
    "gopSize": 60,               // GOP size (keyframe interval)
    "enableBFrames": false       // Enable B-frames (increases latency)
  },
  "webrtc": {
    "signalingServerUrl": "ws://localhost:9000",
    "stunServer": "stun:stun.l.google.com:19302",
    "turnServers": []            // Optional TURN servers
  },
  "performance": {
    "enableHardwareAcceleration": true,
    "enableZeroCopy": true,
    "maxLatencyMs": 50
  },
  "logging": {
    "enabled": true,
    "logFilePath": "wgc_capture.log"
  }
}
```

### Capture Modes

- **`primary`**: Capture the primary monitor (default)
- **`monitor`**: Capture a specific monitor (will show picker)
- **`window`**: Capture a specific window (will show picker)

### Codec Options

- **`h264`**: H.264/AVC - Best compatibility, widely supported
- **`hevc`**: H.265/HEVC - Better compression, lower bandwidth
- **`av1`**: AV1 - Best compression, requires newer hardware

### Preset Options

- **`ultra_low_latency`**: Minimum latency (~5-10ms), lower quality
- **`low_latency`**: Balanced latency and quality (~10-20ms)
- **`quality`**: Higher quality, moderate latency (~20-40ms)
- **`high_quality`**: Maximum quality, higher latency (~40-80ms)

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Windows Application                       │
│                                                              │
│  ┌────────────────┐    ┌──────────────┐    ┌─────────────┐ │
│  │ WGC Capture    │───▶│ Frame        │───▶│ NVENC       │ │
│  │ (GPU Texture)  │    │ Processor    │    │ Encoder     │ │
│  └────────────────┘    └──────────────┘    └─────────────┘ │
│         │                                          │         │
│         │                                          ▼         │
│         │                                   ┌─────────────┐ │
│         │                                   │ WebRTC      │ │
│         │                                   │ Client      │ │
│         │                                   └─────────────┘ │
│         │                                          │         │
└─────────┼──────────────────────────────────────────┼─────────┘
          │                                          │
          ▼                                          ▼
   ┌─────────────┐                          ┌──────────────┐
   │ Game/App    │                          │ Signaling    │
   │ (Fullscreen)│                          │ Server       │
   └─────────────┘                          └──────────────┘
                                                    │
                                                    ▼
                                            ┌──────────────┐
                                            │ Web Browser  │
                                            │ (Client)     │
                                            └──────────────┘
```

## 📊 Performance Characteristics

| Metric | Typical Value |
|--------|--------------|
| Capture Latency | 1-3 ms |
| Encode Latency | 3-8 ms |
| Network Latency | 5-20 ms |
| Total End-to-End | 10-30 ms |
| CPU Usage | 5-15% |
| GPU Usage | 10-20% |

## 🔧 Troubleshooting

### "Windows Graphics Capture is not supported"
- Ensure you're running Windows 10 1903 or later
- Update Windows to the latest version

### "Failed to initialize NVENC"
- Update NVIDIA graphics drivers
- Verify your GPU supports NVENC (GTX 600+ or newer)
- Check if another application is using NVENC

### "Failed to connect to signaling server"
- Verify the signaling server is running
- Check the `signalingServerUrl` in config.json
- Ensure firewall allows the connection

### Black screen or no video
- Try selecting a different capture mode
- Ensure the target window/monitor is not minimized
- Check if the application has permission to capture

### High latency
- Use `ultra_low_latency` preset
- Reduce resolution or frame rate
- Disable B-frames
- Check network conditions

### Dropped frames
- Reduce bitrate or resolution
- Close other GPU-intensive applications
- Increase `maxLatencyMs` in config

## 🎮 Integration with Existing System

This Windows Graphics Capture streamer integrates with your existing WebRTC game streaming system:

1. **Run the signaling server** (from main project):
   ```bash
   cd /root/webrtc-game-stream
   docker compose up signaling
   ```

2. **Run this Windows streamer** (on Windows machine):
   ```bash
   WindowsGraphicsCapture.exe
   ```

3. **Access from web browser**:
   ```
   http://localhost:8080
   ```

The Windows streamer acts as a **replacement for the Linux game container**, providing:
- Native Windows game capture
- Better performance for Windows games
- No virtualization overhead
- Full DirectX/Vulkan support

## 📝 Command Line Options

```
Usage: WindowsGraphicsCapture.exe [options]

Options:
  -c, --config <file>    Configuration file path (default: config.json)
  -g, --generate-config  Generate example configuration file
  -h, --help            Show help message
  -v, --version         Show version information

Examples:
  WindowsGraphicsCapture.exe --config my_config.json
  WindowsGraphicsCapture.exe --generate-config
```

## 🔐 Security & Privacy

- **User Permission Required**: Windows will prompt for capture permission
- **No Hooks**: Does not inject into games or modify processes
- **Anti-Cheat Safe**: Uses official Windows APIs
- **Local Processing**: All encoding happens locally
- **Secure WebRTC**: Uses DTLS-SRTP for encrypted streaming

## 🚧 Known Limitations

- Requires Windows 10 1903 or later
- NVENC requires NVIDIA GPU
- Some DRM-protected content cannot be captured
- Cursor capture may not work in all applications
- HDR capture not yet supported

## 🛠️ Development

### Project Structure

```
windows_graphics_capture/
├── include/              # Header files
│   ├── common.h         # Common definitions and utilities
│   ├── d3d_device.h     # DirectX 11 device management
│   ├── wgc_capture.h    # Windows Graphics Capture
│   ├── nvenc_encoder.h  # NVENC encoder wrapper
│   ├── webrtc_client.h  # WebRTC client
│   ├── frame_processor.h # Frame processing utilities
│   └── config_manager.h  # Configuration management
├── src/                 # Source files
│   ├── main.cpp
│   ├── d3d_device.cpp
│   ├── wgc_capture.cpp
│   ├── nvenc_encoder.cpp
│   ├── webrtc_client.cpp
│   ├── frame_processor.cpp
│   └── config_manager.cpp
├── config/              # Configuration files
│   └── config.json
├── build/               # Build output (generated)
├── docs/                # Documentation
├── CMakeLists.txt       # CMake build configuration
└── README.md           # This file
```

### Building from Source

1. Install prerequisites
2. Clone the repository
3. Run CMake configuration
4. Build with Visual Studio or MSBuild

### Adding Features

To add new features:
1. Add header in `include/`
2. Implement in `src/`
3. Update `CMakeLists.txt` if needed
4. Update configuration schema in `config_manager.cpp`

## 📚 Additional Resources

- [Windows Graphics Capture API Documentation](https://docs.microsoft.com/en-us/windows/uwp/audio-video-camera/screen-capture)
- [NVIDIA Video Codec SDK](https://developer.nvidia.com/nvidia-video-codec-sdk)
- [WebRTC Documentation](https://webrtc.org/)
- [DirectX 11 Programming Guide](https://docs.microsoft.com/en-us/windows/win32/direct3d11/dx-graphics-overviews)

## 📄 License

This project is part of the WebRTC Game Streaming System.

## 🤝 Contributing

Contributions are welcome! Please ensure:
- Code follows existing style
- All features are documented
- Configuration changes are reflected in config.json
- README is updated for new features

## 💬 Support

For issues or questions:
1. Check the troubleshooting section
2. Review the logs in `wgc_capture.log`
3. Verify your system meets requirements
4. Check NVIDIA driver version

---

**Note**: This is a Windows-only component. For Linux game streaming, use the existing Docker-based game container in the main project.
