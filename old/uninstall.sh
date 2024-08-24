#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

BIN_DIR="/usr/local/bin"
MAN_DIR="/usr/local/share/man/man1"
EXECUTABLE="vumz"
MAN_PAGE="vumz.1"

# Check if the executable exists before attempting to remove it
if [[ -f "$BIN_DIR/$EXECUTABLE" ]]; then
    sudo rm "$BIN_DIR/$EXECUTABLE"
    echo "Executable removed"
else
    echo "Vumz is not installed"
fi

if [[ -f "$MAN_DIR/$MAN_PAGE" ]]; then
    sudo rm "$MAN_DIR/$MAN_PAGE"
    echo "Man page removed"
else
    echo "Man page not found"
fi

echo "Vumz was successfully uninstalled"

