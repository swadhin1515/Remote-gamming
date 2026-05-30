#!/usr/bin/env python3
"""
Session Manager Service
Automatically creates GameSession CRs when browser clients connect
Monitors WebSocket connections and creates isolated sessions per tab
"""

import asyncio
import json
import logging
import os
import uuid
from datetime import datetime
from typing import Dict, Set
import websockets
from kubernetes import client, config
from kubernetes.client.rest import ApiException

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s'
)
logger = logging.getLogger(__name__)

# Configuration
SIGNALING_WS_URL = os.getenv('SIGNALING_WS_URL', 'ws://signaling-service:9000')
LISTEN_PORT = int(os.getenv('LISTEN_PORT', '8080'))
NAMESPACE = os.getenv('NAMESPACE', 'default')
BASE_ROOM_PREFIX = os.getenv('BASE_ROOM_PREFIX', 'room-dockerstream1.fyre.ibm.com')

# Track active sessions per base room
active_sessions: Dict[str, Set[str]] = {}  # base_room -> set of session_ids
session_to_room: Dict[str, str] = {}  # session_id -> actual_room_name

# Load Kubernetes config
try:
    config.load_incluster_config()
    logger.info("Loaded in-cluster Kubernetes config")
except:
    config.load_kube_config()
    logger.info("Loaded local Kubernetes config")

k8s_api = client.CustomObjectsApi()
core_api = client.CoreV1Api()


def generate_session_id() -> str:
    """Generate unique session ID"""
    return str(uuid.uuid4())[:8]


def get_base_room(requested_room: str) -> str:
    """Extract base room name from requested room"""
    # Remove any existing session suffix
    if '-session-' in requested_room:
        return requested_room.split('-session-')[0]
    return requested_room


def create_session_room(base_room: str, session_id: str) -> str:
    """Create unique room name for this session"""
    return f"{base_room}-session-{session_id}"


async def create_game_session(username: str, room: str, session_id: str) -> bool:
    """Create GameSession CR and Pod in Kubernetes"""
    session_name = f"session-{session_id}"
    
    # GameSession CR
    game_session = {
        "apiVersion": "gamestream.example.com/v1",
        "kind": "GameSession",
        "metadata": {
            "name": session_name,
            "namespace": NAMESPACE,
            "labels": {
                "auto-created": "true",
                "session-id": session_id
            }
        },
        "spec": {
            "username": username,
            "room": room,
            "gameName": "snake.sh",
            "captureMode": "container",
            "signalingUrl": "ws://signaling-service:9000",
            "sessionTimeout": 30,
            "resources": {
                "limits": {
                    "cpu": "1.0",
                    "memory": "2Gi"
                },
                "requests": {
                    "cpu": "0.5",
                    "memory": "1Gi"
                }
            }
        }
    }
    
    # Pod definition
    pod = {
        "apiVersion": "v1",
        "kind": "Pod",
        "metadata": {
            "name": f"game-{session_name}",
            "namespace": NAMESPACE,
            "labels": {
                "app": "game-session",
                "session": session_name,
                "session-id": session_id,
                "auto-created": "true"
            }
        },
        "spec": {
            "restartPolicy": "Never",
            "containers": [{
                "name": "game",
                "image": "game:latest",
                "imagePullPolicy": "Never",
                "env": [
                    {"name": "SIGNALING_URL", "value": "ws://signaling-service:9000"},
                    {"name": "ROOM", "value": room},
                    {"name": "DISPLAY", "value": ":99"},
                    {"name": "GAME_CMD", "value": "xterm -maximized -e /app/game_launcher.sh"},
                    {"name": "GAME_NAME", "value": "snake.sh"},
                    {"name": "CAPTURE_MODE", "value": "container"},
                    {"name": "USERNAME", "value": username}
                ],
                "resources": {
                    "limits": {"cpu": "1.0", "memory": "2Gi"},
                    "requests": {"cpu": "0.5", "memory": "1Gi"}
                },
                "securityContext": {"privileged": True},
                "volumeMounts": [
                    {"name": "games", "mountPath": "/games", "readOnly": True},
                    {"name": "dev-uinput", "mountPath": "/dev/uinput"}
                ]
            }],
            "volumes": [
                {"name": "games", "hostPath": {"path": "/root/webrtc-game-stream/games", "type": "DirectoryOrCreate"}},
                {"name": "dev-uinput", "hostPath": {"path": "/dev/uinput", "type": "CharDevice"}}
            ]
        }
    }
    
    try:
        # Create GameSession CR
        k8s_api.create_namespaced_custom_object(
            group="gamestream.example.com",
            version="v1",
            namespace=NAMESPACE,
            plural="gamesessions",
            body=game_session
        )
        logger.info(f"Created GameSession CR: {session_name}")
        
        # Create Pod
        core_api.create_namespaced_pod(
            namespace=NAMESPACE,
            body=pod
        )
        logger.info(f"Created Pod: game-{session_name} for room: {room}")
        
        return True
        
    except ApiException as e:
        logger.error(f"Failed to create session {session_name}: {e}")
        return False


async def delete_game_session(session_id: str) -> bool:
    """Delete GameSession CR and Pod"""
    session_name = f"session-{session_id}"
    
    try:
        # Delete Pod
        try:
            core_api.delete_namespaced_pod(
                name=f"game-{session_name}",
                namespace=NAMESPACE,
                grace_period_seconds=5
            )
            logger.info(f"Deleted Pod: game-{session_name}")
        except ApiException as e:
            if e.status != 404:
                logger.warning(f"Failed to delete pod: {e}")
        
        # Delete GameSession CR
        try:
            k8s_api.delete_namespaced_custom_object(
                group="gamestream.example.com",
                version="v1",
                namespace=NAMESPACE,
                plural="gamesessions",
                name=session_name
            )
            logger.info(f"Deleted GameSession CR: {session_name}")
        except ApiException as e:
            if e.status != 404:
                logger.warning(f"Failed to delete GameSession: {e}")
        
        return True
        
    except Exception as e:
        logger.error(f"Error deleting session {session_id}: {e}")
        return False


async def handle_client(websocket, path):
    """Handle WebSocket connection from browser client"""
    session_id = None
    base_room = None
    actual_room = None
    
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                msg_type = data.get('type')
                requested_room = data.get('room')
                
                if msg_type == 'join' and requested_room:
                    # Extract base room and generate session
                    base_room = get_base_room(requested_room)
                    session_id = generate_session_id()
                    actual_room = create_session_room(base_room, session_id)
                    
                    # Track session
                    if base_room not in active_sessions:
                        active_sessions[base_room] = set()
                    active_sessions[base_room].add(session_id)
                    session_to_room[session_id] = actual_room
                    
                    logger.info(f"New client joined base room '{base_room}' -> assigned session '{session_id}' with room '{actual_room}'")
                    logger.info(f"Active sessions for '{base_room}': {len(active_sessions[base_room])}")
                    
                    # Create GameSession and Pod
                    username = data.get('username', f'user-{session_id}')
                    success = await create_game_session(username, actual_room, session_id)
                    
                    if success:
                        # Send back the actual room to use
                        response = {
                            'type': 'session_assigned',
                            'session_id': session_id,
                            'room': actual_room,
                            'base_room': base_room,
                            'message': f'Session created. Use room: {actual_room}'
                        }
                        await websocket.send(json.dumps(response))
                        logger.info(f"Sent session assignment to client: {actual_room}")
                    else:
                        error_response = {
                            'type': 'error',
                            'message': 'Failed to create game session'
                        }
                        await websocket.send(json.dumps(error_response))
                        
            except json.JSONDecodeError:
                logger.warning("Received invalid JSON")
            except Exception as e:
                logger.error(f"Error processing message: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client disconnected")
    finally:
        # Cleanup session on disconnect
        if session_id and base_room:
            logger.info(f"Cleaning up session {session_id} from base room {base_room}")
            
            # Remove from tracking
            if base_room in active_sessions:
                active_sessions[base_room].discard(session_id)
                if not active_sessions[base_room]:
                    del active_sessions[base_room]
            
            if session_id in session_to_room:
                del session_to_room[session_id]
            
            # Delete Kubernetes resources
            await delete_game_session(session_id)
            logger.info(f"Session {session_id} cleaned up")


async def main():
    """Start the session manager service"""
    logger.info(f"Starting Session Manager on port {LISTEN_PORT}")
    logger.info(f"Signaling server: {SIGNALING_WS_URL}")
    logger.info(f"Namespace: {NAMESPACE}")
    logger.info(f"Base room prefix: {BASE_ROOM_PREFIX}")
    
    async with websockets.serve(handle_client, "0.0.0.0", LISTEN_PORT):
        logger.info(f"Session Manager listening on ws://0.0.0.0:{LISTEN_PORT}")
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    asyncio.run(main())
