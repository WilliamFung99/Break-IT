#!/bin/bash
set -e
mkdir -p build
cd build
cmake ..
make
echo "YAFsys build complete. Run with: ./fileserver admin_keyfile"
