#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# ZDB a distributed, fault-tolerant database.
# Copyright (C) 2025 Ahmed Refaat Gadalla Mohamed
#
# This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.

# Run cppcheck with consistent options for local and CI usage.
# Usage: ./run-cppcheck.sh [BUILD_DIR] [OUTPUT_XML]
# - BUILD_DIR: optional path to CMake build directory (for compile_commands.json)
# - OUTPUT_XML: optional path for the XML report (default: <repo>/cppcheck-report.xml)

set -euo pipefail

# Resolve repo root as the directory of this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$SCRIPT_DIR"

BUILD_DIR="${1:-}"        # optional
OUTPUT_XML="${2:-${REPO_ROOT}/cppcheck-report.xml}"

# Ensure cppcheck is available
if ! command -v cppcheck >/dev/null 2>&1; then
  echo "cppcheck not found. Please install cppcheck and retry." >&2
  exit 0  # Soft exit to not break CI chains when optional
fi

# Prefer compile_commands.json if a build dir is provided and file exists
PROJECT_ARG=()
if [[ -n "${BUILD_DIR}" && -f "${BUILD_DIR}/compile_commands.json" ]]; then
  PROJECT_ARG=(--project="${BUILD_DIR}/compile_commands.json")
  echo "Using compile_commands.json from: ${BUILD_DIR}"
else
  echo "compile_commands.json not found; falling back to source scan (src/)."
fi

# Common cppcheck options (mirrors CI config)
COMMON_FLAGS=(
  --enable=warning,style,performance,portability
  --std=c++23
  --suppress=missingIncludeSystem
  --suppress=unusedFunction
  --suppress=unmatchedSuppression
  # Suppress version-check errors in generated protobuf files
  --suppress=preprocessorErrorDirective:*/proto/*.pb.h
  --suppress=preprocessorErrorDirective:*/proto/*.pb.cc
  --inline-suppr
  --quiet
  --xml --xml-version=2
  --max-configs=1
)

######################################
# Run cppcheck and collect the report
######################################
if [[ ${#PROJECT_ARG[@]} -gt 0 ]]; then
  # With compile_commands.json, cppcheck does not need explicit src path
  cppcheck "${COMMON_FLAGS[@]}" "${PROJECT_ARG[@]}" 2> "${OUTPUT_XML}" || true
else
  cppcheck "${COMMON_FLAGS[@]}" "${REPO_ROOT}/src/" 2> "${OUTPUT_XML}" || true
fi

echo "cppcheck report -> ${OUTPUT_XML}"

######################################
# Evaluate results: exit with error count
######################################
if [[ ! -s "${OUTPUT_XML}" ]]; then
  echo "cppcheck: no XML output produced at ${OUTPUT_XML}" >&2
  # Treat as tooling issue, not findings; exit 1 to be safe
  exit 1
fi

# Count only severity="error" entries
ERROR_COUNT=$(grep -c 'severity="error"' "${OUTPUT_XML}" || true)

if [[ "${ERROR_COUNT}" -gt 0 ]]; then
  echo "cppcheck: ${ERROR_COUNT} errors found" >&2
  exit 1
else
  echo "cppcheck: no errors found"
  exit 0
fi
