#!/bin/bash

# Default prefix
PREFIX=${1:-${SIMGRID_PREFIX:-/usr/local}}

echo "Installing SimGrid to: $PREFIX"

# Install dependencies
apt update
apt install -y \
    build-essential \
    nlohmann-json3-dev \
    graphviz graphviz-dev libgraphviz-dev \
    hwloc libhwloc-dev \
    numactl libnuma-dev \
    cmake \
    zip \
    unzip \
    wget

# Download and build SimGrid
wget https://github.com/simgrid/simgrid/releases/download/v3.35/simgrid-3.35.tar.gz
tar xf simgrid-3.35.tar.gz
cd simgrid-3.35

cmake -DCMAKE_INSTALL_PREFIX="$PREFIX" .
make -j"$(nproc)"
make install

# Export environment variables if prefix is not /usr/local
if [[ "$PREFIX" != "/usr/local" ]]; then
  echo ""
  echo "To use the installed SimGrid, add the following lines to your shell config (e.g., ~/.bashrc):"
  echo "export SIMGRID_PREFIX=$PREFIX"
  echo "export PATH=\$SIMGRID_PREFIX/bin:\$PATH"
  echo "export PKG_CONFIG_PATH=\$SIMGRID_PREFIX/lib/pkgconfig:\$PKG_CONFIG_PATH"
  echo "export LIBRARY_PATH=\$SIMGRID_PREFIX/lib:\$LIBRARY_PATH"
  echo "export LD_LIBRARY_PATH=\$SIMGRID_PREFIX/lib:\$LD_LIBRARY_PATH"
  echo "export CPATH=\$SIMGRID_PREFIX/include:\$CPATH"
fi

