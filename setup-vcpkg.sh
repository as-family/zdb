#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# ZDB a distributed, fault-tolerant database.
# Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

# Script to set up vcpkg for the zdb project

set -euo pipefail

echo "Setting up vcpkg for zdb project..."

if ! command -v jq >/dev/null 2>&1; then
    echo "jq is required to pin vcpkg to the baseline. Please install jq." >&2
    exit 1
fi

if [ ! -f "vcpkg.json" ]; then
    echo "Error: vcpkg.json not found in repo root." >&2
    exit 1
fi

BASELINE="$(jq -r '."builtin-baseline"' vcpkg.json)"
if [ -z "${BASELINE}" ] || [ "${BASELINE}" = "null" ]; then
    echo "Error: Could not read builtin-baseline from vcpkg.json." >&2
    exit 1
fi

# Ensure repo exists
if [ ! -d "vcpkg/.git" ]; then
    echo "Cloning vcpkg..."
    rm -rf vcpkg
    git clone https://github.com/microsoft/vcpkg.git vcpkg
fi

# Fetch and pin to baseline deterministically
git -C vcpkg fetch --all --tags --quiet
if ! git -C vcpkg rev-parse --verify "${BASELINE}^{commit}" >/dev/null 2>&1; then
    # Commit may not be present in a shallow clone; fetch it explicitly
    git -C vcpkg fetch origin "${BASELINE}" --quiet || {
        echo "Error: Unable to fetch baseline commit ${BASELINE}." >&2
        exit 1
    }
fi

git -C vcpkg checkout --force "${BASELINE}"
echo "Pinned vcpkg to baseline commit ${BASELINE}."

cd vcpkg

# Verify bootstrap script exists before running
if [ ! -f "./bootstrap-vcpkg.sh" ]; then
    echo "Error: bootstrap-vcpkg.sh not found in vcpkg directory"
    exit 1
fi

# Bootstrap vcpkg
echo "Bootstrapping vcpkg..."
./bootstrap-vcpkg.sh
