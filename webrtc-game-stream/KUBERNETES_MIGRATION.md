# Kubernetes Migration Guide

## Overview

This document explains the migration from Docker Compose to Kubernetes using a custom operator for the WebRTC Game Streaming system.

## Problem Statement

The original `docker-compose.multi-user.yml` had limitations:
- Fixed number of pre-defined game instances (game-1, game-2, game-3)
- Manual scaling required
- No automatic session cleanup
- Multiple browser tabs connecting to the same room would conflict
- No self-healing capabilities

## Solution: Kubernetes Operator

We've implemented a Kubernetes Operator that:
- **Dynamically creates game pods** on-demand per user session
- **Automatically manages lifecycle** with configurable timeouts
- **Self-heals** by restarting crashed pods
- **Declaratively manages state** using Custom Resources
- **Scales infinitely** - no pre-defined limits

## Architecture Comparison

### Before (Docker Compose)
```
User 1 → http://host:8085/?room=room-1 → game-1 container (pre-existing)
User 2 → http://host:8085/?room=room-2 → game-2 container (pre-existing)
User 3 → http://host:8085/?room=room-3 → game-3 container (pre-existing)
User 4 → ❌ No available container
```

### After (Kubernetes Operator)
```
User 1 → Creates GameSession CR → Operator spawns game-pod-1
User 2 → Creates GameSession CR → Operator spawns game-pod-2
User 3 → Creates GameSession CR → Operator spawns game-pod-3
User N → Creates GameSession CR → Operator spawns game-pod-N
```

## Key Components

### 1. Custom Resource Definition (CRD)
- Defines `GameSession` as a Kubernetes resource type
- Location: `k8s/crds/gamesession-crd.yaml`

### 2. Operator (Python + Kopf)
- Watches for GameSession resources
- Creates/deletes pods and services automatically
- Handles session timeouts
- Location: `k8s/operator/operator.py`

### 3. Core Services
- **Signaling Server**: WebRTC signaling (unchanged)
- **Web Server**: Frontend interface (unchanged)
- Location: `k8s/manifests/`

## Quick Start

### 1. Deploy Everything
```bash
cd k8s
./deploy.sh
```

### 2. Create a Game Session
```bash
kubectl apply -f examples/example-session.yaml
```

Or create dynamically:
```yaml
apiVersion: gamestream.example.com/v1
kind: GameSession
metadata:
  name: session-alice
spec:
  username: "alice"
  room: "room-alice-123"
  sessionTimeout: 30  # Auto-cleanup after 30 minutes
```

### 3. Access the Game
```
http://dockerstream1.fyre.ibm.com:30085/?room=room-alice-123
```

## Session Management

### List Active Sessions
```bash
kubectl get gamesessions
```

### View Session Details
```bash
kubectl describe gamesession session-alice
```

### Delete a Session
```bash
kubectl delete gamesession session-alice
```

### View Logs
```bash
kubectl logs game-session-alice
```

## Automatic Features

### 1. Self-Healing
If a game pod crashes, the operator automatically recreates it because the GameSession resource still exists.

### 2. Automatic Cleanup
Sessions with `sessionTimeout > 0` are automatically deleted after the specified time:
- Operator checks every 60 seconds
- Expired sessions are gracefully terminated
- All resources (pods, services) are cleaned up

### 3. Resource Management
Each session has configurable resource limits:
```yaml
resources:
  limits:
    cpu: "2.0"
    memory: "4Gi"
  requests:
    cpu: "1.0"
    memory: "2Gi"
```

## Migration Benefits

| Feature | Docker Compose | Kubernetes Operator |
|---------|---------------|---------------------|
| **Concurrent Users** | 3 (fixed) | Unlimited |
| **Session Creation** | Manual | Automatic |
| **Cleanup** | Manual | Automatic (timeout-based) |
| **Self-Healing** | No | Yes |
| **Scalability** | Limited | Infinite |
| **Resource Efficiency** | Always running | On-demand |
| **State Management** | Imperative | Declarative |

## Integration with Existing System

### Web Frontend
Update your web application to create GameSession resources via Kubernetes API:

```python
from kubernetes import client, config

config.load_incluster_config()
api = client.CustomObjectsApi()

def create_user_session(username):
    session = {
        "apiVersion": "gamestream.example.com/v1",
        "kind": "GameSession",
        "metadata": {"name": f"session-{username}"},
        "spec": {
            "username": username,
            "room": f"room-{username}",
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
```

### Signaling Server
No changes required - uses same WebSocket protocol.

### Game Container
No changes required - same Docker image works in Kubernetes.

## Troubleshooting

### Sessions Not Starting
```bash
# Check operator logs
kubectl logs -f deployment/game-operator

# Check CRD status
kubectl get crd gamesessions.gamestream.example.com
```

### Connection Issues
```bash
# Verify signaling server
kubectl get svc signaling-service
kubectl logs deployment/signaling-server

# Check game pod
kubectl logs game-session-username
```

### Resource Issues
```bash
# Check node resources
kubectl describe nodes

# Adjust resource requests in GameSession spec
```

## Next Steps

1. **Deploy to Production**: Use the deployment script
2. **Integrate with Web App**: Add session creation API
3. **Monitor Sessions**: Use `kubectl get gamesessions`
4. **Scale as Needed**: Kubernetes handles it automatically

## Documentation

- Full documentation: `k8s/README.md`
- Example sessions: `k8s/examples/`
- Deployment script: `k8s/deploy.sh`

## Support

For issues:
1. Check operator logs: `kubectl logs deployment/game-operator`
2. Review session status: `kubectl describe gamesession <name>`
3. Check pod events: `kubectl get events`
