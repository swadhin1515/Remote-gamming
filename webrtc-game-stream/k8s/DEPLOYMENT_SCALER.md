# Kubernetes Deployment Scaler Setup

## Overview

This approach uses **Kubernetes native deployment scaling** instead of creating individual pods. When a browser tab opens, the deployment's replica count increases by 1. When a tab closes, it decreases by 1.

## Architecture

```
Browser Tab 1 Opens → Scaler scales deployment to 1 replica → Pod 1 created by K8s
Browser Tab 2 Opens → Scaler scales deployment to 2 replicas → Pod 2 created by K8s
Browser Tab 3 Opens → Scaler scales deployment to 3 replicas → Pod 3 created by K8s
Tab Closes → Scaler scales deployment down → K8s terminates pod
```

## Key Difference from Session Manager

| Feature | Session Manager | Deployment Scaler |
|---------|----------------|-------------------|
| **Pod Creation** | Creates individual pods directly | Scales deployment, K8s creates pods |
| **Resource Type** | GameSession CR + Pod | Deployment replicas |
| **Kubernetes Native** | Custom resources | Standard deployment scaling |
| **Complexity** | Higher (CRD required) | Lower (standard K8s) |
| **Best For** | Complex session management | Simple auto-scaling |

## Components

### 1. Session Scaler Service
- **Location**: `k8s/session-scaler/`
- **Purpose**: Monitors browser connections and scales deployment
- **Language**: Python with websockets and kubernetes client
- **Port**: 30086 (NodePort)

### 2. Game Deployment
- **File**: `k8s/manifests/game-deployment.yaml`
- **Initial Replicas**: 0 (scales dynamically)
- **Scaling**: Automatic based on browser connections

### 3. Modified Web Client
- **Files**: `web/client-scaler.js`, `web/index-scaler.html`
- **Purpose**: Connects to scaler, gets room assignment, connects to game
- **Access**: `http://dockerstream1.fyre.ibm.com:30085/index-scaler.html`

## Setup Instructions

### Step 1: Update Web Server

```bash
# Copy new files
cp /root/webrtc-game-stream/web/client-scaler.js /root/webrtc-game-stream/web/
cp /root/webrtc-game-stream/web/index-scaler.html /root/webrtc-game-stream/web/

# Rebuild web image
cd /root/webrtc-game-stream
docker build -t web:latest -f web/Dockerfile web/

# Import to K3s
docker save web:latest | k3s ctr images import -

# Restart web server
kubectl rollout restart deployment web-server
```

### Step 2: Deploy Game Deployment

```bash
# Deploy game deployment (starts at 0 replicas)
kubectl apply -f /root/webrtc-game-stream/k8s/manifests/game-deployment.yaml

# Verify deployment exists
kubectl get deployment game-sessions
```

Expected output:
```
NAME            READY   UP-TO-DATE   AVAILABLE   AGE
game-sessions   0/0     0            0           5s
```

### Step 3: Build and Deploy Session Scaler

```bash
# Build scaler image
cd /root/webrtc-game-stream/k8s/session-scaler
docker build -t session-scaler:latest .

# Import to K3s
docker save session-scaler:latest | k3s ctr images import -

# Deploy scaler
kubectl apply -f /root/webrtc-game-stream/k8s/manifests/session-scaler-deployment.yaml

# Verify deployment
kubectl get pods -l app=session-scaler
kubectl logs -f deployment/session-scaler
```

### Step 4: Access Scaler Interface

Open in browser:
```
http://dockerstream1.fyre.ibm.com:30085/index-scaler.html
```

## How It Works

### Browser Side (client-scaler.js)

1. **Connect to Session Scaler** (port 30086)
   ```javascript
   ws://dockerstream1.fyre.ibm.com:30086
   ```

2. **Request Session**
   ```json
   {
     "type": "join",
     "room": "room-dockerstream1.fyre.ibm.com-1"
   }
   ```

3. **Receive Assignment**
   ```json
   {
     "type": "session_assigned",
     "room": "room-dockerstream1.fyre.ibm.com-1-2",
     "base_room": "room-dockerstream1.fyre.ibm.com-1",
     "replica_index": 2,
     "total_replicas": 2
   }
   ```

4. **Connect to Signaling Server** with assigned room

### Server Side (scaler.py)

1. **Receive Join Request** from browser
2. **Calculate Required Replicas** (number of active connections)
3. **Scale Deployment** using Kubernetes API
   ```python
   deployment.spec.replicas = required_replicas
   apps_api.patch_namespaced_deployment(...)
   ```
4. **Assign Room** based on replica index
5. **Track Active Connections**
6. **On Disconnect**: Scale down deployment

### Kubernetes Side

1. **Deployment Controller** detects replica change
2. **Creates/Deletes Pods** to match desired replicas
3. **Pod Names** are auto-generated (e.g., `game-sessions-abc123-xyz`)
4. **Each Pod** gets unique room via environment variable

## Testing

### Test 1: Single Tab
```bash
# Open in browser
http://dockerstream1.fyre.ibm.com:30085/index-scaler.html?room=room-test-1

# Check deployment scaled to 1
kubectl get deployment game-sessions
# Expected: READY 1/1

# Check pod created
kubectl get pods -l app=game-session
```

### Test 2: Multiple Tabs
```bash
# Open 3 tabs
Tab 1: http://dockerstream1.fyre.ibm.com:30085/index-scaler.html?room=room-test-1
Tab 2: http://dockerstream1.fyre.ibm.com:30085/index-scaler.html?room=room-test-1
Tab 3: http://dockerstream1.fyre.ibm.com:30085/index-scaler.html?room=room-test-1

# Check deployment scaled to 3
kubectl get deployment game-sessions
# Expected: READY 3/3

# Check 3 pods created
kubectl get pods -l app=game-session
```

### Test 3: Auto Scale Down
```bash
# Close one tab
# Wait 5 seconds

# Check deployment scaled to 2
kubectl get deployment game-sessions
# Expected: READY 2/2
```

## Monitoring

### View Scaler Logs
```bash
kubectl logs -f deployment/session-scaler
```

Expected output:
```
[2026-03-30 14:47:00] [INFO] Starting Session Scaler on port 8080
[2026-03-30 14:47:05] [INFO] New connection for base room 'room-test-1' -> assigned 'room-test-1-1'
[2026-03-30 14:47:05] [INFO] Active connections for 'room-test-1': 1
[2026-03-30 14:47:05] [INFO] Scaling deployment game-sessions to 1 replicas
[2026-03-30 14:47:10] [INFO] New connection for base room 'room-test-1' -> assigned 'room-test-1-2'
[2026-03-30 14:47:10] [INFO] Active connections for 'room-test-1': 2
[2026-03-30 14:47:10] [INFO] Scaling deployment game-sessions to 2 replicas
```

### View Deployment Status
```bash
# Watch deployment scaling
kubectl get deployment game-sessions -w

# View deployment details
kubectl describe deployment game-sessions

# View replica set
kubectl get rs -l app=game-session
```

### Check Pods
```bash
# List all game pods
kubectl get pods -l app=game-session

# View specific pod logs
kubectl logs <pod-name>

# Check pod environment
kubectl exec <pod-name> -- env | grep ROOM
```

## Troubleshooting

### Scaler Not Scaling Deployment
```bash
# Check scaler logs
kubectl logs deployment/session-scaler

# Verify RBAC permissions
kubectl auth can-i update deployments --as=system:serviceaccount:default:session-scaler
kubectl auth can-i patch deployments --as=system:serviceaccount:default:session-scaler

# Check deployment exists
kubectl get deployment game-sessions
```

### Pods Not Starting
```bash
# Check deployment events
kubectl describe deployment game-sessions

# Check pod events
kubectl get pods -l app=game-session
kubectl describe pod <pod-name>

# Check node resources
kubectl top node
```

### Browser Can't Connect
```bash
# Verify scaler service
kubectl get svc session-scaler-service

# Test scaler endpoint
curl -v http://dockerstream1.fyre.ibm.com:30086

# Check web server has new files
kubectl exec -it deployment/web-server -- ls -la /usr/share/nginx/html/
```

### Room Assignment Issues
```bash
# Check scaler logs for room assignments
kubectl logs deployment/session-scaler | grep "assigned"

# Verify pod environment variables
kubectl get pods -l app=game-session -o jsonpath='{.items[*].spec.containers[0].env[?(@.name=="ROOM")].value}'
```

## Resource Management

### Current Configuration
Each game replica requests:
- CPU: 0.5 cores (limit: 1.0)
- Memory: 1Gi (limit: 2Gi)

### Adjust Resources
Edit `k8s/manifests/game-deployment.yaml`:
```yaml
resources:
  limits:
    cpu: "0.5"      # Reduced
    memory: "1Gi"   # Reduced
  requests:
    cpu: "0.3"      # Reduced
    memory: "800Mi" # Reduced
```

Then reapply:
```bash
kubectl apply -f k8s/manifests/game-deployment.yaml
```

## Advantages of This Approach

1. **Kubernetes Native**: Uses standard deployment scaling
2. **Simpler**: No custom CRDs required
3. **Automatic Pod Management**: K8s handles pod lifecycle
4. **Rolling Updates**: Can update game image easily
5. **Resource Limits**: Deployment-level resource management
6. **Health Checks**: Can add liveness/readiness probes

## Limitations

1. **Pod Names**: Auto-generated, not predictable
2. **Room Assignment**: Based on replica index, not pod name
3. **Startup Time**: Pods take time to start (3-5 seconds)
4. **No Session Persistence**: Pods are ephemeral

## Production Considerations

### 1. Max Replicas
Set maximum replicas to prevent resource exhaustion:
```python
# In scaler.py
MAX_REPLICAS = 10  # Limit to 10 concurrent sessions

if required_replicas > MAX_REPLICAS:
    required_replicas = MAX_REPLICAS
```

### 2. Min Replicas
Keep minimum replicas for faster response:
```yaml
# In game-deployment.yaml
spec:
  replicas: 1  # Keep 1 pod always running
```

### 3. Pod Disruption Budget
Prevent all pods from being terminated:
```yaml
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: game-sessions-pdb
spec:
  minAvailable: 1
  selector:
    matchLabels:
      app: game-session
```

### 4. Horizontal Pod Autoscaler
Use HPA for CPU-based scaling:
```bash
kubectl autoscale deployment game-sessions --cpu-percent=80 --min=1 --max=10
```

## Comparison: All Approaches

| Approach | Complexity | K8s Native | Auto-Cleanup | Best For |
|----------|-----------|------------|--------------|----------|
| **Manual Scripts** | Low | No | Manual | Testing |
| **Session Manager** | High | Partial | Yes | Complex sessions |
| **Deployment Scaler** | Medium | Yes | Yes | Production |

## Next Steps

1. **Deploy Scaler**: Follow setup instructions above
2. **Test Scaling**: Open multiple tabs
3. **Monitor Resources**: Use `kubectl top`
4. **Adjust Limits**: Based on hardware
5. **Add HPA**: For additional auto-scaling

## Support

For issues:
1. Check scaler logs: `kubectl logs deployment/session-scaler`
2. Verify RBAC: `kubectl auth can-i update deployments --as=system:serviceaccount:default:session-scaler`
3. Check deployment: `kubectl describe deployment game-sessions`
4. Review pod events: `kubectl describe pod <pod-name>`
