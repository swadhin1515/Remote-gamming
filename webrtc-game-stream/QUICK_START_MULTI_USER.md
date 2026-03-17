# Quick Start - Multi-User Game Streaming

Get multiple users streaming and playing games in under 5 minutes!

## What You'll Get

- Multiple game rooms running simultaneously
- Each user gets their own game session
- Beautiful room selector interface
- Real-time room status updates

## Step 1: Start the Multi-User System

```bash
# Navigate to project directory
cd webrtc-game-stream

# Start with 3 game instances (supports 3 concurrent users)
docker-compose -f docker-compose.multi-user.yml up --build
```

That's it! The system is now running with:
- ✅ Signaling server (port 9000)
- ✅ 3 game containers (room1, room2, room3)
- ✅ Web server (port 8080)

## Step 2: Access the Room Selector

Open your browser and go to:
```
http://localhost:8080/room-selector.html
```

You'll see:
- 10 available rooms
- Real-time status (Available/Active)
- Number of viewers per room
- Custom room creation option

## Step 3: Join a Room

**Option 1: Click a room card**
- Click any "Available" room
- You'll be redirected to the game stream
- Click the video to start playing

**Option 2: Create custom room**
- Enter a custom room name
- Click "Join Room"
- Your game will start in that room

## Step 4: Invite Others

Share the room URL with friends:
```
http://localhost:8080/index.html?room=room1
http://localhost:8080/index.html?room=room2
http://localhost:8080/index.html?room=custom-room-name
```

Each person gets their own game session!

## Scaling to More Users

Need more than 3 concurrent users? Just scale up:

```bash
# Support 10 concurrent users
docker-compose -f docker-compose.multi-user.yml up --scale game=10

# Support 20 concurrent users
docker-compose -f docker-compose.multi-user.yml up --scale game=20
```

## Upload Your Own Game

1. Place your game in the `games/` folder:
```bash
cp my-game.sh games/
chmod +x games/my-game.sh
```

2. Restart the containers:
```bash
docker-compose -f docker-compose.multi-user.yml restart game
```

3. All rooms will now run your game!

## Different Games Per Room

Want different games in different rooms?

```bash
# Room 1 with game1
ROOM=room1 GAME_NAME=game1.sh docker-compose up -d game

# Room 2 with game2
ROOM=room2 GAME_NAME=game2.sh docker-compose up -d game

# Room 3 with game3
ROOM=room3 GAME_NAME=game3.sh docker-compose up -d game
```

## Monitoring

Check active sessions:
```bash
# View logs
docker-compose -f docker-compose.multi-user.yml logs -f signaling

# Check container stats
docker stats
```

The signaling server logs show:
- Active sessions
- Connected clients per room
- Real-time connection status

## Troubleshooting

### No rooms showing as available?
```bash
# Check if game containers are running
docker ps | grep game

# Restart if needed
docker-compose -f docker-compose.multi-user.yml restart game
```

### Can't connect to a room?
```bash
# Check signaling server
docker-compose -f docker-compose.multi-user.yml logs signaling

# Verify port 9000 is accessible
curl http://localhost:9000
```

### Game not starting?
```bash
# Check game container logs
docker-compose -f docker-compose.multi-user.yml logs game

# Verify games folder has executable files
ls -la games/
```

## Architecture Overview

```
Users → Room Selector → Choose Room → Game Stream
                           ↓
                    Signaling Server
                           ↓
                    Game Containers
                    (one per room)
```

## Resource Usage

Per game container:
- CPU: 1-2 cores
- RAM: 2-4 GB
- Network: ~6 Mbps per stream

Example: 10 concurrent users = ~20 CPU cores, 40 GB RAM

## Production Deployment

For production with auto-scaling, see:
- [MULTI_USER_SETUP.md](MULTI_USER_SETUP.md) - Detailed configuration
- [KUBERNETES_DEPLOYMENT.md](KUBERNETES_DEPLOYMENT.md) - Cloud deployment

## Common Use Cases

### 1. Game Testing Party
```bash
# Start 5 rooms for your team
docker-compose -f docker-compose.multi-user.yml up --scale game=5
```

### 2. Public Demo
```bash
# Start 20 rooms for public access
docker-compose -f docker-compose.multi-user.yml up --scale game=20
```

### 3. Development
```bash
# Start 1 room for testing
docker-compose -f docker-compose.multi-user.yml up --scale game=1
```

## Next Steps

- ✅ **Working?** Share the room selector URL with others!
- 📚 **Learn more:** Read [MULTI_USER_SETUP.md](MULTI_USER_SETUP.md)
- ☁️ **Deploy to cloud:** See [KUBERNETES_DEPLOYMENT.md](KUBERNETES_DEPLOYMENT.md)
- 🎮 **Add your game:** Check [games/README.md](games/README.md)

## Support

Having issues? Check:
1. Docker is running: `docker ps`
2. Ports are free: `lsof -i :8080,9000`
3. Games folder exists: `ls games/`
4. Logs for errors: `docker-compose logs`

---

**That's it!** You now have a multi-user game streaming system running. 🎉
