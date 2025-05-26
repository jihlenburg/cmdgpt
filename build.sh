#!/bin/bash

# Unified build script for cmdgpt
# Supports macOS and Linux

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}[BUILD]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Detect operating system
OS_TYPE=""
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS_TYPE="macOS"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS_TYPE="Linux"
else
    print_error "Unsupported operating system: $OSTYPE"
    exit 1
fi

print_status "Detected operating system: $OS_TYPE"

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to install dependencies on macOS
install_macos_deps() {
    print_status "Checking macOS dependencies..."
    
    # Check for Homebrew
    if ! command_exists brew; then
        print_status "Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi
    
    # Check for required tools
    local deps=("cmake" "git")
    for dep in "${deps[@]}"; do
        if ! command_exists "$dep"; then
            print_status "Installing $dep..."
            brew install "$dep"
        else
            print_status "$dep is already installed"
        fi
    done
    
    # Check for OpenSSL
    if ! brew list openssl >/dev/null 2>&1; then
        print_status "Installing OpenSSL..."
        brew install openssl
    else
        print_status "OpenSSL is already installed"
    fi
    
    # Check for Xcode Command Line Tools
    if ! xcode-select -p >/dev/null 2>&1; then
        print_status "Installing Xcode Command Line Tools..."
        xcode-select --install
        print_warning "Please complete the Xcode Command Line Tools installation and run this script again"
        exit 0
    fi
}

# Function to install dependencies on Linux
install_linux_deps() {
    print_status "Checking Linux dependencies..."
    
    # Detect package manager
    if command_exists apt-get; then
        # Debian/Ubuntu
        print_status "Detected Debian/Ubuntu system"
        local deps=("build-essential" "cmake" "git" "libssl-dev")
        
        print_status "Updating package lists..."
        sudo apt-get update
        
        for dep in "${deps[@]}"; do
            if ! dpkg -l | grep -q "^ii  $dep"; then
                print_status "Installing $dep..."
                sudo apt-get install -y "$dep"
            else
                print_status "$dep is already installed"
            fi
        done
        
    elif command_exists yum; then
        # RHEL/CentOS/Fedora
        print_status "Detected RHEL/CentOS/Fedora system"
        local deps=("gcc" "gcc-c++" "make" "cmake" "git" "openssl-devel")
        
        for dep in "${deps[@]}"; do
            if ! rpm -qa | grep -q "$dep"; then
                print_status "Installing $dep..."
                sudo yum install -y "$dep"
            else
                print_status "$dep is already installed"
            fi
        done
        
    elif command_exists pacman; then
        # Arch Linux
        print_status "Detected Arch Linux system"
        local deps=("base-devel" "cmake" "git" "openssl")
        
        for dep in "${deps[@]}"; do
            if ! pacman -Q "$dep" >/dev/null 2>&1; then
                print_status "Installing $dep..."
                sudo pacman -S --noconfirm "$dep"
            else
                print_status "$dep is already installed"
            fi
        done
        
    else
        print_error "Unsupported Linux distribution"
        print_error "Please install the following manually: build-essential cmake git libssl-dev"
        exit 1
    fi
}

# Install system dependencies
if [[ "$OS_TYPE" == "macOS" ]]; then
    install_macos_deps
elif [[ "$OS_TYPE" == "Linux" ]]; then
    install_linux_deps
fi

# Check CMake version
CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
CMAKE_MIN_VERSION="3.13"

version_ge() {
    [ "$(printf '%s\n' "$1" "$2" | sort -V | head -n1)" = "$2" ]
}

if ! version_ge "$CMAKE_VERSION" "$CMAKE_MIN_VERSION"; then
    print_error "CMake version $CMAKE_VERSION is too old. Minimum required: $CMAKE_MIN_VERSION"
    exit 1
fi

print_status "CMake version $CMAKE_VERSION is sufficient"

# Create build directory
BUILD_DIR="build"
if [ -d "$BUILD_DIR" ]; then
    print_warning "Build directory already exists. Cleaning..."
    rm -rf "$BUILD_DIR"
fi

print_status "Creating build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_status "Configuring project with CMake..."
if [[ "$OS_TYPE" == "macOS" ]]; then
    # macOS specific flags for OpenSSL
    OPENSSL_ROOT_DIR=$(brew --prefix openssl)
    cmake -DOPENSSL_ROOT_DIR="$OPENSSL_ROOT_DIR" ..
else
    # Linux
    cmake ..
fi

# Build the project
print_status "Building project..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)

# Run tests
print_status "Running tests..."
if ./cmdgpt_tests; then
    print_status "All tests passed!"
else
    print_error "Some tests failed!"
    exit 1
fi

# Print success message
echo ""
print_status "Build completed successfully!"
print_status "Executables created:"
echo "  - $(pwd)/cmdgpt"
echo "  - $(pwd)/cmdgpt_tests"
echo ""
print_status "To use cmdgpt, run:"
echo "  export OPENAI_API_KEY='your-api-key'"
echo "  ./build/cmdgpt 'Your prompt here'"
echo ""
print_status "Or try interactive mode:"
echo "  ./build/cmdgpt -i"
echo ""
print_status "For documentation:"
echo "  ./build_docs.sh"
echo ""
print_status "To install shell completions:"
echo "  ./scripts/install-completions.sh"