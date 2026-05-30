#!/bin/bash
set -e

echo "=========================================="
echo "WebRTC Game Streaming - Kubernetes Deployment"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[!]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

# Check if kubectl is available
if ! command -v kubectl &> /dev/null; then
    print_error "kubectl not found. Please install kubectl first."
    exit 1
fi

# Check if connected to cluster
if ! kubectl cluster-info &> /dev/null; then
    print_error "Not connected to a Kubernetes cluster. Please configure kubectl."
    exit 1
fi

print_status "Connected to Kubernetes cluster"
echo ""

# Step 1: Build Docker images
echo "Step 1: Building Docker images..."
echo "--------------------------------"

if [ "$1" == "--skip-build" ]; then
    print_warning "Skipping image build (--skip-build flag)"
else
    print_status "Building game image..."
    docker build -t game:latest -f ../game/Dockerfile ../game/
    
    print_status "Building signaling image..."
    docker build -t signaling:latest -f ../signaling/Dockerfile ../signaling/
    
    print_status "Building web image..."
    docker build -t web:latest -f ../web/Dockerfile ../web/
    
    print_status "Building operator image..."
    docker build -t game-operator:latest -f operator/Dockerfile operator/
fi

echo ""

# Step 2: Deploy CRD
echo "Step 2: Deploying Custom Resource Definition..."
echo "------------------------------------------------"
kubectl apply -f crds/gamesession-crd.yaml
print_status "CRD deployed"
echo ""

# Wait for CRD to be established
echo "Waiting for CRD to be established..."
kubectl wait --for condition=established --timeout=60s crd/gamesessions.gamestream.example.com
print_status "CRD is ready"
echo ""

# Step 3: Deploy core services
echo "Step 3: Deploying core services..."
echo "-----------------------------------"
kubectl apply -f manifests/signaling-deployment.yaml
print_status "Signaling server deployed"

kubectl apply -f manifests/web-deployment.yaml
print_status "Web server deployed"
echo ""

# Step 4: Deploy operator
echo "Step 4: Deploying operator..."
echo "------------------------------"
kubectl apply -f manifests/operator-deployment.yaml
print_status "Operator deployed"
echo ""

# Wait for deployments to be ready
echo "Waiting for deployments to be ready..."
echo "---------------------------------------"

kubectl wait --for=condition=available --timeout=120s deployment/signaling-server || print_warning "Signaling server not ready yet"
kubectl wait --for=condition=available --timeout=120s deployment/web-server || print_warning "Web server not ready yet"
kubectl wait --for=condition=available --timeout=120s deployment/game-operator || print_warning "Operator not ready yet"

echo ""
print_status "All deployments are ready!"
echo ""

# Display status
echo "=========================================="
echo "Deployment Status"
echo "=========================================="
echo ""

echo "Pods:"
kubectl get pods -l 'app in (signaling-server,web-server,game-operator)'
echo ""

echo "Services:"
kubectl get svc -l 'app in (signaling-server,web-server)'
echo ""

echo "Custom Resource Definition:"
kubectl get crd gamesessions.gamestream.example.com
echo ""

# Display access information
echo "=========================================="
echo "Access Information"
echo "=========================================="
echo ""

WEB_PORT=$(kubectl get svc web-service -o jsonpath='{.spec.ports[0].nodePort}' 2>/dev/null || echo "30085")
NODE_IP=$(kubectl get nodes -o jsonpath='{.items[0].status.addresses[?(@.type=="InternalIP")].address}' 2>/dev/null || echo "localhost")

echo "Web Interface:"
echo "  http://${NODE_IP}:${WEB_PORT}"
echo ""

echo "To create a game session:"
echo "  kubectl apply -f examples/example-session.yaml"
echo ""

echo "To list active sessions:"
echo "  kubectl get gamesessions"
echo ""

echo "To view operator logs:"
echo "  kubectl logs -f deployment/game-operator"
echo ""

print_status "Deployment complete!"
