const video = document.getElementById("v");
const status = document.getElementById("status");
const room = "room1";

let pc = null;
let dc = null;
let ws = null;

let midHint = null;
let pendingIce = [];
let negotiationTimeout = null;
let connectionTimeout = null;

function setStatus(msg) {
  status.textContent = msg;
  console.log("[CLIENT]", msg);
}

function connectWebSocket() {
  const wsUrl = `ws://${window.location.hostname}:9000`;
  console.log("[CLIENT] Connecting to:", wsUrl);
  setStatus(`Connecting to ${wsUrl}...`);

  // Add connection timeout
  connectionTimeout = setTimeout(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
      setStatus("❌ Connection timeout - is signaling server running?");
      if (ws) ws.close();
    }
  }, 5000);

  ws = new WebSocket(wsUrl);

  ws.onopen = async () => {
    clearTimeout(connectionTimeout);
    console.log("[CLIENT] WebSocket connected!");
    setStatus("✅ Connected to signaling server");
    await start();
  };

  ws.onerror = (err) => {
    clearTimeout(connectionTimeout);
    console.error("[CLIENT] WebSocket error:", err);
    setStatus("❌ WebSocket error - cannot connect to signaling server");
  };

  ws.onclose = () => {
    clearTimeout(connectionTimeout);
    console.log("[CLIENT] WebSocket closed");
    setStatus("❌ Disconnected from signaling server");
    
    // Attempt reconnection after 3 seconds
    setTimeout(() => {
      setStatus("Attempting to reconnect...");
      connectWebSocket();
    }, 3000);
  };

  ws.onmessage = async (msg) => {
    const m = JSON.parse(msg.data);
    if (m.room !== room) return;

    // ---------- OFFER ----------
    if (m.type === "offer") {
      // Clear timeout since we got the offer
      if (negotiationTimeout) {
        clearTimeout(negotiationTimeout);
        negotiationTimeout = null;
      }

      console.log("[CLIENT] Received offer");
      setStatus("Received offer, creating answer...");

      try {
        const offer = { type: "offer", sdp: m.data };
        await pc.setRemoteDescription(offer);

        // Defer MID discovery until transceivers are populated
        setTimeout(() => {
          const t = pc.getTransceivers().find(tr => tr.mid);
          if (t) {
            midHint = t.mid;
            console.log("[CLIENT] Learned MID:", midHint);
          }
        }, 0);

        const answer = await pc.createAnswer();
        await pc.setLocalDescription(answer);

        ws.send(JSON.stringify({
          room,
          type: "answer",
          data: answer.sdp
        }));

        console.log("[CLIENT] Answer sent");
        setStatus("Answer sent, waiting for connection...");

        // Apply any ICE that arrived early
        for (const c of pendingIce) {
          await pc.addIceCandidate(new RTCIceCandidate(c));
        }
        pendingIce = [];

      } catch (err) {
        console.error("[CLIENT] Offer/answer error:", err);
        setStatus(`❌ ${err.message}`);
      }
    }

    // ---------- ICE ----------
    else if (m.type === "ice") {
      const incoming = m.data;

      const candInit = typeof incoming === "string"
        ? { candidate: incoming }
        : { ...incoming };

      // Ensure Chrome requirements
      if (candInit.sdpMid == null && candInit.sdpMLineIndex == null) {
        if (midHint) candInit.sdpMid = midHint;
        else candInit.sdpMLineIndex = 0;
      }

      if (!pc) {
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

async function start() {
  // Clear any existing timeout
  if (negotiationTimeout) {
    clearTimeout(negotiationTimeout);
  }
  
  // Set timeout for negotiation
  negotiationTimeout = setTimeout(() => {
    setStatus("❌ Connection timeout - no offer received from game");
    console.error("[CLIENT] Negotiation timeout");
  }, 10000); // 10 second timeout

  pc = new RTCPeerConnection({
    iceServers: [{ urls: "stun:stun.l.google.com:19302" }]
  });

  // Add transceiver to receive video (required for proper H264 negotiation)
  pc.addTransceiver("video", { direction: "recvonly" });

  // Handle data channel from peer (game creates it)
  pc.ondatachannel = (event) => {
    console.log("[CLIENT] Data channel received from peer");
    dc = event.channel;
    setupDataChannel();
  };
  
  // Also create data channel ourselves (more reliable)
  dc = pc.createDataChannel("input", { ordered: true });
  setupDataChannel();

  pc.ontrack = (ev) => {
    console.log("[CLIENT] Track received");
    video.srcObject = ev.streams[0];
    setStatus("Video stream received, waiting for connection...");
  };

  pc.onicecandidate = (ev) => {
    if (ev.candidate) {
      ws.send(JSON.stringify({
        room,
        type: "ice",
        data: ev.candidate
      }));
    }
  };

  // Monitor ICE connection state
  pc.oniceconnectionstatechange = () => {
    console.log("[CLIENT] ICE connection state:", pc.iceConnectionState);
    
    switch (pc.iceConnectionState) {
      case "connected":
        setStatus("✅ WebRTC connected");
        break;
      case "disconnected":
        setStatus("⚠️ Connection interrupted");
        break;
      case "failed":
        setStatus("❌ Connection failed - check network/firewall");
        break;
      case "closed":
        setStatus("❌ Connection closed");
        break;
    }
  };

  // Join signaling room
  ws.send(JSON.stringify({
    room,
    type: "join",
    data: "web"
  }));

  console.log("[CLIENT] Waiting for offer from game...");
  setStatus("Waiting for game to send video offer...");

  installInputHandlers();
}

function setupDataChannel() {
  dc.onopen = () => {
    console.log("[CLIENT] Data channel opened");
    setStatus("✅ Connected! Click video to start playing");
  };
  
  dc.onclose = () => {
    console.log("[CLIENT] Data channel closed");
    setStatus("❌ Connection lost");
  };
  
  dc.onerror = (err) => {
    console.error("[CLIENT] Data channel error:", err);
    setStatus("❌ Data channel error");
  };
}

function installInputHandlers() {
  console.log("[CLIENT] Installing input handlers...");
  
  // Keyboard event handlers
  document.addEventListener("keydown", (e) => {
    if (!dc || dc.readyState !== "open") return;
    
    // Prevent default for game keys
    if (["KeyW", "KeyA", "KeyS", "KeyD", "ArrowUp", "ArrowDown", 
         "ArrowLeft", "ArrowRight", "Space", "Enter", "Escape"].includes(e.code)) {
      e.preventDefault();
    }
    
    dc.send(JSON.stringify({
      t: "key",
      code: e.code,
      down: true
    }));
  });
  
  document.addEventListener("keyup", (e) => {
    if (!dc || dc.readyState !== "open") return;
    
    dc.send(JSON.stringify({
      t: "key",
      code: e.code,
      down: false
    }));
  });
  
  // Mouse movement handler
  video.addEventListener("mousemove", (e) => {
    if (!dc || dc.readyState !== "open") return;
    
    dc.send(JSON.stringify({
      t: "mouse",
      dx: e.movementX,
      dy: e.movementY
    }));
  });
  
  // Mouse button handlers
  video.addEventListener("mousedown", (e) => {
    if (!dc || dc.readyState !== "open") return;
    
    e.preventDefault();
    const btn = e.button === 0 ? "left" : "right";
    dc.send(JSON.stringify({
      t: "mouse",
      btn: btn,
      down: true
    }));
  });
  
  video.addEventListener("mouseup", (e) => {
    if (!dc || dc.readyState !== "open") return;
    
    const btn = e.button === 0 ? "left" : "right";
    dc.send(JSON.stringify({
      t: "mouse",
      btn: btn,
      down: false
    }));
  });
  
  // Request pointer lock for better mouse control
  video.addEventListener("click", () => {
    video.requestPointerLock();
  });
  
  console.log("[CLIENT] Input handlers installed");
}

// Start immediately
connectWebSocket();