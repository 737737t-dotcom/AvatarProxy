#!/bin/bash
cd "$(dirname "$0")"

if [ ! -d "build" ]; then
    mkdir build
fi

cd build
cmake ..
make

if [ $? -eq 0 ]; then
    ./avatar_proxy
else
    echo "Build failed!"
    exit 1
fi
