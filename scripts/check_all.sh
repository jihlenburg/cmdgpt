#!/bin/bash
# Master code quality check script
# Runs all code quality checks in the correct order

set -e

# Get the directory where this script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[CHECK]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_section() {
    echo ""
    echo -e "${YELLOW}=== $1 ===${NC}"
}

cd "$PROJECT_ROOT"

# Track overall status
FAILED=false

# 1. Check code formatting
print_section "Code Formatting"
if "$SCRIPT_DIR/run_clang_format.sh" --check; then
    print_status "Code formatting check passed!"
else
    print_error "Code formatting issues found!"
    print_error "Run './scripts/run_clang_format.sh --fix' to fix them"
    FAILED=true
fi

# 2. Run static analysis
print_section "Static Analysis"
if "$SCRIPT_DIR/run_cppcheck.sh"; then
    print_status "Static analysis passed!"
else
    print_error "Static analysis failed!"
    FAILED=true
fi

# 3. Build the project
print_section "Build"
if [ -d "build" ]; then
    print_status "Using existing build directory..."
else
    print_status "Creating build directory..."
    mkdir build
fi

cd build
if ! cmake .. >/dev/null 2>&1; then
    print_error "CMake configuration failed!"
    FAILED=true
else
    print_status "CMake configuration succeeded!"
    
    if make -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4) >/dev/null 2>&1; then
        print_status "Build succeeded!"
    else
        print_error "Build failed!"
        FAILED=true
    fi
fi

# 4. Run tests
if [ -f "cmdgpt_tests" ]; then
    print_section "Unit Tests"
    if ./cmdgpt_tests; then
        print_status "All tests passed!"
    else
        print_error "Some tests failed!"
        FAILED=true
    fi
fi

# Summary
print_section "Summary"
if [ "$FAILED" = true ]; then
    print_error "Some checks failed!"
    exit 1
else
    print_status "All checks passed! ✅"
    echo ""
    echo "Your code is ready for commit!"
fi