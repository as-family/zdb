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
    # Read builtin-baseline from vcpkg.json and pin vcpkg to that commit
    if ! command -v jq &> /dev/null; then
        echo "jq is required to pin vcpkg to the baseline. Please install jq."
        exit 1
    fi
    BASELINE=$(jq -r '."builtin-baseline"' vcpkg.json)
    if [ -n "$BASELINE" ] && [ "$BASELINE" != "null" ]; then
        git -C vcpkg checkout "$BASELINE"
        echo "Pinned vcpkg to baseline commit $BASELINE."
    else
        echo "Error: Could not read builtin-baseline from vcpkg.json."
        exit 1
    fi
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
