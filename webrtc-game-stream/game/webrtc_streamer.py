import asyncio
import json
import os
import websockets

import gi
gi.require_version("Gst", "1.0")
gi.require_version("GstWebRTC", "1.0")
gi.require_version("GstSdp", "1.0")
from gi.repository import Gst, GObject, GstWebRTC, GstSdp

import subprocess
import fcntl
import termios

Gst.init(None)
GObject.threads_init()

SIGNALING_URL = os.environ.get("SIGNALING_URL", "ws://localhost:9000")
ROOM = os.environ.get("ROOM", "room1")
DISPLAY = os.environ.get("DISPLAY", ":99")

# Map browser key codes to terminal byte sequences
KEY_BYTES = {
    "ArrowUp":    b'\x1b[A',
    "ArrowDown":  b'\x1b[B',
    "ArrowRight": b'\x1b[C',
    "ArrowLeft":  b'\x1b[D',
    "KeyW": b'w', "KeyA": b'a', "KeyS": b's', "KeyD": b'd',
    "KeyH": b'h', "KeyJ": b'j', "KeyK": b'k', "KeyL": b'l',
    "KeyP": b'p', "KeyQ": b'q',
    "Space": b' ', "Enter": b'\n', "Escape": b'\x1b',
}

def find_snake_tty():
    """Find the TTY device used by the snake bash process."""
    try:
        result = subprocess.run(['pgrep', '-f', 'snake.sh'], capture_output=True, text=True)
        pids = [p.strip() for p in result.stdout.strip().split('\n') if p.strip()]
        for pid in pids:
            tty_link = f'/proc/{pid}/fd/0'
            try:
                tty = os.readlink(tty_link)
                if tty.startswith('/dev/pts/'):
                    return tty
            except Exception:
                continue
    except Exception:
        pass
    return None

def inject(evt: dict):
    """Inject keystrokes into the snake TTY using TIOCSTI ioctl."""
    t = evt.get("t")
    if t != "key":
        return
    if not evt.get("down"):   # only on key-press, not release
        return
    code = evt.get("code", "")
    data = KEY_BYTES.get(code)
    if not data:
        return
    tty = find_snake_tty()
    if not tty:
        print("[STREAMER] inject: snake TTY not found")
        return
    try:
        with open(tty, 'wb') as fd:
            for byte in data:
                fcntl.ioctl(fd, termios.TIOCSTI, bytes([byte]))
    except Exception as ex:
        print(f"[STREAMER] inject error ({tty}): {ex}")

class WebRTCStreamer:
    def __init__(self):
        self.pipe = None
        self.webrtc = None
        self.ws = None
        self.data_channel = None
        self.loop = None

    async def connect_signaling(self):
        self.ws = await websockets.connect(SIGNALING_URL)
        # Join room (first message will auto-join on server)
        await self.ws.send(json.dumps({"room": ROOM, "type": "join", "data": "game"}))

    def build_pipeline(self):
        # Use gst-launch style pipeline - this ensures proper transceiver/MID creation
        pipeline_str = f"""
        webrtcbin name=webrtc bundle-policy=max-bundle
        ximagesrc display-name={DISPLAY} use-damage=false ! 
        video/x-raw,framerate=60/1 ! 
        videoconvert ! 
        queue ! 
        vp8enc deadline=1 cpu-used=8 target-bitrate=6000000 keyframe-max-dist=60 ! 
        rtpvp8pay pt=96 ! 
        application/x-rtp,media=video,encoding-name=VP8,payload=96 ! 
        webrtc.
        """
        
        print("[STREAMER] Creating pipeline from string...")
        self.pipe = Gst.parse_launch(pipeline_str)
        
        # Get webrtc element reference
        self.webrtc = self.pipe.get_by_name("webrtc")
        self.data_channel = None
        
        # Configure STUN and TURN servers
        print("[STREAMER] Configuring ICE servers...")
        self.webrtc.set_property("stun-server", "stun://stun.l.google.com:19302")
        self.webrtc.set_property("turn-server", "turn://webrtc:SecurePassword123@dockerstream1.fyre.ibm.com:443")
        print("[STREAMER] ICE servers configured")
        
        # Connect WebRTC signals
        print("[STREAMER] Connecting WebRTC signals...")
        self.webrtc.connect("on-negotiation-needed", self.on_negotiation_needed)
        self.webrtc.connect("on-ice-candidate", self.on_ice_candidate)
        self.webrtc.connect("on-data-channel", self.on_data_channel)
        print("[STREAMER] Signals connected")



    def on_data_channel(self, webrtc, channel):
        print(f"[STREAMER] ✅ Data channel received from browser: {channel.get_property('label')}")
        self.data_channel = channel
        channel.connect("on-open", self.on_data_channel_open)
        channel.connect("on-message-string", self.on_data_message)
        channel.connect("on-close", self.on_data_channel_close)
        channel.connect("on-error", self.on_data_channel_error)

    def on_data_channel_open(self, channel):
        print("[STREAMER] ✅ Data channel OPENED - ready to receive input!")

    def on_data_channel_close(self, channel):
        print("[STREAMER] Data channel closed")

    def on_data_channel_error(self, channel, error):
        print(f"[STREAMER] Data channel error: {error}")

    def on_data_message(self, channel, msg):
        try:
            evt = json.loads(msg)
            print(f"[STREAMER] Input received: {evt}")
            inject(evt)
        except Exception as ex:
            print(f"[STREAMER] Failed to parse input message: {ex}")

    def on_ice_candidate(self, element, mlineindex, candidate):
        asyncio.run_coroutine_threadsafe(
            self.ws.send(json.dumps({
                "room": ROOM,
                "type": "ice",
                "data": {"mlineindex": mlineindex, "candidate": candidate}
            })),
            self.loop
        )

    def on_negotiation_needed(self, element):
        print("[STREAMER] *** NEGOTIATION NEEDED CALLBACK TRIGGERED ***")
        print("[STREAMER] Creating offer...")
        promise = Gst.Promise.new_with_change_func(self.on_offer_created, element, None)
        element.emit("create-offer", None, promise)
        print("[STREAMER] Offer creation initiated")

    def on_offer_created(self, promise, element, _):
        print("[STREAMER] *** on_offer_created CALLBACK CALLED ***")
        try:
            promise.wait()
            print("[STREAMER] Promise wait completed")
            reply = promise.get_reply()
            print(f"[STREAMER] Promise reply: {reply}")
            offer = reply.get_value("offer")
            print(f"[STREAMER] Offer extracted: {offer}")
            element.emit("set-local-description", offer, Gst.Promise.new())
            print("[STREAMER] Local description set")

            sdp_text = offer.sdp.as_text()
            print(f"[STREAMER] SDP text length: {len(sdp_text)}")
            print("[STREAMER] Sending offer to signaling server...")
            future = asyncio.run_coroutine_threadsafe(
                self.ws.send(json.dumps({"room": ROOM, "type": "offer", "data": sdp_text})),
                self.loop
            )
            future.result(timeout=5)
            print("[STREAMER] Offer sent successfully!")
        except Exception as e:
            print(f"[STREAMER] ERROR in on_offer_created: {e}")
            import traceback
            traceback.print_exc()

    async def signaling_loop(self):
        async for msg in self.ws:
            m = json.loads(msg)
            t = m.get("type")
            
            # When a browser joins, create a new offer
            if t == "join" and m.get("data") == "web":
                print("[STREAMER] Browser joined! Creating new offer...")
                # Give pipeline a moment to stabilize
                await asyncio.sleep(0.5)
                promise = Gst.Promise.new_with_change_func(self.on_offer_created, self.webrtc, None)
                self.webrtc.emit("create-offer", None, promise)
                continue
            
            if t == "answer":
                _, sdp_msg = GstSdp.SDPMessage.new_from_text(m["data"])
                sdp = GstWebRTC.WebRTCSessionDescription.new(
                    GstWebRTC.WebRTCSDPType.ANSWER,
                    sdp_msg
                )
                self.webrtc.emit("set-remote-description", sdp, Gst.Promise.new())

            elif t == "offer":
                # If browser offers instead (depending on your client flow)
                _, sdp_msg = GstSdp.SDPMessage.new_from_text(m["data"])
                sdp = GstWebRTC.WebRTCSessionDescription.new(
                    GstWebRTC.WebRTCSDPType.OFFER,
                    sdp_msg
                )
                self.webrtc.emit("set-remote-description", sdp, Gst.Promise.new())
                promise = Gst.Promise.new_with_change_func(self.on_answer_created, self.webrtc, None)
                self.webrtc.emit("create-answer", None, promise)

            elif t == "ice":
                data = m["data"]
                # Handle both formats: browser sends sdpMLineIndex, we send mlineindex
                mlineindex = data.get("sdpMLineIndex") or data.get("mlineindex", 0)
                candidate = data.get("candidate")
                if candidate:
                    self.webrtc.emit("add-ice-candidate", mlineindex, candidate)

    def on_answer_created(self, promise, element, _):
        promise.wait()
        reply = promise.get_reply()
        answer = reply.get_value("answer")
        element.emit("set-local-description", answer, Gst.Promise.new())
        sdp_text = answer.sdp.as_text()
        asyncio.run_coroutine_threadsafe(
            self.ws.send(json.dumps({"room": ROOM, "type": "answer", "data": sdp_text})),
            self.loop
        )

    def start(self):
        # Add bus watch to see errors
        bus = self.pipe.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.on_bus_message)
        
        print("[STREAMER] Setting pipeline to PLAYING state...")
        ret = self.pipe.set_state(Gst.State.PLAYING)
        print(f"[STREAMER] Pipeline state change result: {ret}")
        
        if ret == Gst.StateChangeReturn.FAILURE:
            print("[STREAMER] ERROR: Pipeline failed to start!")
            # Get error from bus
            msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.ERROR)
            if msg:
                err, debug = msg.parse_error()
                print(f"[STREAMER] Error: {err.message}")
                print(f"[STREAMER] Debug: {debug}")
        else:
            print("[STREAMER] Pipeline ready, waiting for browser connection...")
            # Create data channel AFTER pipeline is PLAYING so webrtcbin accepts it
            print("[STREAMER] Creating data channel 'input'...")
            channel = self.webrtc.emit("create-data-channel", "input", None)
            if channel:
                self.data_channel = channel
                channel.connect("on-open", self.on_data_channel_open)
                channel.connect("on-message-string", self.on_data_message)
                channel.connect("on-close", self.on_data_channel_close)
                channel.connect("on-error", self.on_data_channel_error)
                print("[STREAMER] Data channel 'input' created and wired")
            else:
                print("[STREAMER] WARNING: create-data-channel returned None")
        
    def on_bus_message(self, bus, message):
        t = message.type
        if t == Gst.MessageType.ERROR:
            err, debug = message.parse_error()
            print(f"[STREAMER] Bus ERROR: {err.message}")
            print(f"[STREAMER] Debug info: {debug}")
        elif t == Gst.MessageType.WARNING:
            err, debug = message.parse_warning()
            print(f"[STREAMER] Bus WARNING: {err.message}")
        elif t == Gst.MessageType.STATE_CHANGED:
            if message.src == self.pipe:
                old, new, pending = message.parse_state_changed()
                print(f"[STREAMER] Pipeline state: {old.value_nick} -> {new.value_nick}")

async def main():
    print("[STREAMER] Starting WebRTC streamer...")
    s = WebRTCStreamer()
    s.loop = asyncio.get_event_loop()
    
    print("[STREAMER] Connecting to signaling server...")
    await s.connect_signaling()
    print("[STREAMER] Connected to signaling server")
    
    print("[STREAMER] Building GStreamer pipeline...")
    s.build_pipeline()
    print("[STREAMER] Pipeline built successfully")
    
    print("[STREAMER] Starting pipeline...")
    s.start()
    print("[STREAMER] Pipeline started, waiting for negotiation...")

    # WebRTC requires exchanging SDP/ICE over signaling before it can connect
    print("[STREAMER] Entering signaling loop...")
    await s.signaling_loop()

if __name__ == "__main__":
    asyncio.run(main())
