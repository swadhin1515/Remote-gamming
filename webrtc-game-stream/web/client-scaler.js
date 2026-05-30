const video = document.getElementById("video");
const status = document.getElementById("status");

// Get room from URL parameter, default to hostname-based room
const urlParams = new URLSearchParams(window.location.search);
const baseRoom = urlParams.get('room') || `room-${window.location.hostname}`;

// Session scaler for dynamic deployment scaling
const scalerUrl = urlParams.get('scaler') || window.location.hostname;
const signalingServer = urlParams.get('signaling') || window.location.hostname;

console.log(`[CLIENT] Base room: ${baseRoom}`);
console.log(`[CLIENT] Session scaler: ${scalerUrl}`);
console.log(`[CLIENT] Signaling server: ${signalingServer}`);

let pc = null;
let dc = null;
let ws = null;
let scalerWs = null;
let assignedRoom = null;
let replicaIndex = null;

let midHint = null;
let pendingIce = [];
let negotiationTimeout = null;
let connectionTimeout = null;

const ICE_SERVERS = [
  { urls: "stun:stun.l.google.com:19302" },
  {
    urls: "turn:9.30.249.236:443?transport=udp",
    username: "webrtc",
    credential: "SecurePassword123"
  },
  {
    urls: "turn:9.30.249.236:443?transport=tcp",
    username: "webrtc",
    credential: "SecurePassword123"
  }
];

function setStatus(msg) {
  status.textContent = msg;
  console.log("[CLIENT]", msg);
}

// ─── PeerConnection factory ───────────────────────────────────────────────────
function setupPeerConnection() {
  if (pc) {
    pc.ontrack = null;
    pc.onicecandidate = null;
    pc.oniceconnectionstatechange = null;
    pc.ondatachannel = null;
    try { pc.close(); } catch (_) {}
  }
  dc = null;

  pc = new RTCPeerConnection({ iceServers: ICE_SERVERS });

  pc.ondatachannel = (event) => {
    console.log("[CLIENT] Data channel received:", event.channel.label);
    dc = event.channel;
    setupDataChannel();
  };

  pc.ontrack = (ev) => {
    console.log("[CLIENT] Track received");
    video.srcObject = ev.streams[0];
    setStatus("🎮 Video stream received — click to play");
  };

  pc.onicecandidate = (ev) => {
    if (ev.candidate && ws && assignedRoom) {
      ws.send(JSON.stringify({ room: assignedRoom, type: "ice", data: ev.candidate }));
    }
  };

  pc.oniceconnectionstatechange = () => {
    console.log("[CLIENT] ICE state:", pc.iceConnectionState);
    switch (pc.iceConnectionState) {
      case "connected":   setStatus("✅ WebRTC connected"); break;
      case "disconnected":setStatus("⚠️ Connection interrupted"); break;
      case "failed":      setStatus("❌ Connection failed"); break;
      case "closed":      setStatus("❌ Connection closed"); break;
    }
  };
}

// ─── Session Scaler Connection ────────────────────────────────────────────────
async function connectToScaler() {
  const scalerWsUrl = `ws://${scalerUrl}:30086`;
  setStatus(`Requesting session from ${scalerWsUrl}...`);

  scalerWs = new WebSocket(scalerWsUrl);

  scalerWs.onopen = () => {
    console.log("[CLIENT] Connected to session scaler");
    // Request a session - scaler will scale deployment
    scalerWs.send(JSON.stringify({
      type: "join",
      room: baseRoom
    }));
    setStatus("Waiting for deployment scaling...");
  };

  scalerWs.onerror = (err) => {
    console.error("[CLIENT] Session scaler error:", err);
    setStatus("❌ Failed to connect to session scaler");
  };

  scalerWs.onclose = () => {
    console.log("[CLIENT] Session scaler disconnected");
  };

  scalerWs.onmessage = async (msg) => {
    const data = JSON.parse(msg.data);
    console.log("[CLIENT] Scaler message:", data);

    if (data.type === "session_assigned") {
      assignedRoom = data.room;
      replicaIndex = data.replica_index;
      console.log(`[CLIENT] ✅ Assigned to room: ${assignedRoom} (replica ${replicaIndex}/${data.total_replicas})`);
      setStatus(`Assigned to replica ${replicaIndex} - Waiting for pod to start...`);
      
      // Wait a bit for pod to start
      await new Promise(resolve => setTimeout(resolve, 3000));
      
      // Now connect to signaling server with assigned room
      await connectToSignalingServer();
    } else if (data.type === "error") {
      setStatus(`❌ ${data.message}`);
    }
  };
}

// ─── WebSocket / signaling ────────────────────────────────────────────────────
async function connectToSignalingServer() {
  if (!assignedRoom) {
    console.error("[CLIENT] No room assigned yet");
    return;
  }

  const wsUrl = `ws://${signalingServer}:9000`;
  setStatus(`Connecting to signaling server...`);

  connectionTimeout = setTimeout(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      setStatus("❌ Connection timeout — is signaling server running?");
      if (ws) ws.close();
    }
  }, 10000);

  ws = new WebSocket(wsUrl);

  ws.onopen = async () => {
    clearTimeout(connectionTimeout);
    setStatus("✅ Connected to signaling server");
    await start();
  };

  ws.onerror = () => {
    clearTimeout(connectionTimeout);
    setStatus("❌ WebSocket error");
  };

  ws.onclose = () => {
    clearTimeout(connectionTimeout);
    setStatus("❌ Disconnected — reconnecting in 3s...");
    setTimeout(connectToSignalingServer, 3000);
  };

  ws.onmessage = async (msg) => {
    const m = JSON.parse(msg.data);
    if (m.room !== assignedRoom) return;

    // ── OFFER ──────────────────────────────────────────────────────────────
    if (m.type === "offer") {
      if (negotiationTimeout) { clearTimeout(negotiationTimeout); negotiationTimeout = null; }
      console.log("[CLIENT] Received offer — creating fresh PeerConnection");
      setStatus("Received offer, creating answer...");

      setupPeerConnection();
      pendingIce = [];
      midHint = null;

      try {
        await pc.setRemoteDescription({ type: "offer", sdp: m.data });

        setTimeout(() => {
          const t = pc.getTransceivers().find(tr => tr.mid);
          if (t) { midHint = t.mid; console.log("[CLIENT] MID:", midHint); }
        }, 0);

        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);
        ws.send(JSON.stringify({ room: assignedRoom, type: "answer", data: answer.sdp }));
        setStatus("Answer sent, waiting for ICE...");

        for (const c of pendingIce) {
          await pc.addIceCandidate(new RTCIceCandidate(c));
        }
        pendingIce = [];
      } catch (err) {
        console.error("[CLIENT] Offer/answer error:", err);
        setStatus(`❌ ${err.message}`);
      }
    }

    // ── ICE ────────────────────────────────────────────────────────────────
    else if (m.type === "ice") {
      const incoming = m.data;
      const candInit = typeof incoming === "string"
        ? { candidate: incoming }
        : { ...incoming };

      if (candInit.sdpMid == null && candInit.sdpMLineIndex == null) {
        if (midHint) candInit.sdpMid = midHint;
        else candInit.sdpMLineIndex = 0;
      }

      if (!pc || !pc.remoteDescription) {
        pendingIce.push(candInit);
        return;
      }
      try {
        await pc.addIceCandidate(new RTCIceCandidate(candInit));
      } catch (err) {
        console.error("[CLIENT] ICE add failed:", err);
      }
    }
  };
}

// ─── Initial join ─────────────────────────────────────────────────────────────
async function start() {
  if (negotiationTimeout) clearTimeout(negotiationTimeout);
  negotiationTimeout = setTimeout(() => {
    setStatus("❌ No offer received — is the game pod running?");
  }, 15000);

  setupPeerConnection();

  // Tell signaling we're here with our assigned room
  ws.send(JSON.stringify({ room: assignedRoom, type: "join", data: "web" }));
  setStatus("Waiting for game to send video offer...");
  installInputHandlers();
}

// ─── Data channel ─────────────────────────────────────────────────────────────
function setupDataChannel() {
  dc.onopen = () => {
    console.log("[CLIENT] Data channel opened");
    setStatus("✅ Connected! Click video to lock mouse & start playing");
  };
  dc.onclose = () => {
    console.log("[CLIENT] Data channel closed");
    setStatus("⚠️ Data channel closed");
  };
  dc.onerror = (err) => {
    console.error("[CLIENT] Data channel error:", err);
  };
}

// ─── Input handlers (installed once) ─────────────────────────────────────────
let inputHandlersInstalled = false;
function installInputHandlers() {
  if (inputHandlersInstalled) return;
  inputHandlersInstalled = true;

  window.addEventListener("keydown", (e) => {
    if (!dc || dc.readyState !== "open") return;
    e.preventDefault();
    e.stopPropagation();
    dc.send(JSON.stringify({ t: "key", code: e.code, key: e.key, down: true }));
  }, true);

  window.addEventListener("keyup", (e) => {
    if (!dc || dc.readyState !== "open") return;
    e.preventDefault();
    e.stopPropagation();
    dc.send(JSON.stringify({ t: "key", code: e.code, key: e.key, down: false }));
  }, true);

  video.addEventListener("mousemove", (e) => {
    if (!dc || dc.readyState !== "open") return;
    if (document.pointerLockElement !== video) return;
    if (e.movementX === 0 && e.movementY === 0) return;
    dc.send(JSON.stringify({ t: "mouse", dx: e.movementX, dy: e.movementY }));
  });

  video.addEventListener("mousedown", (e) => {
    if (!dc || dc.readyState !== "open") return;
    e.preventDefault();
    const btn = { 0: "left", 1: "middle", 2: "right" }[e.button] || "left";
    dc.send(JSON.stringify({ t: "mouse", btn, down: true }));
  });

  video.addEventListener("mouseup", (e) => {
    if (!dc || dc.readyState !== "open") return;
    const btn = { 0: "left", 1: "middle", 2: "right" }[e.button] || "left";
    dc.send(JSON.stringify({ t: "mouse", btn, down: false }));
  });

  video.addEventListener("wheel", (e) => {
    if (!dc || dc.readyState !== "open") return;
    e.preventDefault();
    dc.send(JSON.stringify({ t: "wheel", dy: e.deltaY }));
  }, { passive: false });

  video.addEventListener("click", () => {
    if (document.pointerLockElement !== video) video.requestPointerLock();
  });

  video.addEventListener("contextmenu", (e) => e.preventDefault());
}

// ─── Boot ─────────────────────────────────────────────────────────────────────
connectToScaler();
