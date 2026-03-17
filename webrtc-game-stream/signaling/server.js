const WebSocket = require("ws");
const wss = new WebSocket.Server({ port: 9000 });

/**
 * Multi-user signaling server with session management
 * Supports multiple rooms with separate game containers and web clients
 * 
 * Message format:
 * { room: "room1", type: "join|offer|answer|ice|leave", data: ... }
 */

// Track active sessions: roomName -> { web: Set(ws), game: ws, lastActivity: timestamp }
const sessions = new Map();

function getOrCreateSession(room) {
  if (!sessions.has(room)) {
    sessions.set(room, {
      web: new Set(),
      game: null,
      lastActivity: Date.now()
    });
    console.log(`[${room}] Session created`);
  }
  return sessions.get(room);
}

function updateActivity(room) {
  const session = sessions.get(room);
  if (session) {
    session.lastActivity = Date.now();
  }
}

function broadcastRoomStatus() {
  const status = Array.from(sessions.entries()).map(([room, session]) => ({
    room,
    hasGame: !!session.game,
    webClients: session.web.size,
    active: Date.now() - session.lastActivity < 300000 // Active in last 5 minutes
  }));

  // Broadcast to all connected clients
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      try {
        client.send(JSON.stringify({ 
          type: 'room-status', 
          data: status 
        }));
      } catch (err) {
        console.error('Error broadcasting room status:', err);
      }
    }
  });
}

function cleanupSession(room) {
  const session = sessions.get(room);
  if (session && session.web.size === 0 && !session.game) {
    sessions.delete(room);
    console.log(`[${room}] Session cleaned up (no clients)`);
    broadcastRoomStatus();
  }
}

wss.on("connection", (ws) => {
  console.log("New WebSocket connection established");
  
  let currentRoom = null;
  let clientType = null; // 'web' or 'game'

  ws.on("message", (msg) => {
    try {
      const m = JSON.parse(msg.toString());
      const { room, type, data } = m;

      if (!room || !type) {
        console.warn('Invalid message format:', msg.toString());
        return;
      }

      // Handle join message
      if (type === 'join') {
        currentRoom = room;
        clientType = data; // 'web' or 'game'
        
        const session = getOrCreateSession(room);
        updateActivity(room);

        if (clientType === 'web') {
          session.web.add(ws);
          console.log(`[${room}] Web client joined (${session.web.size} total)`);
          
          // Notify game container that web client joined
          if (session.game && session.game.readyState === WebSocket.OPEN) {
            session.game.send(JSON.stringify({ 
              room, 
              type: 'join', 
              data: 'web' 
            }));
          }
        } else if (clientType === 'game') {
          session.game = ws;
          console.log(`[${room}] Game container connected`);
          
          // Notify all web clients that game is ready
          session.web.forEach(webClient => {
            if (webClient.readyState === WebSocket.OPEN) {
              webClient.send(JSON.stringify({ 
                room, 
                type: 'join', 
                data: 'game' 
              }));
            }
          });
        }

        broadcastRoomStatus();
        return;
      }

      // Handle leave message
      if (type === 'leave') {
        console.log(`[${currentRoom}] ${clientType} client leaving`);
        // Will be handled in close event
        return;
      }

      // Forward other messages within the room
      if (currentRoom) {
        const session = sessions.get(currentRoom);
        if (session) {
          updateActivity(currentRoom);

          if (clientType === 'web' && session.game) {
            // Forward from web to game
            if (session.game.readyState === WebSocket.OPEN) {
              session.game.send(msg.toString());
            }
          } else if (clientType === 'game') {
            // Forward from game to all web clients
            session.web.forEach(webClient => {
              if (webClient.readyState === WebSocket.OPEN) {
                webClient.send(msg.toString());
              }
            });
          }
        }
      }
    } catch (err) {
      console.error('Error processing message:', err);
    }
  });

  ws.on("close", () => {
    if (currentRoom && sessions.has(currentRoom)) {
      const session = sessions.get(currentRoom);
      
      if (clientType === 'web') {
        session.web.delete(ws);
        console.log(`[${currentRoom}] Web client disconnected (${session.web.size} remaining)`);
        
        // Notify game container
        if (session.game && session.game.readyState === WebSocket.OPEN) {
          session.game.send(JSON.stringify({ 
            room: currentRoom, 
            type: 'leave', 
            data: 'web' 
          }));
        }
      } else if (clientType === 'game') {
        session.game = null;
        console.log(`[${currentRoom}] Game container disconnected`);
        
        // Notify all web clients
        session.web.forEach(webClient => {
          if (webClient.readyState === WebSocket.OPEN) {
            webClient.send(JSON.stringify({ 
              room: currentRoom, 
              type: 'leave', 
              data: 'game' 
            }));
          }
        });
      }

      // Clean up empty sessions
      cleanupSession(currentRoom);
      broadcastRoomStatus();
    }
  });

  ws.on("error", (err) => {
    console.error('WebSocket error:', err);
  });
});

// Periodic cleanup of inactive sessions (every 5 minutes)
setInterval(() => {
  const now = Date.now();
  const inactiveTimeout = 600000; // 10 minutes

  sessions.forEach((session, room) => {
    if (now - session.lastActivity > inactiveTimeout && 
        session.web.size === 0 && 
        !session.game) {
      sessions.delete(room);
      console.log(`[${room}] Session cleaned up (inactive)`);
    }
  });
}, 300000); // Check every 5 minutes

// Log server stats every minute
setInterval(() => {
  console.log(`\n=== Server Stats ===`);
  console.log(`Active sessions: ${sessions.size}`);
  console.log(`Total connections: ${wss.clients.size}`);
  sessions.forEach((session, room) => {
    console.log(`  [${room}] Web: ${session.web.size}, Game: ${session.game ? 'connected' : 'disconnected'}`);
  });
  console.log(`===================\n`);
}, 60000);

console.log("Multi-user signaling server running on ws://0.0.0.0:9000");
console.log("Supports multiple rooms with session management");