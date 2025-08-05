#!/bin/bash

# Script to set up vcpkg for the zdb project

set -e

echo "Setting up vcpkg for zdb project..."

# Check if vcpkg directory already exists
if [ -d "vcpkg" ]; then
    echo "vcpkg directory already exists. Skipping clone."
else
    echo "Cloning vcpkg..."
    git clone https://github.com/Microsoft/vcpkg.git
fi

cd vcpkg

# Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
./bootstrap-vcpkg.sh
