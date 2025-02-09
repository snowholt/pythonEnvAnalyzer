#!/bin/bash

# Check for dependencies
echo "Checking dependencies..."
if ! command -v meson &> /dev/null; then
    echo "Meson not found. Please install meson."
    exit 1
fi

deps=("ninja-build" "libgtk-4-dev" "libjson-glib-dev" "libgraphviz-dev" "libsqlite3-dev")
missing=()

for dep in "${deps[@]}"; do
    dpkg -l | grep -q "^ii  $dep" || missing+=("$dep")
done

if [ ${#missing[@]} -ne 0 ]; then
    echo "Missing dependencies: ${missing[*]}"
    echo "Installing missing dependencies..."
    sudo apt install -y "${missing[@]}"
fi

# Check build location
if [ ! -f "meson.build" ]; then
    echo "Error: meson.build not found in current directory"
    exit 1
fi

# Set up build directory at project root
echo "Setting up build directory..."
if [ ! -d "build" ]; then
    meson setup build
else
    meson setup --reconfigure build
fi

# Build the project
echo "Building project..."
ninja -C build
