FROM ubuntu:22.04

# Set noninteractive installation mode
ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update && apt-get install -y \
    g++ \
    make \
    iproute2 \
    sudo \
    git \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create workspace directory
RUN mkdir -p /app
WORKDIR /app

CMD ["/bin/bash", "-c", "cd /app && /bin/bash"]
