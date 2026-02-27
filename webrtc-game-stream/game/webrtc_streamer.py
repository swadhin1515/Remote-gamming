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
import threading

Gst.init(None)
GObject.threads_init()

SIGNALING_URL = os.environ.get("SIGNALING_URL", "ws://localhost:9000")
ROOM = os.environ.get("ROOM", "room1")
DISPLAY_ENV = os.environ.get("DISPLAY", ":1")

# Lazy-init Xlib display (opened once, reused across reconnects)
_xdisplay = None
_xdisplay_lock = threading.Lock()

def get_xdisplay():
    global _xdisplay
    if _xdisplay is not None:
        return _xdisplay
    with _xdisplay_lock:
        if _xdisplay is None:
            try:
                from Xlib import display as Xdisplay
                _xdisplay = Xdisplay.Display(DISPLAY_ENV)
                print(f"[STREAMER] Xlib connected to {DISPLAY_ENV}")
            except Exception as ex:
                print(f"[STREAMER] Xlib connect failed: {ex}")
        return _xdisplay

# Browser key code → X11 keysym name
KEYSYM_MAP = {
    "ArrowUp": "Up", "ArrowDown": "Down", "ArrowLeft": "Left", "ArrowRight": "Right",
    "KeyW": "w", "KeyA": "a", "KeyS": "s", "KeyD": "d",
    "KeyH": "h", "KeyJ": "j", "KeyK": "k", "KeyL": "l",
    "KeyQ": "q", "KeyE": "e", "KeyR": "r", "KeyF": "f",
    "KeyG": "g", "KeyI": "i", "KeyM": "m", "KeyN": "n",
    "KeyO": "o", "KeyP": "p", "KeyT": "t", "KeyU": "u",
    "KeyV": "v", "KeyX": "x", "KeyY": "y", "KeyZ": "z",
    "KeyB": "b", "KeyC": "c",
    "Digit1": "1", "Digit2": "2", "Digit3": "3", "Digit4": "4",
    "Digit5": "5", "Digit6": "6", "Digit7": "7", "Digit8": "8",
    "Digit9": "9", "Digit0": "0",
    "Space": "space", "Enter": "Return", "Escape": "Escape",
    "Tab": "Tab", "Backspace": "BackSpace", "Delete": "Delete",
    "ShiftLeft": "Shift_L", "ShiftRight": "Shift_R",
    "ControlLeft": "Control_L", "ControlRight": "Control_R",
    "AltLeft": "Alt_L", "AltRight": "Alt_R",
    "F1": "F1", "F2": "F2", "F3": "F3", "F4": "F4",
    "F5": "F5", "F6": "F6", "F7": "F7", "F8": "F8",
    "F9": "F9", "F10": "F10", "F11": "F11", "F12": "F12",
}

def inject(evt: dict):
    """Inject keyboard/mouse events via X11 XTEST — works for any X11 game."""
    t = evt.get("t")
    dpy = get_xdisplay()
    if dpy is None:
        return
    try:
        from Xlib.ext import xtest
        from Xlib import X
        import Xlib.XK as XK

        if t == "key":
            code = evt.get("code", "")
            down = evt.get("down", False)
            sym_name = KEYSYM_MAP.get(code)
            if not sym_name:
                return
            keysym = XK.string_to_keysym(sym_name)
            if keysym == 0:
                return
            keycode = dpy.keysym_to_keycode(keysym)
            if keycode == 0:
                return
            event_type = X.KeyPress if down else X.KeyRelease
            xtest.fake_input(dpy, event_type, keycode)
            dpy.flush()

        elif t == "mouse":
            dx = int(evt.get("dx", 0))
            dy = int(evt.get("dy", 0))
            btn = evt.get("btn")
            down = evt.get("down")
            if dx or dy:
                xtest.fake_input(dpy, X.MotionNotify, detail=True, x=dx, y=dy)
                dpy.flush()
            if btn is not None and down is not None:
                btn_map = {"left": 1, "middle": 2, "right": 3}
                btn_num = btn_map.get(btn)
                if btn_num:
                    event_type = X.ButtonPress if down else X.ButtonRelease
                    xtest.fake_input(dpy, event_type, btn_num)
                    dpy.flush()

        elif t == "wheel":
            dy = evt.get("dy", 0)
            btn = 5 if dy > 0 else 4
            xtest.fake_input(dpy, X.ButtonPress, btn)
            xtest.fake_input(dpy, X.ButtonRelease, btn)
            dpy.flush()

    except Exception as ex:
        print(f"[STREAMER] inject error: {ex}")


class WebRTCStreamer:
    def __init__(self, ws, loop):
        self.ws = ws
        self.loop = loop
        self.pipe = None
        self.webrtc = None
        self.data_channel = None

    def build_pipeline(self):
        pipeline_str = f"""
        webrtcbin name=webrtc bundle-policy=max-bundle
        ximagesrc display-name={DISPLAY_ENV} use-damage=false !
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
        self.webrtc = self.pipe.get_by_name("webrtc")

        stun_server = "stun://stun.l.google.com:19302"
        print(f"[STREAMER] Configuring ICE servers...")
        self.webrtc.set_property("stun-server", stun_server)
        print("[STREAMER] ICE servers configured")

        print("[STREAMER] Connecting WebRTC signals...")
        self.webrtc.connect("on-negotiation-needed", self.on_negotiation_needed)
        self.webrtc.connect("on-ice-candidate", self.on_ice_candidate)
        self.webrtc.connect("on-data-channel", self.on_incoming_data_channel)
        print("[STREAMER] Signals connected")
        print("[STREAMER] Pipeline built successfully")

    def on_incoming_data_channel(self, webrtc, channel):
        print(f"[STREAMER] Incoming data channel: {channel}")
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

    def on_data_message(self, channel, msg_str):
        try:
            evt = json.loads(msg_str)
            inject(evt)
        except Exception as ex:
            print(f"[STREAMER] Data message error: {ex}")

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
            reply = promise.get_reply()
            offer = reply.get_value("offer")
            element.emit("set-local-description", offer, Gst.Promise.new())
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
        bus = self.pipe.get_bus()
        bus.add_signal_watch()
        bus.connect("message", self.on_bus_message)

        print("[STREAMER] Setting pipeline to PLAYING state...")
        ret = self.pipe.set_state(Gst.State.PLAYING)
        print(f"[STREAMER] Pipeline state change result: {ret}")

        if ret == Gst.StateChangeReturn.FAILURE:
            print("[STREAMER] ERROR: Pipeline failed to start!")
            msg = bus.timed_pop_filtered(Gst.CLOCK_TIME_NONE, Gst.MessageType.ERROR)
            if msg:
                err, debug = msg.parse_error()
                print(f"[STREAMER] Error: {err.message}")
                print(f"[STREAMER] Debug: {debug}")
        else:
            print("[STREAMER] Pipeline ready, waiting for browser connection...")
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

    def stop(self):
        """Cleanly tear down the GStreamer pipeline."""
        try:
            if self.pipe:
                self.pipe.set_state(Gst.State.NULL)
                self.pipe = None
                self.webrtc = None
                self.data_channel = None
                print("[STREAMER] Pipeline stopped and cleaned up")
        except Exception as ex:
            print(f"[STREAMER] Error stopping pipeline: {ex}")

    async def signaling_loop(self):
        """Handle signaling messages. Returns when the websocket closes."""
        async for msg in self.ws:
            m = json.loads(msg)
            t = m.get("type")

            if t == "join" and m.get("data") == "web":
                print("[STREAMER] Browser joined! Resetting pipeline for fresh session...")
                # Tear down old pipeline so WebRTC state is clean
                self.stop()
                await asyncio.sleep(0.5)
                # Rebuild and restart pipeline
                self.build_pipeline()
                self.start()
                continue

            if t == "answer":
                _, sdp_msg = GstSdp.SDPMessage.new_from_text(m["data"])
                sdp = GstWebRTC.WebRTCSessionDescription.new(
                    GstWebRTC.WebRTCSDPType.ANSWER, sdp_msg)
                self.webrtc.emit("set-remote-description", sdp, Gst.Promise.new())

            elif t == "offer":
                _, sdp_msg = GstSdp.SDPMessage.new_from_text(m["data"])
                sdp = GstWebRTC.WebRTCSessionDescription.new(
                    GstWebRTC.WebRTCSDPType.OFFER, sdp_msg)
                self.webrtc.emit("set-remote-description", sdp, Gst.Promise.new())
                promise = Gst.Promise.new_with_change_func(self.on_answer_created, self.webrtc, None)
                self.webrtc.emit("create-answer", None, promise)

            elif t == "ice":
                data = m["data"]
                mlineindex = data.get("sdpMLineIndex") or data.get("mlineindex", 0)
                candidate = data.get("candidate")
                if candidate:
                    self.webrtc.emit("add-ice-candidate", mlineindex, candidate)

            elif t == "leave":
                print("[STREAMER] Browser left the room")
                # Don't break — wait for next browser to join

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


async def run_session(loop):
    """Connect to signaling, build pipeline, run until disconnect, then clean up."""
    print("[STREAMER] Connecting to signaling server...")
    async with websockets.connect(SIGNALING_URL) as ws:
        print("[STREAMER] Connected to signaling server")
        await ws.send(json.dumps({"room": ROOM, "type": "join", "data": "game"}))

        s = WebRTCStreamer(ws, loop)
        print("[STREAMER] Building GStreamer pipeline...")
        s.build_pipeline()
        print("[STREAMER] Starting pipeline...")
        s.start()
        print("[STREAMER] Entering signaling loop...")
        try:
            await s.signaling_loop()
        except websockets.exceptions.ConnectionClosed as e:
            print(f"[STREAMER] Signaling connection closed: {e}")
        except Exception as e:
            print(f"[STREAMER] Signaling loop error: {e}")
            import traceback
            traceback.print_exc()
        finally:
            s.stop()


async def main():
    print("[STREAMER] Starting WebRTC streamer (auto-reconnect mode)...")
    loop = asyncio.get_event_loop()
    retry_delay = 2
    while True:
        try:
            await run_session(loop)
        except (OSError, websockets.exceptions.WebSocketException) as e:
            print(f"[STREAMER] Connection error: {e}")
        except Exception as e:
            print(f"[STREAMER] Unexpected error: {e}")
            import traceback
            traceback.print_exc()
        print(f"[STREAMER] Reconnecting in {retry_delay}s...")
        await asyncio.sleep(retry_delay)
        retry_delay = min(retry_delay * 2, 30)  # exponential backoff, max 30s


if __name__ == "__main__":
    asyncio.run(main())