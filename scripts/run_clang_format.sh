#!/bin/bash
# Centralized clang-format runner for cmdgpt project
# This ensures consistent code formatting across all environments

set -e

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Parse command line arguments
CHECK_ONLY=false
VERBOSE=false
FIX=false

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --check     Check formatting without modifying files (default)"
    echo "  --fix       Fix formatting issues in-place"
    echo "  --verbose   Show verbose output"
    echo "  --help      Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --check)
            CHECK_ONLY=true
            shift
            ;;
        --fix)
            FIX=true
            CHECK_ONLY=false
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --help|-h)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

# Default to check-only if not specified
if [ "$CHECK_ONLY" = false ] && [ "$FIX" = false ]; then
    CHECK_ONLY=true
fi

cd "$PROJECT_ROOT"

# Find all C++ files (excluding build directory and dependencies)
FILES=$(find . -name "*.cpp" -o -name "*.h" | \
        grep -v "./build" | \
        grep -v "./.git" | \
        grep -v ".github" | \
        sort)

# Check for clang-format
if ! command -v clang-format >/dev/null 2>&1; then
    echo "Error: clang-format not found!"
    echo "Please install clang-format to check code formatting."
    exit 1
fi

# Determine clang-format version and use appropriate one for CI compatibility
CLANG_FORMAT_CMD="clang-format"
if command -v clang-format-14 >/dev/null 2>&1; then
    CLANG_FORMAT_CMD="clang-format-14"
fi

if [ "$VERBOSE" = true ]; then
    echo "Using: $($CLANG_FORMAT_CMD --version)"
    echo "Checking files:"
    echo "$FILES" | sed 's/^/  /'
    echo ""
fi

# Run clang-format
EXIT_CODE=0
if [ "$CHECK_ONLY" = true ]; then
    # Check mode
    for file in $FILES; do
        if [ "$VERBOSE" = true ]; then
            echo "Checking: $file"
        fi
        if ! $CLANG_FORMAT_CMD --style=file:"$SCRIPT_DIR/.clang-format" --dry-run --Werror "$file" 2>/dev/null; then
            echo "Formatting issue in: $file"
            EXIT_CODE=1
        fi
    done
    
    if [ $EXIT_CODE -ne 0 ]; then
        echo ""
        echo "Formatting issues found!"
        echo "Run '$0 --fix' to automatically fix them."
    elif [ "$VERBOSE" = true ]; then
        echo "All files properly formatted!"
    fi
else
    # Fix mode
    echo "Fixing formatting issues..."
    for file in $FILES; do
        if [ "$VERBOSE" = true ]; then
            echo "Formatting: $file"
        fi
        $CLANG_FORMAT_CMD --style=file:"$SCRIPT_DIR/.clang-format" -i "$file"
    done
    echo "Formatting complete!"
fi

exit $EXIT_CODE