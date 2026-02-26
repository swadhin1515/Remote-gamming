const WebSocket = require("ws");
const wss = new WebSocket.Server({ port: 9000 });

/**
 * Minimal signaling: broadcast messages to others in same room.
 * Message format:
 * { room: "room1", type: "offer|answer|ice", data: ... }
 */
const rooms = new Map(); // room -> Set(ws)

function join(room, ws) {
  if (!rooms.has(room)) rooms.set(room, new Set());
  rooms.get(room).add(ws);
  ws._room = room;
}

function leave(ws) {
  const room = ws._room;
  if (!room) return;
  const set = rooms.get(room);
  if (set) {
    set.delete(ws);
    if (set.size === 0) rooms.delete(room);
  }
}

wss.on("connection", (ws) => {
  console.log("New WebSocket connection established");
  ws.on("message", (msg) => {
    console.log("Received message:", msg.toString());
    let m;
    try { m = JSON.parse(msg.toString()); } catch { return; }
    if (!m.room || !m.type) return;

    if (!ws._room) join(m.room, ws);

    // Relay to others in the room
    const peers = rooms.get(m.room) || new Set();
    for (const p of peers) {
      if (p !== ws && p.readyState === WebSocket.OPEN) {
        p.send(JSON.stringify(m));
      }
    }
  });

  ws.on("close", () => leave(ws));
});

console.log("Signaling server running on ws://0.0.0.0:9000");
