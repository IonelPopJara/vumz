#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

# Compile source code

if [[ -d "./build" ]]; then
    echo "Deleting previous build..."
    sudo rm -r ./build
fi

echo "Building source code..."
mkdir -p build
cd build
cmake ..
make

echo "Installing Vumz..."
# Install
sudo make install

echo "Vumz was successfully installed in your pc :)"

