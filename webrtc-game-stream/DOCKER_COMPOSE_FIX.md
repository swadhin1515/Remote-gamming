# Docker Compose Network Configuration Fix

## Error Explanation

The error occurs because Docker Compose doesn't allow both `network_mode` and `networks` to be defined on the same service. These are **mutually exclusive** options.

**Current Issue**: Your `docker-compose.yml` has:
- `networks: [qa-net]` on each service
- `network_mode: "host"` on each service
- A `networks` section defining `qa-net`

**You must choose ONE approach:**

---

## Solution 1: Use Host Network Mode (For Remote Access)

**Best for**: Remote access scenarios where you need WebRTC to work

**Complete corrected `docker-compose.yml`**:

```yaml
services:
  signaling:
    build: ./signaling
    network_mode: "host"
    # Remove ports section - host mode uses host's ports directly

  game:
    build: ./game
    depends_on: [signaling]
    network_mode: "host"
    environment:
      - SIGNALING_URL=ws://localhost:9000  # Changed from signaling:9000
      - ROOM=room1
      - DISPLAY=:99
      - GAME_CMD=xterm -maximized -e /app/snake.sh
    privileged: true
    devices:
      - /dev/uinput:/dev/uinput

  web:
    build: ./web
    depends_on: [signaling]
    network_mode: "host"
    # Remove ports section - host mode uses host's ports directly

# Remove the networks section entirely
```

**Key Changes**:
1. ✅ Removed `networks: [qa-net]` from all services
2. ✅ Removed `ports` sections (host mode doesn't need port mapping)
3. ✅ Removed `networks` definition at the bottom
4. ✅ Changed `SIGNALING_URL` to use `localhost` instead of `signaling`

---

## Solution 2: Use Bridge Network (Original Setup)

**Best for**: Local development, better isolation

**Complete corrected `docker-compose.yml`**:

```yaml
services:
  signaling:
    build: ./signaling
    ports:
      - "9000:9000"
    networks: [qa-net]
    # Remove network_mode line

  game:
    build: ./game
    depends_on: [signaling]
    networks: [qa-net]
    environment:
      - SIGNALING_URL=ws://signaling:9000
      - ROOM=room1
      - DISPLAY=:99
      - GAME_CMD=xterm -maximized -e /app/snake.sh
    privileged: true
    devices:
      - /dev/uinput:/dev/uinput
    # Remove network_mode line

  web:
    build: ./web
    depends_on: [signaling]
    ports:
      - "8080:80"
    networks: [qa-net]
    # Remove network_mode line

networks:
  qa-net:
    driver: bridge
```

**Key Changes**:
1. ✅ Removed all `network_mode: "host"` lines
2. ✅ Kept `networks: [qa-net]` on all services
3. ✅ Kept `ports` sections for port mapping
4. ✅ Kept `networks` definition at the bottom

**Note**: This won't fix your remote access issue - you'll still have ICE connection problems.

---

## Recommended Action for Your Use Case

Since you're accessing remotely and experiencing ICE connection issues, **use Solution 1 (Host Network Mode)**.

### Steps:

1. **Backup current file**:
   ```bash
   cp docker-compose.yml docker-compose.yml.backup
   ```

2. **Replace `docker-compose.yml` with Solution 1 content above**

3. **Restart containers**:
   ```bash
   docker compose down
   docker compose up --build
   ```

4. **Test at**: `http://dockerstream1.fyre.ibm.com:8080`

---

## Why Host Network Mode Fixes Remote Access

**With Bridge Network** (original):
- Containers get internal IPs: `172.18.0.x`
- ICE candidates include these internal IPs
- Remote browser cannot reach internal IPs
- ❌ Connection fails

**With Host Network Mode**:
- Containers use host's network directly
- ICE candidates include host's public IP
- Remote browser can reach host's public IP
- ✅ Connection succeeds

---

## Alternative: Keep Bridge Network + Add TURN Server

If you want to keep the bridge network for better isolation, you need to set up a TURN server:

1. **Install Coturn** on your server
2. **Configure TURN** in both `web/client.js` and `game/webrtc_streamer.py`
3. **Open UDP ports** 10000-20000 on firewall

This is more complex but provides better security and isolation.

---

## Quick Reference

| Scenario | Use This | Trade-offs |
|----------|----------|------------|
| Remote access, simple setup | Host network mode | Less isolation, simpler |
| Local development | Bridge network | Better isolation, won't work remotely |
| Production deployment | Bridge + TURN server | Best security, more complex |

---

## Next Steps

1. Choose Solution 1 (Host Network Mode)
2. Update your `docker-compose.yml` file
3. Run `docker compose down && docker compose up --build`
4. Test the connection
5. If it works, you're done! If not, check firewall settings.
