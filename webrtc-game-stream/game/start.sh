#!/usr/bin/env bash
set -e

pkill -9 Xvfb || true
rm -f /tmp/.X1-lock /tmp/.X11-unix/X1 || true

export DISPLAY=:1
<<<<<<< HEAD
=======

Xvfb :1 -screen 0 1280x720x24 +extension RANDR &
sleep 2
>>>>>>> ad3d39c6944ab8d767354f549e59b830a883be7f

Xvfb :1 -screen 0 1280x720x24 +extension RANDR &
sleep 2

echo "[game] Launching game in xterm window..."
# Launch the game inside xterm so it creates a visible X11 window
# Explicitly set DISPLAY for the subprocess and redirect errors to see what's happening
# DISPLAY=:1 xterm -maximized -fa 'Monospace' -fs 14 -bg black -fg green -e /app/snake.sh 2>&1 | tee /tmp/xterm.log &
DISPLAY=:1 xterm -maximized -fa 'Monospace' -fs 14 -bg white -fg green -e /app/snake.sh 2>&1 | tee /tmp/xterm.log &

XTERM_PID=$!
echo "[game] xterm launched with PID: $XTERM_PID"
sleep 3
# Verify xterm is still running
if ps -p $XTERM_PID > /dev/null 2>&1; then
    echo "[game] xterm window is running"
else
    echo "[game] WARNING: xterm process exited! Check /tmp/xterm.log"
    cat /tmp/xterm.log 2>/dev/null || echo "No xterm log found"
fi

echo "[game] Starting WebRTC streamer..."
python3 -u /app/webrtc_streamer.py