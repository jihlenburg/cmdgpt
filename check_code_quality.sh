#!/bin/bash
# Local code quality checks script
# Run this before committing to ensure CI will pass

set -e

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

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Check if we're in the project root
if [ ! -f "CMakeLists.txt" ] || [ ! -f "cmdgpt.cpp" ]; then
    print_error "This script must be run from the project root directory"
    exit 1
fi

# Check for required tools
print_status "Checking for required tools..."
for tool in cppcheck clang-format cmake make; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        print_error "Missing required tool: $tool"
        exit 1
    fi
done

# Configure CMake to download dependencies
print_status "Configuring CMake..."
if ! cmake -B build >/dev/null 2>&1; then
    print_error "CMake configuration failed"
    exit 1
fi

# Run cppcheck
print_status "Running static analysis with cppcheck..."
if ! cppcheck --enable=all \
    --suppress=missingIncludeSystem \
    --suppress=unmatchedSuppression \
    --suppress=missingInclude \
    --suppress=checkersReport \
    --suppress=unusedFunction \
    --suppress=unknownMacro \
    --inline-suppr \
    --std=c++17 \
    --language=c++ \
    -I. \
    cmdgpt.cpp cmdgpt.h main.cpp tests/cmdgpt_tests.cpp >/dev/null 2>&1; then
    print_error "Static analysis failed!"
    exit 1
fi
print_status "Static analysis passed!"

# Check code formatting
print_status "Checking code formatting..."
FORMAT_FAILED=false
for file in $(find . -name "*.cpp" -o -name "*.h" | grep -v build | grep -v ".github"); do
    if ! clang-format --dry-run --Werror "$file" >/dev/null 2>&1; then
        print_error "Formatting issue in: $file"
        FORMAT_FAILED=true
    fi
done

if [ "$FORMAT_FAILED" = true ]; then
    print_error "Code formatting issues found!"
    print_error "Run this command to fix them:"
    print_error "  find . -name '*.cpp' -o -name '*.h' | grep -v build | xargs clang-format -i"
    exit 1
fi
print_status "Code formatting check passed!"

# Build the project
print_status "Building project..."
if ! make -C build -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1) >/dev/null 2>&1; then
    print_error "Build failed!"
    exit 1
fi
print_status "Build succeeded!"

# Run tests
print_status "Running tests..."
if ! ./build/cmdgpt_tests >/dev/null 2>&1; then
    print_error "Some tests failed!"
    exit 1
fi
print_status "All tests passed!"

echo ""
print_status "All checks passed! Code is ready for commit."
echo ""
print_status "Summary:"
echo "  ✓ Static analysis (cppcheck)"
echo "  ✓ Code formatting (clang-format)"
echo "  ✓ Build successful"
echo "  ✓ All tests passing"