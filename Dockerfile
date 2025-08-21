FROM ubuntu:22.04

# system packages
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential make python3.11 python3-pip git \
        libgraphviz-dev && \
    rm -rf /var/lib/apt/lists/*

# project files
WORKDIR /workspace
COPY . .

# Python deps
RUN python3 -m pip install --no-cache-dir -r requirements.txt

# build C++ targets from sim/
WORKDIR /workspace/sim
RUN make clean && \
    cd datacenter && make clean && cd .. && \
    make -j"$(nproc)" && \
    cd datacenter && make -j"$(nproc)"

# runtime env
WORKDIR /workspace
ENV LD_LIBRARY_PATH=/workspace/sim/lib:$LD_LIBRARY_PATH
