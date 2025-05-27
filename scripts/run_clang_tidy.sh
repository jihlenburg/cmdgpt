#!/bin/bash
# Centralized clang-tidy runner for cmdgpt project
# This ensures consistent static analysis across all environments

set -e

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Parse command line arguments
FIX=false
VERBOSE=false

usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --fix       Apply fixes automatically"
    echo "  --verbose   Show verbose output"
    echo "  --help      Show this help message"
    exit 0
}

while [[ $# -gt 0 ]]; do
    case $1 in
        --fix)
            FIX=true
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

cd "$PROJECT_ROOT"

# Check for clang-tidy
if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "Error: clang-tidy not found!"
    echo "Please install clang-tidy for static analysis."
    exit 1
fi

# Check for compile_commands.json
if [ ! -f "build/compile_commands.json" ]; then
    echo "Error: build/compile_commands.json not found!"
    echo "Please run cmake with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
    exit 1
fi

# Find all C++ files (excluding build directory and dependencies)
FILES=$(find . -name "*.cpp" -o -name "*.h" | \
        grep -v "./build" | \
        grep -v "./.git" | \
        sort)

if [ "$VERBOSE" = true ]; then
    echo "Using: $(clang-tidy --version)"
    echo "Config: $SCRIPT_DIR/.clang-tidy"
    echo "Checking files:"
    echo "$FILES" | sed 's/^/  /'
    echo ""
fi

# Build fix flag if requested
FIX_FLAG=""
if [ "$FIX" = true ]; then
    FIX_FLAG="--fix"
fi

# Run clang-tidy
EXIT_CODE=0
for file in $FILES; do
    if [ "$VERBOSE" = true ]; then
        echo "Checking: $file"
    fi
    
    if ! clang-tidy --config-file="$SCRIPT_DIR/.clang-tidy" \
                    -p build \
                    $FIX_FLAG \
                    "$file" 2>&1 | grep -v "warnings generated" | grep -v "Suppressed.*warnings"; then
        EXIT_CODE=1
    fi
done

if [ $EXIT_CODE -eq 0 ]; then
    echo "No clang-tidy issues found!"
else
    echo "clang-tidy found issues!"
    if [ "$FIX" = false ]; then
        echo "Run '$0 --fix' to attempt automatic fixes."
    fi
fi

exit $EXIT_CODE