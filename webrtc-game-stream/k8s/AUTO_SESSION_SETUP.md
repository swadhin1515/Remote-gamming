# Automatic Session Management Setup

## Overview

This guide explains how to set up automatic session management where each browser tab automatically gets its own isolated game session.

## Architecture

```
Browser Tab 1 → Session Manager → Creates GameSession CR → Pod 1 spawned
Browser Tab 2 → Session Manager → Creates GameSession CR → Pod 2 spawned
Browser Tab 3 → Session Manager → Creates GameSession CR → Pod 3 spawned
```

## Components

### 1. Session Manager Service
- **Location**: `k8s/session-manager/`
- **Purpose**: Detects new browser connections and automatically creates GameSession CRs
- **Language**: Python with websockets and kubernetes client
- **Port**: 8080 (WebSocket)

### 2. Modified Web Client
- **Files**: `web/client-auto.js`, `web/index-auto.html`
- **Purpose**: Connects to session manager first, gets assigned room, then connects to game
- **Access**: `http://dockerstream1.fyre.ibm.com:30085/index-auto.html`

## Setup Instructions

### Step 1: Update Web Server with Auto-Session Files

```bash
# Copy new files to web directory
cp /root/webrtc-game-stream/web/client-auto.js /root/webrtc-game-stream/web/
cp /root/webrtc-game-stream/web/index-auto.html /root/webrtc-game-stream/web/

# Rebuild web image
cd /root/webrtc-game-stream
docker build -t web:latest -f web/Dockerfile web/

# Import to K3s
docker save web:latest | k3s ctr images import -

# Restart web server
kubectl rollout restart deployment web-server
```

### Step 2: Build and Deploy Session Manager

```bash
# Build session manager image
cd /root/webrtc-game-stream/k8s/session-manager
docker build -t session-manager:latest .

# Import to K3s
docker save session-manager:latest | k3s ctr images import -

# Deploy session manager
kubectl apply -f /root/webrtc-game-stream/k8s/manifests/session-manager-deployment.yaml

# Verify deployment
kubectl get pods -l app=session-manager
kubectl logs -f deployment/session-manager
```

### Step 3: Access Auto-Session Interface

Open in browser:
```
http://dockerstream1.fyre.ibm.com:30085/index-auto.html
```

Each tab you open will automatically:
1. Connect to session manager
2. Request a new session
3. Get assigned a unique room (e.g., `room-dockerstream1.fyre.ibm.com-session-abc123`)
4. Session manager creates GameSession CR and Pod
5. Browser connects to the game pod
6. When tab closes, session is automatically cleaned up

## How It Works

### Browser Side (client-auto.js)

1. **Connect to Session Manager** (port 8080)
   ```javascript
   ws://dockerstream1.fyre.ibm.com:8080
   ```

2. **Request Session**
   ```json
   {
     "type": "join",
     "room": "room-dockerstream1.fyre.ibm.com-1",
     "username": "user-123456"
   }
   ```

3. **Receive Assignment**
   ```json
   {
     "type": "session_assigned",
     "session_id": "abc123",
     "room": "room-dockerstream1.fyre.ibm.com-1-session-abc123",
     "base_room": "room-dockerstream1.fyre.ibm.com-1"
   }
   ```

4. **Connect to Signaling Server** with assigned room
   ```javascript
   ws://dockerstream1.fyre.ibm.com:9000
   ```

### Server Side (session-manager.py)

1. **Receive Join Request** from browser
2. **Generate Unique Session ID** (e.g., `abc123`)
3. **Create Unique Room** (base_room + `-session-` + session_id)
4. **Create Kubernetes Resources**:
   - GameSession CR
   - Game Pod with assigned room
5. **Track Active Sessions** per base room
6. **On Disconnect**: Delete GameSession CR and Pod

## Testing Multiple Sessions

### Test 1: Single Tab
```bash
# Open in browser
http://dockerstream1.fyre.ibm.com:30085/index-auto.html?room=room-test-1

# Check created resources
kubectl get gamesessions
kubectl get pods -l app=game-session
```

### Test 2: Multiple Tabs
```bash
# Open 3 tabs with same base room
Tab 1: http://dockerstream1.fyre.ibm.com:30085/index-auto.html?room=room-test-1
Tab 2: http://dockerstream1.fyre.ibm.com:30085/index-auto.html?room=room-test-1
Tab 3: http://dockerstream1.fyre.ibm.com:30085/index-auto.html?room=room-test-1

# Each should get unique session
kubectl get gamesessions
# Expected output:
# session-abc123  (for tab 1)
# session-def456  (for tab 2)
# session-ghi789  (for tab 3)
```

### Test 3: Auto Cleanup
```bash
# Close one tab
# Wait 5 seconds
kubectl get gamesessions
# Should show one less session
```

## Monitoring

### View Session Manager Logs
```bash
kubectl logs -f deployment/session-manager
```

Expected output:
```
[2026-03-30 14:30:00] [INFO] Starting Session Manager on port 8080
[2026-03-30 14:30:05] [INFO] New client joined base room 'room-test-1' -> assigned session 'abc123'
[2026-03-30 14:30:05] [INFO] Active sessions for 'room-test-1': 1
[2026-03-30 14:30:05] [INFO] Created GameSession CR: session-abc123
[2026-03-30 14:30:06] [INFO] Created Pod: game-session-abc123 for room: room-test-1-session-abc123
```

### View Active Sessions
```bash
# List all game sessions
kubectl get gamesessions

# List all game pods
kubectl get pods -l app=game-session

# View specific session details
kubectl describe gamesession session-abc123
```

### Check Resource Usage
```bash
kubectl top node
kubectl top pods
```

## Troubleshooting

### Session Manager Not Starting
```bash
# Check pod status
kubectl get pods -l app=session-manager

# View logs
kubectl logs deployment/session-manager

# Check RBAC permissions
kubectl get clusterrolebinding session-manager-binding
```

### Sessions Not Creating
```bash
# Check session manager logs
kubectl logs -f deployment/session-manager

# Verify CRD exists
kubectl get crd gamesessions.gamestream.example.com

# Check RBAC permissions
kubectl auth can-i create gamesessions --as=system:serviceaccount:default:session-manager
kubectl auth can-i create pods --as=system:serviceaccount:default:session-manager
```

### Browser Can't Connect
```bash
# Verify session manager service
kubectl get svc session-manager-service

# Test session manager endpoint
curl -v http://dockerstream1.fyre.ibm.com:8080

# Check web server has new files
kubectl exec -it deployment/web-server -- ls -la /usr/share/nginx/html/
```

### Pods Stuck in Pending
```bash
# Check node resources
kubectl describe node dockerstream1.fyre.ibm.com

# View pod events
kubectl describe pod game-session-abc123

# Reduce resource requests in session-manager.py if needed
```

## Resource Management

### Current Configuration
Each game session requests:
- CPU: 0.5 cores (limit: 1.0)
- Memory: 1Gi (limit: 2Gi)

### For Limited Resources
Edit `k8s/session-manager/session-manager.py` and reduce:
```python
"resources": {
    "limits": {
        "cpu": "0.5",      # Reduced from 1.0
        "memory": "1Gi"    # Reduced from 2Gi
    },
    "requests": {
        "cpu": "0.3",      # Reduced from 0.5
        "memory": "800Mi"  # Reduced from 1Gi
    }
}
```

Then rebuild and redeploy session manager.

## Comparison: Manual vs Auto-Session

| Feature | Manual (create-session.sh) | Auto-Session |
|---------|---------------------------|--------------|
| **Session Creation** | Run script manually | Automatic on page load |
| **Multiple Tabs** | Must create each manually | Automatic per tab |
| **Cleanup** | Manual deletion | Automatic on tab close |
| **User Experience** | Technical, requires kubectl | Simple, just open URL |
| **Room Assignment** | User specifies | Auto-generated unique |
| **Best For** | Testing, development | Production, end users |

## Production Considerations

### 1. Resource Limits
Set appropriate limits based on your hardware:
```python
# In session-manager.py
"resources": {
    "limits": {"cpu": "0.5", "memory": "1Gi"},
    "requests": {"cpu": "0.3", "memory": "800Mi"}
}
```

### 2. Session Timeout
Sessions auto-cleanup after 30 minutes (configurable in GameSession spec):
```python
"sessionTimeout": 30  # minutes
```

### 3. Maximum Concurrent Sessions
Limited only by node resources. Add more nodes to scale:
```bash
# On new node
curl -sfL https://get.k3s.io | K3S_URL=https://dockerstream1.fyre.ibm.com:6443 \
  K3S_TOKEN=$(cat /var/lib/rancher/k3s/server/node-token) sh -
```

### 4. Load Balancing
For multiple session manager replicas:
```yaml
spec:
  replicas: 3  # Multiple instances
```

Session state is tracked per instance, so use sticky sessions or shared state (Redis).

## Next Steps

1. **Deploy Session Manager**: Follow Step 2 above
2. **Test Auto-Session**: Open multiple tabs
3. **Monitor Resources**: Use `kubectl top`
4. **Adjust Limits**: Based on your hardware
5. **Add Nodes**: For more concurrent users

## Support

For issues:
1. Check session manager logs: `kubectl logs deployment/session-manager`
2. Verify RBAC: `kubectl get clusterrolebinding session-manager-binding`
3. Check resources: `kubectl top node`
4. Review pod events: `kubectl describe pod <pod-name>`
