#!/usr/bin/env bash
set -e

pkill -9 Xvfb || true
rm -f /tmp/.X1-lock /tmp/.X11-unix/X1 || true

export DISPLAY=:1

Xvfb :1 -screen 0 1280x720x24 +extension RANDR &
sleep 2

echo "[game] Launching game: ${GAME_CMD}"
bash -lc "${GAME_CMD}" &

echo "[game] Starting WebRTC streamer..."
python3 -u /app/webrtc_streamer.py