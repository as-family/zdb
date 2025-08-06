#!/bin/bash

# Script to set up vcpkg for the zdb project

set -e

echo "Setting up vcpkg for zdb project..."

# Check if vcpkg directory already exists and has bootstrap script
if [ -d "vcpkg" ] && [ -f "vcpkg/bootstrap-vcpkg.sh" ]; then
    echo "vcpkg directory already exists with bootstrap script. Skipping clone."
else
    echo "Cloning vcpkg..."
    rm -rf vcpkg  # Remove incomplete vcpkg directory if it exists
    git clone https://github.com/Microsoft/vcpkg.git
fi

cd vcpkg

# Verify bootstrap script exists before running
if [ ! -f "./bootstrap-vcpkg.sh" ]; then
    echo "Error: bootstrap-vcpkg.sh not found in vcpkg directory"
    exit 1
fi

# Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
./bootstrap-vcpkg.sh
