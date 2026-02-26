# Docker Build Error Fix Plan

## Problem Analysis

The Docker build is failing with the error:
```
Error: Unable to find a match: gstreamer1-plugins-bad-free-extras
```

**Root Cause:** The `gstreamer1-plugins-bad-free-extras` package no longer exists as a separate package in CentOS Stream 9. It has been consolidated into the main `gstreamer1-plugins-bad-free` package.

## Package Changes in CentOS Stream 9

In RHEL 9 / CentOS Stream 9, the GStreamer packaging structure was simplified:
- The `-extras` subpackage was **merged into** `gstreamer1-plugins-bad-free`
- All functionality previously in `-extras` is now included in the main package
- No separate installation is needed

## Solution

**Remove the obsolete package reference** from `game/Dockerfile`:

### Current Line (Line 11):
```dockerfile
gstreamer1-plugins-bad-free gstreamer1-plugins-bad-free-extras \
```

### Fixed Line:
```dockerfile
gstreamer1-plugins-bad-free \
```

## Implementation Steps

1. Edit `/root/webrtc-game-stream/game/Dockerfile`
2. Remove `gstreamer1-plugins-bad-free-extras` from line 11
3. Keep `gstreamer1-plugins-bad-free` (which now includes all plugins)
4. Rebuild the Docker image

## Expected Outcome

After this fix:
- The Docker build will complete successfully
- All GStreamer functionality will be preserved (the plugins are still available in the main package)
- No loss of features or capabilities

## Next Steps

Switch to **code mode** to implement this fix.
