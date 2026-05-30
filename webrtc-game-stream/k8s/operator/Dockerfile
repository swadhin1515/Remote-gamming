FROM python:3.11-slim

LABEL maintainer="WebRTC Game Stream Operator"
LABEL description="Kubernetes Operator for managing game streaming sessions"

# Set working directory
WORKDIR /operator

# Install system dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    gcc \
    && rm -rf /var/lib/apt/lists/*

# Copy requirements first for better caching
COPY requirements.txt .

# Install Python dependencies
RUN pip install --no-cache-dir -r requirements.txt

# Copy operator code
COPY operator.py .

# Create non-root user for security
RUN useradd -m -u 1000 operator && \
    chown -R operator:operator /operator

USER operator

# Set environment variables
ENV PYTHONUNBUFFERED=1
ENV OPERATOR_NAMESPACE=default

# Run the operator
CMD ["kopf", "run", "--standalone", "--verbose", "operator.py"]
