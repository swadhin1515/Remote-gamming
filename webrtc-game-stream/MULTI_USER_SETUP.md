# Multi-User Game Streaming Setup

Enable multiple users to stream and control different game sessions simultaneously.

## Architecture Overview

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   User 1    │     │   User 2    │     │   User 3    │
│  Browser    │     │  Browser    │     │  Browser    │
└──────┬──────┘     └──────┬──────┘     └──────┬──────┘
       │                   │                   │
       │ room1            │ room2            │ room3
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
                    ┌──────▼──────┐
                    │  Signaling  │
                    │   Server    │
                    │  (Port 9000)│
                    └──────┬──────┘
                           │
       ┌───────────────────┼───────────────────┐
       │                   │                   │
┌──────▼──────┐     ┌──────▼──────┐     ┌──────▼──────┐
│   Game      │     │   Game      │     │   Game      │
│ Container 1 │     │ Container 2 │     │ Container 3 │
│  (room1)    │     │  (room2)    │     │  (room3)    │
└─────────────┘     └─────────────┘     └─────────────┘
```

## Quick Start - Multi-User Mode

### Method 1: Docker Compose Scale (Simple)

Scale game containers to support multiple users:

```bash
# Start with 3 game instances
docker-compose up --scale game=3

# Each instance will get a unique room name automatically
```

### Method 2: Manual Room Assignment (Full Control)

Create separate configurations for each user:

```bash
# User 1 - Room 1
ROOM=room1 docker-compose up -d game

# User 2 - Room 2  
ROOM=room2 docker-compose up -d game

# User 3 - Room 3
ROOM=room3 docker-compose up -d game
```

## Configuration

### 1. Update docker-compose.yml for Scaling

```yaml
version: '3.8'

services:
  signaling:
    build: ./signaling
    ports:
      - "9000:9000"
    networks:
      - game-network

  game:
    build: ./game
    depends_on:
      - signaling
    environment:
      - SIGNALING_URL=ws://signaling:9000
      - ROOM=${ROOM:-room${HOSTNAME}}  # Dynamic room based on hostname
      - DISPLAY=:99
      - GAME_CMD=xterm -maximized -e /app/game_launcher.sh
      - GAME_NAME=${GAME_NAME:-}
      - CAPTURE_MODE=${CAPTURE_MODE:-container}
    privileged: true
    devices:
      - /dev/uinput:/dev/uinput
    volumes:
      - ./games:/games:ro
    networks:
      - game-network
    # Allow scaling
    deploy:
      replicas: 3  # Number of game instances

  web:
    build: ./web
    depends_on:
      - signaling
    ports:
      - "8080:80"
    networks:
      - game-network

networks:
  game-network:
    driver: bridge
```

### 2. Update Web Client for Room Selection

Create `web/room-selector.html`:

```html
<!DOCTYPE html>
<html>
<head>
    <title>Select Game Room</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 50px auto;
            padding: 20px;
            background: #1a1a1a;
            color: #fff;
        }
        .room-list {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 20px;
            margin-top: 30px;
        }
        .room-card {
            background: #2a2a2a;
            padding: 20px;
            border-radius: 10px;
            text-align: center;
            cursor: pointer;
            transition: transform 0.2s;
        }
        .room-card:hover {
            transform: scale(1.05);
            background: #3a3a3a;
        }
        .room-card h3 {
            margin: 0 0 10px 0;
        }
        .status {
            display: inline-block;
            padding: 5px 10px;
            border-radius: 5px;
            font-size: 12px;
        }
        .available {
            background: #28a745;
        }
        .occupied {
            background: #dc3545;
        }
        input {
            padding: 10px;
            width: 300px;
            margin: 20px 0;
            border-radius: 5px;
            border: 1px solid #444;
            background: #2a2a2a;
            color: #fff;
        }
        button {
            padding: 10px 20px;
            background: #007bff;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
        }
        button:hover {
            background: #0056b3;
        }
    </style>
</head>
<body>
    <h1>🎮 Select Game Room</h1>
    <p>Choose a room to join or create a new one</p>
    
    <div>
        <input type="text" id="customRoom" placeholder="Enter custom room name">
        <button onclick="joinCustomRoom()">Join Custom Room</button>
    </div>

    <div class="room-list" id="roomList">
        <!-- Rooms will be populated here -->
    </div>

    <script>
        // Generate room cards
        const roomList = document.getElementById('roomList');
        const numRooms = 10; // Number of available rooms

        for (let i = 1; i <= numRooms; i++) {
            const roomCard = document.createElement('div');
            roomCard.className = 'room-card';
            roomCard.innerHTML = `
                <h3>Room ${i}</h3>
                <span class="status available">Available</span>
            `;
            roomCard.onclick = () => joinRoom(`room${i}`);
            roomList.appendChild(roomCard);
        }

        function joinRoom(roomName) {
            // Redirect to game client with room parameter
            window.location.href = `/index.html?room=${roomName}`;
        }

        function joinCustomRoom() {
            const roomName = document.getElementById('customRoom').value.trim();
            if (roomName) {
                joinRoom(roomName);
            } else {
                alert('Please enter a room name');
            }
        }
    </script>
</body>
</html>
```

### 3. Update client.js to Support Room Parameter

Add to `web/client.js`:

```javascript
// Get room from URL parameter
const urlParams = new URLSearchParams(window.location.search);
const room = urlParams.get('room') || 'room1';

// Update WebSocket connection to use room
ws = new WebSocket('ws://localhost:9000');
ws.onopen = () => {
    ws.send(JSON.stringify({ room: room, type: 'join', data: 'web' }));
};
```

## Deployment Strategies

### Strategy 1: Fixed Number of Rooms

Pre-allocate a fixed number of game containers:

```bash
# Start 5 game instances
docker-compose up --scale game=5
```

**Pros:**
- Simple setup
- Predictable resource usage
- Fast user connection

**Cons:**
- Wastes resources when idle
- Limited by pre-allocated number

### Strategy 2: Dynamic Scaling (Kubernetes)

Use Kubernetes Horizontal Pod Autoscaler:

```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-hpa
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game
  minReplicas: 1
  maxReplicas: 10
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
```

**Pros:**
- Scales based on demand
- Cost-effective
- Handles traffic spikes

**Cons:**
- More complex setup
- Requires Kubernetes

### Strategy 3: On-Demand Container Creation

Create containers only when users join:

```javascript
// In signaling server
const { exec } = require('child_process');

function createGameContainer(roomName) {
    exec(`docker run -d --name game-${roomName} -e ROOM=${roomName} game-image`, 
        (error, stdout, stderr) => {
            if (error) {
                console.error(`Error creating container: ${error}`);
                return;
            }
            console.log(`Container created for room: ${roomName}`);
        }
    );
}
```

**Pros:**
- Maximum resource efficiency
- Unlimited rooms
- Pay only for what you use

**Cons:**
- Startup delay (5-10 seconds)
- Requires Docker API access
- More complex orchestration

## Session Management

### Add Session Tracking to Signaling Server

Update `signaling/server.js`:

```javascript
const WebSocket = require('ws');
const wss = new WebSocket.Server({ port: 9000 });

// Track active sessions
const sessions = new Map(); // roomName -> { web: ws, game: ws, users: [] }

wss.on('connection', (ws) => {
    let currentRoom = null;
    let clientType = null;

    ws.on('message', (message) => {
        const msg = JSON.parse(message);
        const { room, type, data } = msg;

        if (type === 'join') {
            currentRoom = room;
            clientType = data; // 'web' or 'game'

            // Initialize room if doesn't exist
            if (!sessions.has(room)) {
                sessions.set(room, { web: null, game: null, users: [] });
            }

            const session = sessions.get(room);

            if (clientType === 'web') {
                session.web = ws;
                session.users.push(ws);
                console.log(`[${room}] Web client joined (${session.users.length} users)`);
            } else if (clientType === 'game') {
                session.game = ws;
                console.log(`[${room}] Game container joined`);
            }

            // Broadcast room status
            broadcastRoomStatus();
        } else {
            // Forward messages within the room
            const session = sessions.get(currentRoom);
            if (session) {
                if (clientType === 'web' && session.game) {
                    session.game.send(message);
                } else if (clientType === 'game' && session.web) {
                    session.web.send(message);
                }
            }
        }
    });

    ws.on('close', () => {
        if (currentRoom && sessions.has(currentRoom)) {
            const session = sessions.get(currentRoom);
            
            if (clientType === 'web') {
                session.users = session.users.filter(u => u !== ws);
                if (session.users.length === 0) {
                    session.web = null;
                }
                console.log(`[${currentRoom}] Web client left (${session.users.length} users)`);
            } else if (clientType === 'game') {
                session.game = null;
                console.log(`[${currentRoom}] Game container disconnected`);
            }

            // Clean up empty rooms
            if (!session.web && !session.game && session.users.length === 0) {
                sessions.delete(currentRoom);
                console.log(`[${currentRoom}] Room cleaned up`);
            }

            broadcastRoomStatus();
        }
    });
});

function broadcastRoomStatus() {
    const status = Array.from(sessions.entries()).map(([room, session]) => ({
        room,
        hasGame: !!session.game,
        userCount: session.users.length
    }));

    // Broadcast to all connected clients
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify({ type: 'room-status', data: status }));
        }
    });
}

console.log('Multi-user signaling server running on port 9000');
```

## Load Balancing

### Nginx Load Balancer Configuration

Create `nginx-lb.conf`:

```nginx
upstream game_containers {
    least_conn;  # Use least connections algorithm
    server game1:9000;
    server game2:9000;
    server game3:9000;
}

server {
    listen 80;
    
    location / {
        proxy_pass http://game_containers;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
    }
}
```

## Resource Management

### Per-Container Resource Limits

```yaml
game:
  deploy:
    resources:
      limits:
        cpus: '2.0'
        memory: 4G
      reservations:
        cpus: '1.0'
        memory: 2G
```

### Monitor Resource Usage

```bash
# Check container stats
docker stats

# Monitor specific containers
docker stats game-room1 game-room2 game-room3
```

## Security Considerations

### 1. Room Access Control

Add authentication to rooms:

```javascript
// Generate room tokens
const crypto = require('crypto');

function generateRoomToken(roomName) {
    return crypto.createHash('sha256')
        .update(roomName + process.env.SECRET_KEY)
        .digest('hex');
}

// Validate token before joining
function validateRoomToken(roomName, token) {
    return token === generateRoomToken(roomName);
}
```

### 2. Rate Limiting

Limit room creation per IP:

```javascript
const rateLimit = new Map(); // ip -> { count, timestamp }

function checkRateLimit(ip) {
    const now = Date.now();
    const limit = rateLimit.get(ip) || { count: 0, timestamp: now };
    
    if (now - limit.timestamp > 60000) {
        // Reset after 1 minute
        limit.count = 0;
        limit.timestamp = now;
    }
    
    limit.count++;
    rateLimit.set(ip, limit);
    
    return limit.count <= 10; // Max 10 rooms per minute
}
```

### 3. Room Cleanup

Automatically clean up inactive rooms:

```javascript
setInterval(() => {
    const now = Date.now();
    sessions.forEach((session, room) => {
        if (session.users.length === 0 && 
            now - session.lastActivity > 300000) { // 5 minutes
            // Stop game container
            exec(`docker stop game-${room}`);
            sessions.delete(room);
            console.log(`Cleaned up inactive room: ${room}`);
        }
    });
}, 60000); // Check every minute
```

## Monitoring and Logging

### Centralized Logging

```yaml
game:
  logging:
    driver: "json-file"
    options:
      max-size: "10m"
      max-file: "3"
      labels: "room,user"
```

### Metrics Collection

Use Prometheus for monitoring:

```yaml
# prometheus.yml
scrape_configs:
  - job_name: 'game-containers'
    static_configs:
      - targets: ['game1:9090', 'game2:9090', 'game3:9090']
```

## Cost Optimization

### 1. Spot Instances (Cloud)

Use spot instances for game containers:
- 70-90% cost savings
- Suitable for stateless game sessions
- Implement graceful shutdown

### 2. Container Pooling

Pre-warm containers for faster startup:

```bash
# Keep 2 warm containers ready
docker-compose up --scale game=2

# Scale up when needed
docker-compose up --scale game=5
```

### 3. Resource Sharing

Multiple light games per container:

```yaml
game:
  environment:
    - MAX_GAMES_PER_CONTAINER=3
```

## Testing Multi-User Setup

### Test Script

```bash
#!/bin/bash

# Start signaling server
docker-compose up -d signaling

# Start 3 game instances
for i in {1..3}; do
    ROOM=room$i docker-compose up -d game
done

# Start web server
docker-compose up -d web

echo "Multi-user setup ready!"
echo "Open http://localhost:8080/room-selector.html"
echo ""
echo "Available rooms:"
echo "  - http://localhost:8080/index.html?room=room1"
echo "  - http://localhost:8080/index.html?room=room2"
echo "  - http://localhost:8080/index.html?room=room3"
```

## Production Checklist

- [ ] Configure resource limits per container
- [ ] Implement room authentication
- [ ] Set up monitoring and alerting
- [ ] Configure auto-scaling rules
- [ ] Implement graceful shutdown
- [ ] Set up centralized logging
- [ ] Configure backup and recovery
- [ ] Test failover scenarios
- [ ] Document scaling procedures
- [ ] Set up cost monitoring

## Next Steps

1. **Start Simple**: Use fixed scaling (3-5 containers)
2. **Monitor Usage**: Track room utilization
3. **Optimize**: Adjust based on actual usage patterns
4. **Scale Up**: Move to Kubernetes for production
5. **Add Features**: Authentication, analytics, recording

## Related Documentation

- [Kubernetes Deployment](KUBERNETES_DEPLOYMENT.md) - Production scaling
- [Main README](README.md) - Basic setup
- [Host Capture Mode](HOST_CAPTURE_MODE.md) - Alternative mode
