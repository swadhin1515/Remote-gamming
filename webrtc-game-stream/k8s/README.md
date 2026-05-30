# Kubernetes Operator for WebRTC Game Streaming

This directory contains a Kubernetes Operator implementation for managing WebRTC game streaming sessions. The operator provides automatic session management, resource cleanup, and timeout handling.

## Architecture Overview

The system consists of:

1. **Custom Resource Definition (CRD)**: Defines `GameSession` resources
2. **Operator**: Python-based controller using Kopf framework
3. **Signaling Server**: WebRTC signaling service
4. **Web Server**: Frontend for accessing game sessions
5. **Game Pods**: Dynamically created pods for each user session

## Why Use an Operator?

### Advantages over Docker Compose

| Feature | Docker Compose | Kubernetes Operator |
|---------|---------------|---------------------|
| **Scalability** | Manual container management | Automatic pod creation per user |
| **Self-Healing** | Manual restart required | Automatic pod recovery |
| **Resource Management** | Fixed resource allocation | Dynamic resource allocation |
| **Session Lifecycle** | Manual cleanup | Automatic timeout & cleanup |
| **Multi-User** | Pre-defined containers | On-demand session creation |
| **Declarative State** | Imperative commands | Declarative Custom Resources |

### Key Benefits

- **Declarative Management**: Create sessions by applying YAML manifests
- **Automatic Cleanup**: Sessions expire and clean up automatically
- **Self-Healing**: Crashed pods are automatically restarted
- **Scalability**: Handle unlimited concurrent users
- **Resource Efficiency**: Only run pods when sessions are active

## Prerequisites

1. **Kubernetes Cluster** (v1.24+)
   - Minikube, Kind, or production cluster
   - kubectl configured and connected

2. **Docker** for building images

3. **Required Kubernetes Features**
   - Custom Resource Definitions (CRDs)
   - RBAC enabled
   - Privileged containers support (for game input devices)

## Directory Structure

```
k8s/
├── crds/
│   └── gamesession-crd.yaml          # Custom Resource Definition
├── operator/
│   ├── operator.py                    # Operator logic (Kopf)
│   ├── Dockerfile                     # Operator container image
│   └── requirements.txt               # Python dependencies
├── manifests/
│   ├── operator-deployment.yaml       # Operator deployment + RBAC
│   ├── signaling-deployment.yaml      # Signaling server
│   └── web-deployment.yaml            # Web frontend
├── examples/
│   └── example-session.yaml           # Example GameSession resources
└── README.md                          # This file
```

## Quick Start

### Step 1: Build Docker Images

Build all required images:

```bash
# Build game image
cd /root/webrtc-game-stream
docker build -t game:latest -f game/Dockerfile game/

# Build signaling server image
docker build -t signaling:latest -f signaling/Dockerfile signaling/

# Build web server image
docker build -t web:latest -f web/Dockerfile web/

# Build operator image
docker build -t game-operator:latest -f k8s/operator/Dockerfile k8s/operator/
```

### Step 2: Deploy Custom Resource Definition

```bash
kubectl apply -f k8s/crds/gamesession-crd.yaml
```

Verify CRD is installed:
```bash
kubectl get crd gamesessions.gamestream.example.com
```

### Step 3: Deploy Core Services

Deploy signaling server and web frontend:

```bash
# Deploy signaling server
kubectl apply -f k8s/manifests/signaling-deployment.yaml

# Deploy web server
kubectl apply -f k8s/manifests/web-deployment.yaml
```

Verify deployments:
```bash
kubectl get pods
kubectl get services
```

### Step 4: Deploy the Operator

```bash
kubectl apply -f k8s/manifests/operator-deployment.yaml
```

Check operator logs:
```bash
kubectl logs -f deployment/game-operator
```

### Step 5: Create Game Sessions

Create a game session for a user:

```bash
kubectl apply -f k8s/examples/example-session.yaml
```

Or create a custom session:

```yaml
apiVersion: gamestream.example.com/v1
kind: GameSession
metadata:
  name: session-myuser
spec:
  username: "myuser"
  room: "room-dockerstream1.fyre.ibm.com-myuser"
  gameName: "snake.sh"
  sessionTimeout: 30  # minutes
```

## Managing Game Sessions

### List Active Sessions

```bash
kubectl get gamesessions
# or use short name
kubectl get gs
```

Output example:
```
NAME              USERNAME   ROOM                                    PHASE     POD                  AGE
session-player1   player1    room-dockerstream1.fyre.ibm.com-player1 Running   game-session-player1 5m
session-player2   player2    room-dockerstream1.fyre.ibm.com-player2 Running   game-session-player2 3m
```

### View Session Details

```bash
kubectl describe gamesession session-player1
```

### Delete a Session

```bash
kubectl delete gamesession session-player1
```

This will automatically clean up the associated pod and service.

### View Session Logs

```bash
# Get pod name from session
POD_NAME=$(kubectl get gamesession session-player1 -o jsonpath='{.status.podName}')

# View logs
kubectl logs $POD_NAME
```

## Session Configuration

### GameSession Spec Parameters

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `username` | string | Yes | - | Username for the session |
| `room` | string | Yes | - | WebRTC room identifier |
| `gameName` | string | No | "snake.sh" | Game to launch |
| `captureMode` | string | No | "container" | Video capture mode |
| `signalingUrl` | string | No | "ws://signaling-service:9000" | Signaling server URL |
| `sessionTimeout` | integer | No | 30 | Timeout in minutes (0 = no timeout) |
| `resources.limits.cpu` | string | No | "2.0" | CPU limit |
| `resources.limits.memory` | string | No | "4Gi" | Memory limit |
| `resources.requests.cpu` | string | No | "1.0" | CPU request |
| `resources.requests.memory` | string | No | "2Gi" | Memory request |

### Session Timeout Behavior

- **sessionTimeout > 0**: Session automatically expires after specified minutes
- **sessionTimeout = 0**: Session runs indefinitely until manually deleted
- The operator checks for expired sessions every 60 seconds
- Expired sessions are automatically deleted with graceful shutdown

## Accessing Game Sessions

### Via Web Interface

1. Access the web server:
   ```bash
   # Using NodePort (default port 30085)
   http://dockerstream1.fyre.ibm.com:30085
   
   # Or using Ingress (if configured)
   http://dockerstream1.fyre.ibm.com
   ```

2. Enter the room name matching your GameSession:
   ```
   room-dockerstream1.fyre.ibm.com-player1
   ```

### Via Direct URL

```
http://dockerstream1.fyre.ibm.com:30085/?room=room-dockerstream1.fyre.ibm.com-player1
```

## Monitoring and Troubleshooting

### Check Operator Status

```bash
# View operator logs
kubectl logs -f deployment/game-operator

# Check operator pod status
kubectl get pods -l app=game-operator
```

### Check Game Pod Status

```bash
# List all game pods
kubectl get pods -l app=game-session

# View specific game pod logs
kubectl logs game-session-player1

# Describe pod for events
kubectl describe pod game-session-player1
```

### Common Issues

#### 1. Session Not Starting

**Symptoms**: GameSession shows "Pending" phase

**Solutions**:
```bash
# Check operator logs
kubectl logs deployment/game-operator

# Check pod events
kubectl describe gamesession session-name

# Verify game image exists
docker images | grep game
```

#### 2. WebRTC Connection Fails

**Symptoms**: Video stream not appearing in browser

**Solutions**:
```bash
# Check signaling server
kubectl logs deployment/signaling-server

# Verify signaling service
kubectl get svc signaling-service

# Check game pod logs
kubectl logs game-session-player1
```

#### 3. Session Not Expiring

**Symptoms**: Session continues after timeout

**Solutions**:
```bash
# Check operator timer logs
kubectl logs deployment/game-operator | grep timeout

# Verify session timeout setting
kubectl get gamesession session-name -o yaml | grep sessionTimeout
```

#### 4. Insufficient Resources

**Symptoms**: Pods stuck in "Pending" state

**Solutions**:
```bash
# Check node resources
kubectl describe nodes

# Reduce resource requests in GameSession
# Or add more nodes to cluster
```

## Advanced Usage

### Dynamic Session Creation via API

Create a simple API service that generates GameSession resources:

```python
from kubernetes import client, config
import uuid

config.load_incluster_config()
api = client.CustomObjectsApi()

def create_session(username):
    session_id = str(uuid.uuid4())[:8]
    session = {
        "apiVersion": "gamestream.example.com/v1",
        "kind": "GameSession",
        "metadata": {
            "name": f"session-{session_id}"
        },
        "spec": {
            "username": username,
            "room": f"room-{username}-{session_id}",
            "sessionTimeout": 30
        }
    }
    
    api.create_namespaced_custom_object(
        group="gamestream.example.com",
        version="v1",
        namespace="default",
        plural="gamesessions",
        body=session
    )
    
    return session_id
```

### Scaling the Operator

For high availability, increase operator replicas:

```yaml
spec:
  replicas: 3  # Run 3 operator instances
```

Kopf handles leader election automatically.

### Custom Game Images

To use different games, build custom images:

```dockerfile
FROM game:latest
COPY my-game.sh /games/
ENV GAME_NAME=my-game.sh
```

Then reference in GameSession:
```yaml
spec:
  gameName: "my-game.sh"
```

## Cleanup

### Remove All Sessions

```bash
kubectl delete gamesessions --all
```

### Uninstall Operator

```bash
kubectl delete -f k8s/manifests/operator-deployment.yaml
kubectl delete -f k8s/manifests/web-deployment.yaml
kubectl delete -f k8s/manifests/signaling-deployment.yaml
kubectl delete -f k8s/crds/gamesession-crd.yaml
```

## Migration from Docker Compose

### Key Differences

1. **Session Creation**:
   - Docker Compose: Pre-defined containers in YAML
   - Kubernetes: Dynamic GameSession resources

2. **Scaling**:
   - Docker Compose: Manual container addition
   - Kubernetes: Automatic pod creation per session

3. **Cleanup**:
   - Docker Compose: Manual container removal
   - Kubernetes: Automatic timeout-based cleanup

### Migration Steps

1. Build and push images to registry (if using remote cluster)
2. Deploy CRD and operator
3. Deploy core services (signaling, web)
4. Create GameSession resources instead of starting containers
5. Update web frontend to use Kubernetes service names

## Performance Tuning

### Resource Optimization

```yaml
# For low-resource environments
resources:
  limits:
    cpu: "1.0"
    memory: "2Gi"
  requests:
    cpu: "0.5"
    memory: "1Gi"

# For high-performance gaming
resources:
  limits:
    cpu: "4.0"
    memory: "8Gi"
  requests:
    cpu: "2.0"
    memory: "4Gi"
```

### Session Timeout Tuning

```yaml
# Short sessions (testing)
sessionTimeout: 5  # 5 minutes

# Standard sessions
sessionTimeout: 30  # 30 minutes

# Long sessions
sessionTimeout: 120  # 2 hours

# Permanent sessions
sessionTimeout: 0  # No timeout
```

## Security Considerations

1. **Privileged Containers**: Game pods require privileged mode for input devices
2. **RBAC**: Operator has cluster-wide permissions - review for production
3. **Network Policies**: Consider adding network policies to isolate sessions
4. **Resource Limits**: Always set resource limits to prevent resource exhaustion

## Contributing

To extend the operator:

1. Modify `k8s/operator/operator.py`
2. Add new handlers using `@kopf.on.*` decorators
3. Update CRD if adding new spec fields
4. Rebuild operator image
5. Redeploy operator

## Support

For issues or questions:
- Check operator logs: `kubectl logs deployment/game-operator`
- Review Kopf documentation: https://kopf.readthedocs.io/
- Check Kubernetes events: `kubectl get events`

## License

Same as parent project.
