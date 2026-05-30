FROM python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy session manager
COPY session-manager.py .

# Run as non-root user
RUN useradd -m -u 1000 sessionmgr && \
    chown -R sessionmgr:sessionmgr /app

USER sessionmgr

ENV PYTHONUNBUFFERED=1
ENV LISTEN_PORT=8080
ENV SIGNALING_WS_URL=ws://signaling-service:9000
ENV NAMESPACE=default

EXPOSE 8080

CMD ["python3", "session-manager.py"]
