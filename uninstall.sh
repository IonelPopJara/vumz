#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

if [[ ! -d "./build" ]]; then
    echo "Regenerating build system..."
    mkdir -p build
    cd build
    cmake ..
    make
    echo "Uninstalling Vumz..."
    sudo make uninstall
else
    echo "Uninstalling Vumz..."
    cd build
    sudo make uninstall
fi

echo "Vumz was successfully removed from your pc :("

