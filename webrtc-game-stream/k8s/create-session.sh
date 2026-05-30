#!/bin/bash
# Manual Game Session Creator
# This script creates game sessions without the operator
# Use this until the operator is fully deployed

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

# Check if kubectl is available
if ! command -v kubectl &> /dev/null; then
    print_error "kubectl not found"
    exit 1
fi

# Get parameters
USERNAME=${1:-"player1"}
ROOM=${2:-"room-${USERNAME}"}
SESSION_NAME="session-${USERNAME}"

echo "Creating game session:"
echo "  Username: $USERNAME"
echo "  Room: $ROOM"
echo "  Session: $SESSION_NAME"
echo ""

# Create the GameSession resource
cat <<EOF | kubectl apply -f -
apiVersion: gamestream.example.com/v1
kind: GameSession
metadata:
  name: ${SESSION_NAME}
  namespace: default
spec:
  username: "${USERNAME}"
  room: "${ROOM}"
  gameName: "snake.sh"
  captureMode: "container"
  signalingUrl: "ws://signaling-service:9000"
  sessionTimeout: 30
  resources:
    limits:
      cpu: "2.0"
      memory: "4Gi"
    requests:
      cpu: "1.0"
      memory: "2Gi"
EOF

if [ $? -eq 0 ]; then
    print_status "GameSession created: ${SESSION_NAME}"
    echo ""
    echo "Note: Without the operator running, you need to manually create the pod."
    echo "Creating pod manually..."
    echo ""
    
    # Create pod manually
    cat <<EOF | kubectl apply -f -
apiVersion: v1
kind: Pod
metadata:
  name: game-${SESSION_NAME}
  namespace: default
  labels:
    app: game-session
    session: ${SESSION_NAME}
    username: ${USERNAME}
spec:
  restartPolicy: Never
  containers:
  - name: game
    image: game:latest
    imagePullPolicy: Never
    env:
    - name: SIGNALING_URL
      value: "ws://signaling-service:9000"
    - name: ROOM
      value: "${ROOM}"
    - name: DISPLAY
      value: ":99"
    - name: GAME_CMD
      value: "xterm -maximized -e /app/game_launcher.sh"
    - name: GAME_NAME
      value: "snake.sh"
    - name: CAPTURE_MODE
      value: "container"
    - name: USERNAME
      value: "${USERNAME}"
    resources:
      limits:
        cpu: "2.0"
        memory: "4Gi"
      requests:
        cpu: "1.0"
        memory: "2Gi"
    securityContext:
      privileged: true
    volumeMounts:
    - name: games
      mountPath: /games
      readOnly: true
    - name: dev-uinput
      mountPath: /dev/uinput
  volumes:
  - name: games
    hostPath:
      path: /root/webrtc-game-stream/games
      type: DirectoryOrCreate
  - name: dev-uinput
    hostPath:
      path: /dev/uinput
      type: CharDevice
EOF

    if [ $? -eq 0 ]; then
        print_status "Pod created: game-${SESSION_NAME}"
        echo ""
        echo "Access your game at:"
        echo "  http://$(hostname):30085/?room=${ROOM}"
        echo ""
        echo "Check status:"
        echo "  kubectl get pod game-${SESSION_NAME}"
        echo "  kubectl logs game-${SESSION_NAME}"
    else
        print_error "Failed to create pod"
        exit 1
    fi
else
    print_error "Failed to create GameSession"
    exit 1
fi
