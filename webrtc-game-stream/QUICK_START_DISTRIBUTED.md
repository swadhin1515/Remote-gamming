# Quick Start: Distributed Multi-Machine Setup

Stream games from one machine to clients anywhere using separate signaling, game, and web servers.

## 🚀 Quick Setup (3 Steps)

### Step 1: Start Signaling Server (Machine A)

```bash
cd webrtc-game-stream
docker compose -f docker-compose.signaling.yml up -d
```

**Note the IP:** e.g., `192.168.1.100`

### Step 2: Start Game Server (Machine B)

```bash
cd webrtc-game-stream
export SIGNALING_SERVER=192.168.1.100  # IP from Step 1
docker compose -f docker-compose.game.yml up -d
```

### Step 3: Start Web Server (Machine C or same as A)

```bash
cd webrtc-game-stream
export SIGNALING_SERVER=192.168.1.100  # IP from Step 1
docker compose -f docker-compose.web.yml up -d
```

### Step 4: Connect from Browser

Open: `http://<web-server-ip>:8080/?signaling=<signaling-ip>&room=room1`

Example: `http://192.168.1.101:8080/?signaling=192.168.1.100&room=room1`

## 📋 Required Ports

| Machine | Port | Protocol | Purpose |
|---------|------|----------|---------|
| Signaling | 9000 | TCP | WebSocket signaling |
| Game | 10000-20000 | UDP | WebRTC media (P2P) |
| Web | 8080 | TCP | HTTP web interface |

## 🔧 Firewall Setup

```bash
# Signaling server
sudo ufw allow 9000/tcp

# Game machine (for direct P2P)
sudo ufw allow 10000:20000/udp

# Web server
sudo ufw allow 8080/tcp
```

## 🎮 Multiple Games

Run different games on different rooms:

```bash
# Game 1
export SIGNALING_SERVER=192.168.1.100
export ROOM=game1
export GAME_CMD="xterm -e /games/snake.sh"
docker compose -f docker-compose.game.yml up -d

# Game 2 (in another terminal/machine)
export SIGNALING_SERVER=192.168.1.100
export ROOM=game2
export GAME_CMD="xterm -e /games/tetris.sh"
docker compose -f docker-compose.game.yml up -d
```

Connect to: `?room=game1` or `?room=game2`

## 🐛 Troubleshooting

**Connection timeout?**
```bash
# Test signaling server
telnet <signaling-ip> 9000
```

**No video stream?**
```bash
# Check game logs
docker compose -f docker-compose.game.yml logs -f
```

**WebRTC connection fails?**
- Configure TURN server (see MULTI_MACHINE_DEPLOYMENT.md)
- Check firewall allows UDP ports 10000-20000

## 📚 Full Documentation

See [MULTI_MACHINE_DEPLOYMENT.md](./MULTI_MACHINE_DEPLOYMENT.md) for:
- Detailed architecture
- TURN server setup
- Security considerations
- Performance tuning
- Monitoring and scaling

## 🏗️ Architecture

```
Game Machine ──WebSocket──> Signaling Server <──WebSocket── Web Client
     │                                                           │
     └────────────────── WebRTC P2P (direct) ──────────────────┘
```

**Key:** Signaling only handles connection setup. Video streams directly P2P between game and client.
