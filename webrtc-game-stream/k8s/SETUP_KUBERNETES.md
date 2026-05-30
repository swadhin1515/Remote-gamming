# Kubernetes Cluster Setup Guide

## Problem

You're seeing this error:
```
The connection to the server localhost:8080 was refused - did you specify the right host or port?
```

This means you don't have a Kubernetes cluster running or kubectl is not configured.

## Solution Options

You need to set up a Kubernetes cluster first. Here are your options:

---

## Option 1: Minikube (Recommended for Local Testing)

Minikube runs a single-node Kubernetes cluster on your local machine.

### Install Minikube

```bash
# Download and install minikube
curl -LO https://storage.googleapis.com/minikube/releases/latest/minikube-linux-amd64
sudo install minikube-linux-amd64 /usr/local/bin/minikube

# Verify installation
minikube version
```

### Start Minikube

```bash
# Start with Docker driver (recommended)
minikube start --driver=docker --cpus=4 --memory=8192

# Or with KVM2 driver
# minikube start --driver=kvm2 --cpus=4 --memory=8192

# Verify cluster is running
kubectl cluster-info
kubectl get nodes
```

### Configure kubectl

```bash
# Minikube automatically configures kubectl
# Verify it's working
kubectl get pods -A
```

### Deploy the Operator

```bash
cd /root/webrtc-game-stream/k8s

# Build images and load into minikube
docker build -t game:latest -f ../game/Dockerfile ../game/
docker build -t signaling:latest -f ../signaling/Dockerfile ../signaling/
docker build -t web:latest -f ../web/Dockerfile ../web/
docker build -t game-operator:latest -f operator/Dockerfile operator/

# Load images into minikube
minikube image load game:latest
minikube image load signaling:latest
minikube image load web:latest
minikube image load game-operator:latest

# Deploy everything
./deploy.sh --skip-build

# Get the web service URL
minikube service web-service --url
```

---

## Option 2: Kind (Kubernetes in Docker)

Kind runs Kubernetes clusters using Docker containers as nodes.

### Install Kind

```bash
# Download and install kind
curl -Lo ./kind https://kind.sigs.k8s.io/dl/v0.20.0/kind-linux-amd64
chmod +x ./kind
sudo mv ./kind /usr/local/bin/kind

# Verify installation
kind version
```

### Create Cluster

```bash
# Create a cluster
kind create cluster --name game-streaming

# Verify cluster is running
kubectl cluster-info --context kind-game-streaming
kubectl get nodes
```

### Load Images into Kind

```bash
cd /root/webrtc-game-stream/k8s

# Build images
docker build -t game:latest -f ../game/Dockerfile ../game/
docker build -t signaling:latest -f ../signaling/Dockerfile ../signaling/
docker build -t web:latest -f ../web/Dockerfile ../web/
docker build -t game-operator:latest -f operator/Dockerfile operator/

# Load images into kind
kind load docker-image game:latest --name game-streaming
kind load docker-image signaling:latest --name game-streaming
kind load docker-image web:latest --name game-streaming
kind load docker-image game-operator:latest --name game-streaming

# Deploy everything
./deploy.sh --skip-build
```

### Access Services

```bash
# Port forward the web service
kubectl port-forward svc/web-service 8085:80

# Access at http://localhost:8085
```

---

## Option 3: Use Existing Kubernetes Cluster

If you have access to an existing Kubernetes cluster:

### Configure kubectl

```bash
# Copy kubeconfig from your cluster
# Usually located at ~/.kube/config

# Or set KUBECONFIG environment variable
export KUBECONFIG=/path/to/your/kubeconfig

# Verify connection
kubectl cluster-info
kubectl get nodes
```

### Push Images to Registry

```bash
# Tag images for your registry
docker tag game:latest your-registry.com/game:latest
docker tag signaling:latest your-registry.com/signaling:latest
docker tag web:latest your-registry.com/web:latest
docker tag game-operator:latest your-registry.com/game-operator:latest

# Push to registry
docker push your-registry.com/game:latest
docker push your-registry.com/signaling:latest
docker push your-registry.com/web:latest
docker push your-registry.com/game-operator:latest

# Update manifests to use your registry
# Edit k8s/manifests/*.yaml and k8s/operator/operator.py
# Change image references to your-registry.com/...
```

### Deploy

```bash
cd /root/webrtc-game-stream/k8s
./deploy.sh --skip-build
```

---

## Option 4: K3s (Lightweight Kubernetes)

K3s is a lightweight Kubernetes distribution perfect for edge, IoT, and development.

### Install K3s

```bash
# Install K3s
curl -sfL https://get.k3s.io | sh -

# Wait for node to be ready
sudo k3s kubectl get nodes

# Copy kubeconfig for kubectl
mkdir -p ~/.kube
sudo cp /etc/rancher/k3s/k3s.yaml ~/.kube/config
sudo chown $(id -u):$(id -g) ~/.kube/config

# Verify
kubectl get nodes
```

### Deploy the Operator

```bash
cd /root/webrtc-game-stream/k8s

# Build images (K3s uses local Docker images)
docker build -t game:latest -f ../game/Dockerfile ../game/
docker build -t signaling:latest -f ../signaling/Dockerfile ../signaling/
docker build -t web:latest -f ../web/Dockerfile ../web/
docker build -t game-operator:latest -f operator/Dockerfile operator/

# Import images to K3s
sudo k3s ctr images import game.tar
sudo k3s ctr images import signaling.tar
sudo k3s ctr images import web.tar
sudo k3s ctr images import game-operator.tar

# Or use imagePullPolicy: Never in manifests

# Deploy everything
./deploy.sh --skip-build
```

---

## Quick Start (Recommended: Minikube)

Here's the fastest way to get started:

```bash
# 1. Install Minikube
curl -LO https://storage.googleapis.com/minikube/releases/latest/minikube-linux-amd64
sudo install minikube-linux-amd64 /usr/local/bin/minikube

# 2. Start Minikube
minikube start --driver=docker --cpus=4 --memory=8192

# 3. Build and load images
cd /root/webrtc-game-stream
docker build -t game:latest -f game/Dockerfile game/
docker build -t signaling:latest -f signaling/Dockerfile signaling/
docker build -t web:latest -f web/Dockerfile web/
docker build -t game-operator:latest -f k8s/operator/Dockerfile k8s/operator/

minikube image load game:latest
minikube image load signaling:latest
minikube image load web:latest
minikube image load game-operator:latest

# 4. Deploy
cd k8s
./deploy.sh --skip-build

# 5. Access web interface
minikube service web-service --url

# 6. Create a game session
kubectl apply -f examples/example-session.yaml

# 7. Check status
kubectl get gamesessions
kubectl get pods
```

---

## Troubleshooting

### kubectl not found
```bash
# Install kubectl
curl -LO "https://dl.k8s.io/release/$(curl -L -s https://dl.k8s.io/release/stable.txt)/bin/linux/amd64/kubectl"
chmod +x kubectl
sudo mv kubectl /usr/local/bin/
```

### Docker not running
```bash
# Start Docker
sudo systemctl start docker
sudo systemctl enable docker
```

### Minikube won't start
```bash
# Delete and recreate
minikube delete
minikube start --driver=docker --cpus=4 --memory=8192
```

### Images not found in cluster
```bash
# For Minikube
minikube image load your-image:tag

# For Kind
kind load docker-image your-image:tag --name cluster-name

# Or set imagePullPolicy: Never in manifests
```

---

## Next Steps

Once your cluster is running:

1. Deploy the operator: `cd k8s && ./deploy.sh`
2. Create sessions: `kubectl apply -f examples/example-session.yaml`
3. Access web UI: Use the service URL from your cluster
4. Monitor: `kubectl get gamesessions` and `kubectl logs -f deployment/game-operator`

## Support

- Minikube docs: https://minikube.sigs.k8s.io/docs/
- Kind docs: https://kind.sigs.k8s.io/
- K3s docs: https://k3s.io/
- kubectl docs: https://kubernetes.io/docs/reference/kubectl/
