#!/bin/bash
# Quick build script for development - uses existing build directory

set -e

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Change to project root directory
cd "${PROJECT_ROOT}"

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${GREEN}[BUILD]${NC} Quick build starting..."

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo -e "${GREEN}[BUILD]${NC} Creating build directory..."
    mkdir build
    cd build
    cmake ..
    cd ..
fi

# Build
echo -e "${GREEN}[BUILD]${NC} Building..."
cd build
make -j$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Run tests
echo -e "${GREEN}[BUILD]${NC} Running tests..."
if ./cmdgpt_tests; then
    echo -e "${GREEN}[BUILD]${NC} All tests passed!"
else
    echo -e "${RED}[BUILD]${NC} Some tests failed!"
    exit 1
fi

echo -e "${GREEN}[BUILD]${NC} Build complete!"
echo "  Executable: $(pwd)/cmdgpt"