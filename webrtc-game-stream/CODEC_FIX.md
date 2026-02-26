# Critical Issue: Browser Rejecting H264 Video

## Problem Identified

The browser is **rejecting the H264 video stream** from the game and responding with VP8 codec instead.

**Evidence from signaling logs:**
- Game offers: `a=rtpmap:32566 H264/90000` with `a=sendrecv`
- Browser answers: `a=rtpmap:120 VP8/90000` with `a=inactive`

This causes the connection to fail because:
1. Game sends H264 encoded video
2. Browser expects VP8 (its default preference)
3. Codecs don't match → no video stream
4. Browser sets direction to `inactive` → rejects the stream

## Root Cause

Firefox (and some browsers) prefer VP8 over H264 by default. When the browser creates an answer to the game's offer, it's selecting VP8 instead of accepting the H264 that the game is offering.

## Solution Required

We need to force the browser to accept H264 codec. There are two approaches:

### Option 1: Set Codec Preferences (Recommended)
Modify the answer SDP to prefer H264 before setting it as local description.

### Option 2: Modify Game to Offer VP8
Change the game's GStreamer pipeline to use VP8 instead of H264 (requires more changes).

## Implementation - Option 1

Add codec preference handling in `web/client.js` after creating the answer:

```javascript
// In the offer handling section, after createAnswer:
const answer = await pc.createAnswer();

// Force H264 codec preference
const sdp = answer.sdp;
const h264Preferred = sdp.replace(
  /(m=video.*\r\n)/,
  '$1a=rtpmap:96 H264/90000\r\na=fmtp:96 profile-level-id=42e01f\r\n'
);

answer.sdp = h264Preferred;
await pc.setLocalDescription(answer);
```

OR simpler approach - just accept whatever codec the game offers by removing codec filtering.

## Quick Fix

The browser needs to be told to accept the H264 codec from the offer. Let me implement this now.
