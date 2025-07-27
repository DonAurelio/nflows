#!/bin/bash

apt install -y \
    build-essential \
    nlohmann-json3-dev \
    graphviz graphviz-dev libgraphviz-dev \
    hwloc libhwloc-dev \
    numactl libnuma-dev \
    cmake \
    zip \
    unzip

wget https://github.com/simgrid/simgrid/releases/download/v3.35/simgrid-3.35.tar.gz
tar xf simgrid-3.35.tar.gz
cd simgrid-3.35
cmake -DCMAKE_INSTALL_PREFIX=/usr/local .
make -j 
make install
