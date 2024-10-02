#!/bin/bash

set -e # Exit immediately if a command exits with a non-zero status

# Function to check and install dependencies based on OS
install_dependencies() {
    case "$(uname -s)" in
        Linux)
            # Check for specific Linux distributions
            if [[ -f /etc/os-release ]]; then
                . /etc/os-release
                case "$ID" in
                    fedora)
                        echo "Detected Fedora."
                        if ! rpm -q pipewire pipewire-devel ncurses ncurses-devel &>/dev/null; then
                            echo "Installing dependencies..."
                            sudo dnf install -y pipewire pipewire-devel ncurses ncurses-devel
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    ubuntu | debian)
                        echo "Detected Ubuntu/Debian."
                        if ! dpkg -l | grep -qE "libncurses5-dev|libncursesw5-dev|libpipewire-0.3-dev"; then
                            echo "Installing dependencies..."
                            sudo apt install -y libncurses5-dev libncursesw5-dev libpipewire-0.3-dev
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    arch)
                        echo "Detected Arch Linux."
                        if ! pacman -Q pipewire ncurses &>/dev/null; then
                            echo "Installing dependencies..."
                            sudo pacman -S --noconfirm pipewire ncurses
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    void)
                        echo "Detected Void Linux."
                        if ! xbps-query -R pipewire-devel ncurses-devel &>/dev/null; then
                            echo "Installing dependencies..."
                            sudo xbps-install -S pipewire-devel ncurses-devel
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    gentoo)
                        echo "Detected Gentoo."
                        if ! equery list media-video/pipewire dev-libs/ncurses &>/dev/null; then
                            echo "Installing dependencies..."
                            sudo emerge -av media-video/pipewire dev-libs/ncurses
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    opensuse)
                        echo "Detected openSUSE."
                        if ! zypper se -i | grep -qE "pipewire|ncurses-devel"; then
                            echo "Installing dependencies..."
                            sudo zypper install -y pipewire ncurses-devel
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    alpine)
                        echo "Detected Alpine Linux."
                        if ! apk info | grep -qE "pipewire|ncurses-dev"; then
                            echo "Installing dependencies..."
                            sudo apk add pipewire ncurses-dev
                        else
                            echo "All required packages are already installed."
                        fi
                        ;;
                    *)
                        echo "Unsupported Linux distribution. Please install dependencies manually."
                        exit 1
                        ;;
                esac
            else
                echo "Could not detect the OS. Please install dependencies manually."
                exit 1
            fi
            ;;
        Darwin)
            echo "Detected macOS. Please install dependencies with Homebrew manually."
            exit 1
            ;;
        *)
            echo "Unsupported OS. Please install dependencies manually."
            exit 1
            ;;
    esac
}

# Compile source code
if [[ -d "./build" ]]; then
    echo "Deleting previous build..."
    sudo rm -r ./build
fi

install_dependencies

echo "Building source code..."
mkdir -p build
cd build
cmake ..
make

echo "Installing Vumz..."
sudo make install

echo "Vumz was successfully installed on your PC :)"

