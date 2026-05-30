FROM mirror.gcr.io/library/python:3.11-slim

WORKDIR /app

# Install dependencies
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copy scaler
COPY scaler.py .

# Run as non-root user
RUN useradd -m -u 1000 scaler && \
    chown -R scaler:scaler /app

USER scaler

ENV PYTHONUNBUFFERED=1
ENV LISTEN_PORT=8080
ENV NAMESPACE=default
ENV DEPLOYMENT_NAME=game-sessions

EXPOSE 8080

CMD ["python3", "scaler.py"]
