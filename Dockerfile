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

# Create a non-root user for development
RUN useradd -m -s /bin/bash developer && \
    echo "developer ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/developer

# Create workspace directory
RUN mkdir -p /app
WORKDIR /app

# Set user for subsequent commands
USER developer

# Create a script to run tests with proper permissions
RUN echo '#!/bin/bash\n\
cd /app\n\
sudo make setup_dummy_iface\n\
if [ "$#" -eq 0 ]; then\n\
  sudo make test\n\
else\n\
  sudo make "$@"\n\
fi\n\
sudo make clean_iface\n\
' > /home/developer/run_tests.sh && \
chmod +x /home/developer/run_tests.sh

# Create a script for quick development cycle
RUN echo '#!/bin/bash\n\
cd /app\n\
while true; do\n\
  inotifywait -e modify,create,delete -r include/ src/ tests/\n\
  echo "Changes detected, rebuilding..."\n\
  sudo make compile_tests\n\
  echo "Build completed. Run /home/developer/run_tests.sh to test"\n\
done\n\
' > /home/developer/watch_and_build.sh && \
chmod +x /home/developer/watch_and_build.sh

# Install inotify-tools for the watch script
USER root
RUN apt-get update && apt-get install -y inotify-tools && \
    apt-get clean && rm -rf /var/lib/apt/lists/*
USER developer

# Add dev-specific helper script for testing specific components
RUN echo '#!/bin/bash\n\
echo "Available commands:"\n\
echo "  ./run_tests.sh               - Run all tests"\n\
echo "  ./run_tests.sh run_NAME      - Run specific test"\n\
echo "  ./run_tests.sh run_component_NAME - Run specific component test"\n\
echo "  ./watch_and_build.sh         - Watch for changes and rebuild"\n\
' > /home/developer/help.sh && \
chmod +x /home/developer/help.sh

CMD ["/bin/bash", "-c", "cd /app && /home/developer/help.sh && /bin/bash"]
