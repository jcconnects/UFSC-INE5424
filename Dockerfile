# Dockerfile com GCC 14
FROM ubuntu:24.04

# Set noninteractive installation mode
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y software-properties-common curl gpg build-essential

# Install required packages
RUN apt-get update && apt-get install -y \
    make \
    iproute2 \
    sudo \
    git 

# Adiciona o repositório oficial com GCC 14
RUN add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y gcc-14 g++-14

# Define gcc e g++ como gcc-14 e g++-14
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100

# Create workspace directory
RUN mkdir -p /app
WORKDIR /app

CMD ["/bin/bash", "-c", "cd /app && /bin/bash"]
