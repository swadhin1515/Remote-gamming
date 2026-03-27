# WebRTC Integration Options

## Current Status

The Windows Graphics Capture implementation includes:
- ✅ Screen capture (WGC API)
- ✅ Hardware encoding (NVENC)
- ✅ WebSocket signaling
- ⚠️ **WebRTC media streaming (placeholder)**

## Why WebRTC Library is Needed

The current `webrtc_client.cpp` has signaling but not actual media transport. You need a full WebRTC implementation for:

1. **RTP packetization** - Breaking encoded frames into network packets
2. **SRTP encryption** - Securing the video stream
3. **ICE/STUN/TURN** - NAT traversal
4. **Jitter buffer** - Handling network variations
5. **Congestion control** - Adapting to bandwidth

## Integration Options

### Option 1: Add libwebrtc (Most Complete)

**Pros:**
- Full WebRTC stack
- Best compatibility
- Production-ready

**Cons:**
- Large dependency (~500MB)
- Complex build process
- Long compile times

**Implementation:**
```cpp
// Add to CMakeLists.txt
find_package(WebRTC REQUIRED)
target_link_libraries(${PROJECT_NAME} WebRTC::webrtc)

// In webrtc_client.cpp
#include <api/peer_connection_interface.h>

class WebRTCClient {
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory_;
    rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
    
    // Implement video source that feeds NVENC frames
    class NVENCVideoSource : public webrtc::VideoTrackSourceInterface {
        void OnEncodedFrame(const EncodedFrame& frame) {
            // Send to WebRTC
        }
    };
};
```

### Option 2: Stream RTP to Existing Infrastructure (Easiest)

**Pros:**
- Reuse existing WebRTC setup
- No complex WebRTC integration
- Quick to implement

**Cons:**
- Extra network hop
- Slightly higher latency

**Architecture:**
```
Windows PC          →    Linux Server    →    Browser
WGC + NVENC         →    GStreamer       →    WebRTC
(RTP stream)             (webrtcbin)
```

**Implementation:**

1. **Modify Windows streamer to output RTP:**

```cpp
// Add to nvenc_encoder.cpp
ErrorCode NVENCEncoder::sendRTP(const EncodedFrame& frame) {
    // Create RTP packet
    RTPPacket packet;
    packet.payload_type = 96; // H.264
    packet.timestamp = frame.timestamp;
    packet.data = frame.data;
    
    // Send via UDP
    sendto(rtp_socket, &packet, packet.size(), 0, 
           (struct sockaddr*)&server_addr, sizeof(server_addr));
}
```

2. **Update game container to receive RTP:**

```python
# webrtc_streamer.py
pipeline = (
    "udpsrc port=5000 caps=\"application/x-rtp,encoding-name=H264\" ! "
    "rtph264depay ! h264parse ! "
    "webrtcbin name=sendrecv"
)
```

3. **Configure Windows streamer:**

```json
{
  "streaming": {
    "mode": "rtp",
    "target_host": "192.168.1.100",
    "target_port": 5000
  }
}
```

### Option 3: Use WebRTC Native Library

**Pros:**
- Lighter than full libwebrtc
- Easier to integrate
- Good performance

**Cons:**
- Less mature
- May need custom patches

**Libraries to consider:**
- **webrtc-native** - C++ wrapper
- **libdatachannel** - Lightweight WebRTC
- **rawrtc** - Minimal WebRTC stack

### Option 4: Use Existing WebRTC Service

**Pros:**
- No WebRTC code needed
- Managed infrastructure
- Scalable

**Cons:**
- Requires internet
- Monthly costs
- Less control

**Services:**
- Agora.io
- Twilio Video
- Amazon Kinesis Video Streams

## Recommended Approach

### For Quick Testing: Option 2 (RTP to existing infrastructure)

1. Modify Windows streamer to send RTP
2. Update game container to receive RTP
3. Keep existing WebRTC signaling

### For Production: Option 1 (Full libwebrtc)

1. Build libwebrtc for Windows
2. Integrate into Windows streamer
3. Direct peer-to-peer streaming

## Implementation Guide: RTP Streaming (Quick Start)

### Step 1: Add RTP Output to Windows Streamer

```cpp
// Add to include/rtp_sender.h
class RTPSender {
public:
    ErrorCode initialize(const std::string& host, int port);
    ErrorCode sendFrame(const EncodedFrame& frame);
    
private:
    SOCKET socket_;
    sockaddr_in server_addr_;
    uint16_t sequence_number_;
    uint32_t timestamp_;
};
```

### Step 2: Modify NVENC Encoder

```cpp
// In nvenc_encoder.cpp
ErrorCode NVENCEncoder::encodeFrame(const Frame& frame, EncodedFrameCallback callback) {
    // ... existing encoding code ...
    
    // Send via RTP instead of WebRTC
    if (rtp_sender_) {
        rtp_sender_->sendFrame(encodedFrame);
    }
    
    // Also call callback for local processing
    if (callback) {
        callback(encodedFrame);
    }
}
```

### Step 3: Update Configuration

```json
{
  "streaming": {
    "mode": "rtp",
    "target": {
      "host": "192.168.1.100",
      "port": 5000
    }
  }
}
```

### Step 4: Update Game Container

```python
# In webrtc_streamer.py
def create_pipeline(self):
    if self.config.get('input_mode') == 'rtp':
        # Receive from Windows streamer
        source = (
            f"udpsrc port={self.config['rtp_port']} "
            "caps=\"application/x-rtp,encoding-name=H264,payload=96\" ! "
            "rtph264depay ! h264parse ! "
        )
    else:
        # Existing X11 capture
        source = "ximagesrc ! videoconvert ! "
    
    pipeline = source + "webrtcbin name=sendrecv"
    return pipeline
```

## Testing the Integration

### 1. Start Signaling Server
```bash
docker compose up signaling
```

### 2. Start Game Container (RTP receiver)
```bash
docker compose up game
```

### 3. Start Windows Streamer
```cmd
WindowsGraphicsCapture.exe --config config_rtp.json
```

### 4. Open Browser
```
http://localhost:8080
```

## Performance Comparison

| Approach | Latency | Complexity | Compatibility |
|----------|---------|------------|---------------|
| Direct WebRTC | 10-30ms | High | Best |
| RTP → WebRTC | 20-50ms | Low | Good |
| WebRTC Service | 50-100ms | Very Low | Best |

## Next Steps

1. Choose integration approach based on your needs
2. Implement RTP streaming for quick testing
3. Plan migration to full WebRTC for production
4. Test with your existing infrastructure
5. Optimize based on performance metrics

## Support

For WebRTC integration help:
- Check WebRTC.org documentation
- Review GStreamer webrtcbin examples
- Test with simple RTP streaming first
- Monitor network traffic with Wireshark
