#!/usr/bin/env bash
set -e

# Clean up any existing Xvfb processes and lock files
echo "[game] Cleaning up any existing Xvfb processes..."
pkill -9 Xvfb || true
rm -f /tmp/.X99-lock /tmp/.X11-unix/X99 || true

echo "[game] Starting Xvfb on ${DISPLAY}..."
Xvfb ${DISPLAY} -screen 0 1280x720x24 +extension RANDR &
sleep 1

echo "[game] Launching game: ${GAME_CMD}"
bash -lc "${GAME_CMD}" &

echo "[game] Starting WebRTC streamer..."
python3 -u /app/webrtc_streamer.py
