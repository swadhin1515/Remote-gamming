#!/usr/bin/env python3
"""
Kubernetes Operator for WebRTC Game Streaming Sessions
Uses Kopf framework to manage GameSession custom resources
"""

import kopf
import kubernetes.client as k8s
from kubernetes.client.rest import ApiException
from datetime import datetime, timedelta
import logging
import os
import asyncio

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Operator configuration
NAMESPACE = os.getenv('OPERATOR_NAMESPACE', 'default')
GAME_IMAGE = os.getenv('GAME_IMAGE', 'game:latest')
SIGNALING_SERVICE = os.getenv('SIGNALING_SERVICE', 'signaling-service:9000')


def create_game_pod(name: str, namespace: str, spec: dict) -> dict:
    """
    Create a pod definition for a game session
    """
    username = spec.get('username')
    room = spec.get('room')
    game_name = spec.get('gameName', 'snake.sh')
    capture_mode = spec.get('captureMode', 'container')
    signaling_url = spec.get('signalingUrl', f'ws://{SIGNALING_SERVICE}')
    
    # Get resource limits/requests
    resources_spec = spec.get('resources', {})
    limits = resources_spec.get('limits', {'cpu': '2.0', 'memory': '4Gi'})
    requests = resources_spec.get('requests', {'cpu': '1.0', 'memory': '2Gi'})
    
    pod = {
        'apiVersion': 'v1',
        'kind': 'Pod',
        'metadata': {
            'name': f'game-{name}',
            'namespace': namespace,
            'labels': {
                'app': 'game-session',
                'session': name,
                'username': username,
                'room': room,
                'managed-by': 'game-operator'
            },
            'annotations': {
                'gamestream.example.com/session': name,
                'gamestream.example.com/username': username
            }
        },
        'spec': {
            'restartPolicy': 'Never',
            'containers': [{
                'name': 'game',
                'image': GAME_IMAGE,
                'imagePullPolicy': 'IfNotPresent',
                'env': [
                    {'name': 'SIGNALING_URL', 'value': signaling_url},
                    {'name': 'ROOM', 'value': room},
                    {'name': 'DISPLAY', 'value': ':99'},
                    {'name': 'GAME_CMD', 'value': 'xterm -maximized -e /app/game_launcher.sh'},
                    {'name': 'GAME_NAME', 'value': game_name},
                    {'name': 'CAPTURE_MODE', 'value': capture_mode},
                    {'name': 'USERNAME', 'value': username}
                ],
                'resources': {
                    'limits': limits,
                    'requests': requests
                },
                'securityContext': {
                    'privileged': True
                },
                'volumeMounts': [
                    {
                        'name': 'games',
                        'mountPath': '/games',
                        'readOnly': True
                    },
                    {
                        'name': 'dev-uinput',
                        'mountPath': '/dev/uinput'
                    }
                ]
            }],
            'volumes': [
                {
                    'name': 'games',
                    'hostPath': {
                        'path': '/opt/games',
                        'type': 'DirectoryOrCreate'
                    }
                },
                {
                    'name': 'dev-uinput',
                    'hostPath': {
                        'path': '/dev/uinput',
                        'type': 'CharDevice'
                    }
                }
            ]
        }
    }
    
    return pod


def create_game_service(name: str, namespace: str, spec: dict) -> dict:
    """
    Create a service definition for a game session (optional, for direct access)
    """
    room = spec.get('room')
    
    service = {
        'apiVersion': 'v1',
        'kind': 'Service',
        'metadata': {
            'name': f'game-{name}',
            'namespace': namespace,
            'labels': {
                'app': 'game-session',
                'session': name,
                'managed-by': 'game-operator'
            }
        },
        'spec': {
            'selector': {
                'session': name
            },
            'ports': [
                {
                    'name': 'webrtc',
                    'port': 8080,
                    'targetPort': 8080,
                    'protocol': 'TCP'
                }
            ],
            'type': 'ClusterIP'
        }
    }
    
    return service


@kopf.on.create('gamestream.example.com', 'v1', 'gamesessions')
def create_session(spec, name, namespace, logger, **kwargs):
    """
    Handler for GameSession creation
    Creates a pod and service for the game session
    """
    logger.info(f"Creating game session: {name} for user: {spec.get('username')}")
    
    api = k8s.CoreV1Api()
    
    try:
        # Create the game pod
        pod_manifest = create_game_pod(name, namespace, spec)
        pod = api.create_namespaced_pod(namespace=namespace, body=pod_manifest)
        logger.info(f"Created pod: {pod.metadata.name}")
        
        # Create the service (optional)
        service_manifest = create_game_service(name, namespace, spec)
        service = api.create_namespaced_service(namespace=namespace, body=service_manifest)
        logger.info(f"Created service: {service.metadata.name}")
        
        # Calculate expiry time if timeout is set
        session_timeout = spec.get('sessionTimeout', 30)
        start_time = datetime.utcnow()
        expiry_time = None
        if session_timeout > 0:
            expiry_time = start_time + timedelta(minutes=session_timeout)
        
        # Return status to be set on the GameSession resource
        return {
            'podName': pod.metadata.name,
            'serviceName': service.metadata.name,
            'phase': 'Running',
            'startTime': start_time.isoformat() + 'Z',
            'expiryTime': expiry_time.isoformat() + 'Z' if expiry_time else None,
            'message': f'Game session started for user {spec.get("username")}'
        }
        
    except ApiException as e:
        logger.error(f"Failed to create game session: {e}")
        return {
            'phase': 'Failed',
            'message': f'Failed to create resources: {e.reason}'
        }


@kopf.on.delete('gamestream.example.com', 'v1', 'gamesessions')
def delete_session(spec, name, namespace, logger, **kwargs):
    """
    Handler for GameSession deletion
    Cleans up pod and service
    """
    logger.info(f"Deleting game session: {name}")
    
    api = k8s.CoreV1Api()
    
    try:
        # Delete the pod
        pod_name = f'game-{name}'
        try:
            api.delete_namespaced_pod(
                name=pod_name,
                namespace=namespace,
                grace_period_seconds=30
            )
            logger.info(f"Deleted pod: {pod_name}")
        except ApiException as e:
            if e.status != 404:
                logger.warning(f"Failed to delete pod {pod_name}: {e}")
        
        # Delete the service
        service_name = f'game-{name}'
        try:
            api.delete_namespaced_service(
                name=service_name,
                namespace=namespace
            )
            logger.info(f"Deleted service: {service_name}")
        except ApiException as e:
            if e.status != 404:
                logger.warning(f"Failed to delete service {service_name}: {e}")
                
    except Exception as e:
        logger.error(f"Error during cleanup: {e}")


@kopf.on.update('gamestream.example.com', 'v1', 'gamesessions')
def update_session(spec, status, name, namespace, logger, **kwargs):
    """
    Handler for GameSession updates
    Currently just logs the update
    """
    logger.info(f"Game session updated: {name}")
    # You can add logic here to handle updates if needed


@kopf.timer('gamestream.example.com', 'v1', 'gamesessions', interval=60.0)
async def check_session_timeout(spec, status, name, namespace, logger, **kwargs):
    """
    Periodic timer to check if sessions have expired
    Runs every 60 seconds
    """
    session_timeout = spec.get('sessionTimeout', 30)
    
    # Skip if no timeout is set
    if session_timeout == 0:
        return
    
    # Check if session has expired
    expiry_time_str = status.get('expiryTime')
    if not expiry_time_str:
        return
    
    try:
        expiry_time = datetime.fromisoformat(expiry_time_str.replace('Z', '+00:00'))
        current_time = datetime.utcnow()
        
        if current_time >= expiry_time:
            logger.info(f"Session {name} has expired, deleting...")
            
            # Delete the GameSession resource (this will trigger the delete handler)
            api = k8s.CustomObjectsApi()
            try:
                api.delete_namespaced_custom_object(
                    group='gamestream.example.com',
                    version='v1',
                    namespace=namespace,
                    plural='gamesessions',
                    name=name
                )
                logger.info(f"Deleted expired session: {name}")
            except ApiException as e:
                if e.status != 404:
                    logger.error(f"Failed to delete expired session {name}: {e}")
                    
    except Exception as e:
        logger.error(f"Error checking session timeout for {name}: {e}")


@kopf.on.startup()
def configure(settings: kopf.OperatorSettings, **_):
    """
    Configure operator settings on startup
    """
    settings.persistence.finalizer = 'gamestream.example.com/finalizer'
    settings.persistence.progress_storage = kopf.AnnotationsProgressStorage()
    settings.persistence.diffbase_storage = kopf.AnnotationsDiffBaseStorage()
    
    logger.info("Game Session Operator started")
    logger.info(f"Namespace: {NAMESPACE}")
    logger.info(f"Game Image: {GAME_IMAGE}")
    logger.info(f"Signaling Service: {SIGNALING_SERVICE}")


if __name__ == '__main__':
    # Run the operator
    kopf.run()
