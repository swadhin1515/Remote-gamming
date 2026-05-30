#!/bin/bash
set -e

echo "=========================================="
echo "Deploying Kubernetes Deployment Scaler"
echo "=========================================="
echo ""

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Check if running as root or with sudo
if [ "$EUID" -ne 0 ]; then 
    echo -e "${RED}Please run as root or with sudo${NC}"
    exit 1
fi

echo -e "${YELLOW}Step 1: Building and importing images...${NC}"

# Build and import web image
echo "Building web image..."
cd /root/webrtc-game-stream
docker build -t web:latest -f web/Dockerfile web/
echo "Importing web image to K3s..."
docker save web:latest | /usr/local/bin/k3s ctr images import -

# Build and import session scaler image
echo "Building session-scaler image..."
cd /root/webrtc-game-stream/k8s/session-scaler
docker build -t session-scaler:latest .
echo "Importing session-scaler image to K3s..."
docker save session-scaler:latest | /usr/local/bin/k3s ctr images import -

echo -e "${GREEN}✓ Images built and imported${NC}"
echo ""

echo -e "${YELLOW}Step 2: Deploying game deployment...${NC}"
/usr/local/bin/kubectl apply -f /root/webrtc-game-stream/k8s/manifests/game-deployment.yaml
echo -e "${GREEN}✓ Game deployment created (0 replicas)${NC}"
echo ""

echo -e "${YELLOW}Step 3: Deploying session scaler...${NC}"
/usr/local/bin/kubectl apply -f /root/webrtc-game-stream/k8s/manifests/session-scaler-deployment.yaml
echo -e "${GREEN}✓ Session scaler deployed${NC}"
echo ""

echo -e "${YELLOW}Step 4: Restarting web server...${NC}"
/usr/local/bin/kubectl rollout restart deployment web-server
/usr/local/bin/kubectl rollout status deployment web-server --timeout=60s
echo -e "${GREEN}✓ Web server restarted${NC}"
echo ""

echo -e "${YELLOW}Step 5: Waiting for session scaler to be ready...${NC}"
/usr/local/bin/kubectl rollout status deployment session-scaler --timeout=60s
echo -e "${GREEN}✓ Session scaler ready${NC}"
echo ""

echo "=========================================="
echo -e "${GREEN}Deployment Complete!${NC}"
echo "=========================================="
echo ""
echo "Access the application at:"
echo -e "${GREEN}http://dockerstream1.fyre.ibm.com:30085/index-scaler.html${NC}"
echo ""
echo "Monitoring commands:"
echo "  kubectl get deployment game-sessions -w"
echo "  kubectl logs -f deployment/session-scaler"
echo "  kubectl get pods -l app=game-session"
echo ""
echo "Test by opening multiple browser tabs!"
echo "Each tab will scale the deployment by 1 replica."
echo ""
