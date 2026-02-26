# Critical WebRTC SDP MID Issue - Root Cause Analysis

## Problem Summary

The WebRTC connection fails with a persistent SDP error:
```
DOMException: mid specified for bundle transport in group attribute does not exist in the SDP. (mid=(null)0)
```

## Root Cause

GStreamer's webrtcbin is generating **invalid SDP** with NULL media identifiers:
- SDP shows: `a=group:BUNDLE (null)0`
- SDP shows: `a=mid:(null)0`
- Expected: `a=group:BUNDLE video0` and `a=mid:video0`

The GStreamer critical error confirms this:
```
GStreamer-SDP-CRITICAL **: gst_sdp_media_set_media: assertion 'med != NULL' failed
```

## Why This Happens

The webrtcbin element in GStreamer is not properly creating transceivers when:
1. We link the pipeline dynamically
2. The transceiver direction/caps aren't explicitly set
3. The pipeline state changes before transceiver is fully configured

## Attempted Fixes (All Failed)

1. ❌ Manual transceiver creation with `add-transceiver` signal
2. ❌ Explicit pad request with `sink_0`
3. ❌ Setting caps on rtpvp8pay
4. ❌ Creating data channel first
5. ❌ Simple element linking
6. ❌ Changing codec from H264 to VP8

## The Real Solution

This requires using GStreamer's **webrtcbin in a specific way**:

### Option 1: Use gst-launch-style pipeline (RECOMMENDED)
```python
pipeline_str = """
webrtcbin name=webrtc bundle-policy=max-bundle
ximagesrc display-name=:99 use-damage=false ! 
video/x-raw,framerate=60/1 ! 
videoconvert ! 
vp8enc deadline=1 cpu-used=8 target-bitrate=6000000 ! 
rtpvp8pay pt=96 ! 
application/x-rtp,media=video,encoding-name=VP8,payload=96 ! 
webrtc.
"""
self.pipe = Gst.parse_launch(pipeline_str)
```

### Option 2: Set transceiver properties BEFORE pipeline starts
```python
# Create webrtcbin
webrtc = Gst.ElementFactory.make("webrtcbin")

# Add to pipeline but DON'T start yet
self.pipe.add(webrtc)

# Create transceiver with explicit properties
caps = Gst.Caps.from_string("application/x-rtp,media=video,encoding-name=VP8,payload=96")
trans = webrtc.emit("add-transceiver", GstWebRTC.WebRTCRTPTransceiverDirection.SENDONLY, caps)
trans.set_property("codec-preferences", caps)

# NOW link the rest of the pipeline
# ... link video source to webrtc.sink_0
```

### Option 3: Use webrtcsink element (GStreamer 1.20+)
```python
# webrtcsink is a higher-level element that handles this correctly
webrtcsink = Gst.ElementFactory.make("webrtcsink")
```

## Recommendation

**Switch to gst-launch-style pipeline** (Option 1) as it's the most reliable way to ensure proper SDP generation with webrtcbin.

## Files to Modify

- `game/webrtc_streamer.py` - Rewrite `build_pipeline()` method to use `Gst.parse_launch()`

## Expected Result

After fix, SDP should show:
```
a=group:BUNDLE video0
a=mid:video0
```

And browser should successfully create answer without MID errors.
