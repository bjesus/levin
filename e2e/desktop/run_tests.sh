#!/usr/bin/env bash
#
# Desktop E2E Test Runner
# Runs all E2E tests for the desktop daemon
#
# Usage: ./run_tests.sh [test_file.bats]
#
# Requirements:
# - bats-core installed (https://github.com/bats-core/bats-core)
# - levin binary built (cmake --build build)
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Check if bats is installed
if ! command -v bats &> /dev/null; then
    echo "Error: bats-core is not installed"
    echo "Install with:"
    echo "  Ubuntu/Debian: sudo apt-get install bats"
    echo "  macOS: brew install bats-core"
    echo "  Or: npm install -g bats"
    exit 1
fi

# Check if levin is built
if [[ ! -x "${PROJECT_ROOT}/build/levin" ]]; then
    echo "Error: levin binary not found at ${PROJECT_ROOT}/build/levin"
    echo "Build with: cmake -B build && cmake --build build"
    exit 1
fi

# Export paths for tests
export LEVIN_BINARY="${PROJECT_ROOT}/build/levin"
export PROJECT_ROOT
export E2E_DIR="${SCRIPT_DIR}"

# Run specific test or all tests
if [[ $# -gt 0 ]]; then
    bats "$@"
else
    echo "Running all Desktop E2E tests..."
    echo "================================"
    bats "${SCRIPT_DIR}"/*.bats
fi
