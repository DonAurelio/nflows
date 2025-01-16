sudo apt install -y \
    libboost-dev \
    libboost-context-dev libboost-stacktrace-dev \
    pybind11-dev python3-dev \
    libevent-dev \
    libeigen3-dev \
    nlohmann-json3-dev \
    graphviz graphviz-dev libgraphviz-dev \
    build-essential \
    libhwloc-dev \
    libnuma-dev \
    numactl \
    hwloc \
    cmake

wget https://github.com/simgrid/simgrid/releases/download/v3.35/simgrid-3.35.tar.gz
tar xf simgrid-3.35.tar.gz
cd simgrid-3.35
cmake -DCMAKE_INSTALL_PREFIX=/opt/simgrid .
make -j 
sudo make install
rm -rf simgrid-3.35 simgrid-3.35.tar.gz
