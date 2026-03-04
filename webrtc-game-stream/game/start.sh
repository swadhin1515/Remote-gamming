#!/usr/bin/env bash
set -e

CAPTURE_MODE="${CAPTURE_MODE:-container}"

if [ "$CAPTURE_MODE" = "host" ]; then
    echo "[game] =========================================="
    echo "[game] HOST CAPTURE MODE"
    echo "[game] =========================================="
    echo "[game] Capturing from host display: ${DISPLAY}"
    echo "[game] Skipping Xvfb and game launch"
    echo "[game] Make sure your game is running on the host!"
    echo "[game] =========================================="
else
    echo "[game] =========================================="
    echo "[game] CONTAINER MODE (default)"
    echo "[game] =========================================="
    echo "[game] Starting Xvfb virtual display..."
    
    pkill -9 Xvfb || true
    rm -f /tmp/.X1-lock /tmp/.X11-unix/X1 || true
    
    export DISPLAY=:1
    
    Xvfb :1 -screen 0 1280x720x24 +extension RANDR &
    sleep 2
    
    echo "[game] Launching game: ${GAME_CMD}"
    bash -lc "${GAME_CMD}" &
    echo "[game] =========================================="
fi

echo "[game] Starting WebRTC streamer..."
python3 -u /app/webrtc_streamer.py