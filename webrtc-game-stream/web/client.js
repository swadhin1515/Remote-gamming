const video = document.getElementById("video");
const status = document.getElementById("status");

// Get room from URL parameter, default to hostname-based room
const urlParams = new URLSearchParams(window.location.search);
const room = urlParams.get('room') || `room-${window.location.hostname}`;

// Get signaling server from URL parameter or environment variable injected by nginx
const signalingServer = urlParams.get('signaling') || window.SIGNALING_SERVER || window.location.hostname;

console.log(`[CLIENT] Joining room: ${room}`);
console.log(`[CLIENT] Signaling server: ${signalingServer}`);

let pc = null;
let dc = null;
let ws = null;

let midHint = null;
let pendingIce = [];
let negotiationTimeout = null;
let connectionTimeout = null;

const ICE_SERVERS = [
  { urls: "stun:stun.l.google.com:19302" },
  {
    urls: "turn:dockerstream1.fyre.ibm.com:443?transport=udp",
    username: "webrtc",
    credential: "SecurePassword123"
  },
  {
    urls: "turn:dockerstream1.fyre.ibm.com:443?transport=tcp",
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
    if (ev.candidate) {
      ws.send(JSON.stringify({ room, type: "ice", data: ev.candidate }));
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

// ─── WebSocket / signaling ────────────────────────────────────────────────────
function connectWebSocket() {
<<<<<<< HEAD
  const wsUrl = `ws://${signalingServer}:9000`;
=======
  const wsUrl = `ws://${window.location.hostname}:9000`;
>>>>>>> ad3d39c6944ab8d767354f549e59b830a883be7f
  setStatus(`Connecting to ${wsUrl}...`);

  connectionTimeout = setTimeout(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      setStatus("❌ Connection timeout — is signaling server running?");
      if (ws) ws.close();
    }
  }, 5000);

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
    setTimeout(connectWebSocket, 3000);
  };

  ws.onmessage = async (msg) => {
    const m = JSON.parse(msg.data);
    if (m.room !== room) return;

    // ── OFFER ──────────────────────────────────────────────────────────────
    if (m.type === "offer") {
      if (negotiationTimeout) { clearTimeout(negotiationTimeout); negotiationTimeout = null; }
      console.log("[CLIENT] Received offer — creating fresh PeerConnection");
      setStatus("Received offer, creating answer...");

      // Always create a fresh PC so stale WebRTC state never blocks reconnects
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
        ws.send(JSON.stringify({ room, type: "answer", data: answer.sdp }));
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
    setStatus("❌ No offer received — is the game running?");
  }, 10000);

  // Create initial PC so ICE candidates can be buffered before offer arrives
  setupPeerConnection();

  // Tell signaling we're here — game will send a fresh offer
  ws.send(JSON.stringify({ room, type: "join", data: "web" }));
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
<<<<<<< HEAD
connectWebSocket();
=======
connectWebSocket();
>>>>>>> ad3d39c6944ab8d767354f549e59b830a883be7f
