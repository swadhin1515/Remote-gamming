FROM ubuntu:22.04

# Avoid interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install all dependencies in one layer
RUN apt-get update && apt-get install -y \
    python3 python3-pip \
    xvfb x11-xserver-utils xauth \
    xterm \
    mesa-utils \
    gstreamer1.0-tools \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-nice \
    python3-gi gir1.2-gstreamer-1.0 gir1.2-gst-plugins-bad-1.0 gir1.2-nice-0.1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY *.whl ./
RUN pip3 install --no-cache-dir *.whl

COPY webrtc_streamer.py start.sh snake.sh ./
RUN chmod +x /app/start.sh /app/snake.sh

# Helps terminal tools like tput inside non-interactive contexts
ENV TERM=xterm-256color

CMD ["/app/start.sh"]