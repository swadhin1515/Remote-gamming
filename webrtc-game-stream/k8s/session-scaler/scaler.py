#!/usr/bin/env python3
"""
Session Scaler Service
Monitors browser connections and dynamically scales game deployment replicas
Each replica gets a unique room assignment
"""

import asyncio
import json
import logging
import os
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
LISTEN_PORT = int(os.getenv('LISTEN_PORT', '8080'))
NAMESPACE = os.getenv('NAMESPACE', 'default')
DEPLOYMENT_NAME = os.getenv('DEPLOYMENT_NAME', 'game-sessions')
BASE_ROOM_PREFIX = os.getenv('BASE_ROOM_PREFIX', 'room-dockerstream1.fyre.ibm.com')

# Track active connections per base room
active_connections: Dict[str, Set[str]] = {}  # base_room -> set of connection_ids
connection_to_room: Dict[str, str] = {}  # connection_id -> assigned_room

# Load Kubernetes config
try:
    config.load_incluster_config()
    logger.info("Loaded in-cluster Kubernetes config")
except:
    config.load_kube_config()
    logger.info("Loaded local Kubernetes config")

apps_api = client.AppsV1Api()


def get_current_replicas() -> int:
    """Get current number of replicas in deployment"""
    try:
        deployment = apps_api.read_namespaced_deployment(
            name=DEPLOYMENT_NAME,
            namespace=NAMESPACE
        )
        return deployment.spec.replicas or 0
    except ApiException as e:
        logger.error(f"Failed to get deployment replicas: {e}")
        return 0


def scale_deployment(replicas: int) -> bool:
    """Scale deployment to specified number of replicas"""
    try:
        # Get current deployment
        deployment = apps_api.read_namespaced_deployment(
            name=DEPLOYMENT_NAME,
            namespace=NAMESPACE
        )
        
        # Update replicas
        deployment.spec.replicas = replicas
        
        # Patch deployment
        apps_api.patch_namespaced_deployment(
            name=DEPLOYMENT_NAME,
            namespace=NAMESPACE,
            body=deployment
        )
        
        logger.info(f"Scaled deployment {DEPLOYMENT_NAME} to {replicas} replicas")
        return True
        
    except ApiException as e:
        logger.error(f"Failed to scale deployment: {e}")
        return False


def calculate_required_replicas(base_room: str) -> int:
    """Calculate how many replicas needed for a base room"""
    if base_room not in active_connections:
        return 0
    return len(active_connections[base_room])


def assign_room_for_connection(base_room: str, connection_id: str) -> str:
    """Assign a unique room for this connection"""
    # Get current replica index for this connection
    if base_room not in active_connections:
        active_connections[base_room] = set()
    
    active_connections[base_room].add(connection_id)
    replica_index = len(active_connections[base_room])
    
    # Create room name with replica index
    assigned_room = f"{base_room}-{replica_index}"
    connection_to_room[connection_id] = assigned_room
    
    return assigned_room


async def handle_client(websocket, path):
    """Handle WebSocket connection from browser client"""
    connection_id = id(websocket)
    base_room = None
    assigned_room = None
    
    try:
        async for message in websocket:
            try:
                data = json.loads(message)
                msg_type = data.get('type')
                requested_room = data.get('room')
                
                if msg_type == 'join' and requested_room:
                    base_room = requested_room
                    
                    # Assign room for this connection
                    assigned_room = assign_room_for_connection(base_room, connection_id)
                    
                    # Calculate required replicas
                    required_replicas = calculate_required_replicas(base_room)
                    
                    logger.info(f"New connection for base room '{base_room}' -> assigned '{assigned_room}'")
                    logger.info(f"Active connections for '{base_room}': {len(active_connections[base_room])}")
                    logger.info(f"Scaling deployment to {required_replicas} replicas")
                    
                    # Scale deployment
                    success = scale_deployment(required_replicas)
                    
                    if success:
                        # Send back the assigned room
                        response = {
                            'type': 'session_assigned',
                            'room': assigned_room,
                            'base_room': base_room,
                            'replica_index': len(active_connections[base_room]),
                            'total_replicas': required_replicas,
                            'message': f'Assigned to room: {assigned_room}'
                        }
                        await websocket.send(json.dumps(response))
                        logger.info(f"Sent room assignment: {assigned_room}")
                    else:
                        error_response = {
                            'type': 'error',
                            'message': 'Failed to scale deployment'
                        }
                        await websocket.send(json.dumps(error_response))
                        
            except json.JSONDecodeError:
                logger.warning("Received invalid JSON")
            except Exception as e:
                logger.error(f"Error processing message: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        logger.info(f"Client disconnected")
    finally:
        # Cleanup on disconnect
        if connection_id in connection_to_room:
            assigned_room = connection_to_room[connection_id]
            logger.info(f"Cleaning up connection {connection_id} from room {assigned_room}")
            
            # Remove from tracking
            if base_room and base_room in active_connections:
                active_connections[base_room].discard(connection_id)
                
                # Calculate new required replicas
                required_replicas = calculate_required_replicas(base_room)
                
                logger.info(f"Remaining connections for '{base_room}': {len(active_connections[base_room])}")
                logger.info(f"Scaling deployment to {required_replicas} replicas")
                
                # Scale down deployment
                scale_deployment(required_replicas)
                
                # Clean up empty base room
                if not active_connections[base_room]:
                    del active_connections[base_room]
            
            del connection_to_room[connection_id]


async def main():
    """Start the session scaler service"""
    logger.info(f"Starting Session Scaler on port {LISTEN_PORT}")
    logger.info(f"Deployment: {DEPLOYMENT_NAME}")
    logger.info(f"Namespace: {NAMESPACE}")
    logger.info(f"Base room prefix: {BASE_ROOM_PREFIX}")
    
    # Get initial replica count
    initial_replicas = get_current_replicas()
    logger.info(f"Current deployment replicas: {initial_replicas}")
    
    async with websockets.serve(handle_client, "0.0.0.0", LISTEN_PORT):
        logger.info(f"Session Scaler listening on ws://0.0.0.0:{LISTEN_PORT}")
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    asyncio.run(main())
