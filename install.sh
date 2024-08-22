#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

BUILD_DIR="build"
BIN_DIR="/usr/local/bin"
MAN_DIR="/usr/local/share/man/man1"
SRC_DIR="src"
EXECUTABLE="vumz"
MAN_PAGE="doc/vumz.1"

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"

# Compile the source code
gcc -Wall "$SRC_DIR/main.c" "$SRC_DIR/audio-cap.c" -o "$BUILD_DIR/$EXECUTABLE" -lm -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -D_REENTRANT -lpipewire-0.3 -lncursesw

# Ensure the build was successful
if [[ ! -f "$BUILD_DIR/$EXECUTABLE" ]]; then
    echo "Build failed, executable not found!"
    exit 1
fi

# Use sudo to install the executable
sudo install -m 755 "$BUILD_DIR/$EXECUTABLE" "$BIN_DIR/$EXECUTABLE"

# Install the man page
if [[ -f "$MAN_PAGE" ]]; then
    sudo install -m 644 "$MAN_PAGE" "$MAN_DIR/vumz.1"
    echo "Man page installed successfully!"
else
    echo "Man page not found"
fi

echo "Vumz was successfully installed in your pc :)"

