#!/bin/bash
# Centralized cppcheck runner for cmdgpt project
# This ensures consistent static analysis across all environments

set -e

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Define source files to check
SOURCE_FILES=(
    "src/cmdgpt.cpp"
    "include/cmdgpt.h"
    "src/main.cpp"
    "tests/cmdgpt_tests.cpp"
)

# Run cppcheck with consistent options
cd "$PROJECT_ROOT"

# Use --project option if .cppcheck file exists, otherwise use manual options
if [ -f ".cppcheck" ]; then
    echo "Running cppcheck with project configuration..."
    exec cppcheck --project=.cppcheck \
        --error-exitcode=1 \
        --inline-suppr \
        "${SOURCE_FILES[@]}"
else
    echo "Running cppcheck with command-line options..."
    # Run cppcheck and filter out informational messages
    cppcheck --enable=all \
        --suppressions-list="$SCRIPT_DIR/cppcheck_suppressions.txt" \
        --error-exitcode=1 \
        --inline-suppr \
        --std=c++17 \
        --language=c++ \
        -I. -Iinclude -Isrc \
        "${SOURCE_FILES[@]}" 2>&1 | grep -v "information: Limiting analysis" || true
    
    # Check if cppcheck found any real errors by running again and checking exit code
    cppcheck --enable=all \
        --suppressions-list="$SCRIPT_DIR/cppcheck_suppressions.txt" \
        --error-exitcode=1 \
        --inline-suppr \
        --std=c++17 \
        --language=c++ \
        --quiet \
        -I. -Iinclude -Isrc \
        "${SOURCE_FILES[@]}"
fi