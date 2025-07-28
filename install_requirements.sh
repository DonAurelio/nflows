# Install dependencies
apt update
apt install -y \
    build-essential \
    nlohmann-json3-dev \
    libevent-dev libeigen3-dev \
    libboost-dev libboost-context-dev libboost-stacktrace-dev \
    graphviz graphviz-dev libgraphviz-dev \
    hwloc libhwloc-dev \
    numactl libnuma-dev \
    cmake \
    zip \
    unzip \
    wget
