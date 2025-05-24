#!/bin/bash
# Script to build documentation for cmdgpt

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "Building cmdgpt documentation..."

# Check if Doxygen is installed
if ! command -v doxygen &> /dev/null; then
    echo -e "${RED}Error: Doxygen is not installed.${NC}"
    echo ""
    echo "Please install Doxygen first:"
    echo "  macOS:         brew install doxygen"
    echo "  Ubuntu/Debian: sudo apt-get install doxygen"
    echo "  Fedora:        sudo dnf install doxygen"
    echo "  Windows:       Download from https://www.doxygen.nl/download.html"
    echo ""
    echo "Would you like to install Doxygen now? (macOS only) [y/N]"
    read -r response
    if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
        if command -v brew &> /dev/null; then
            echo "Installing Doxygen via Homebrew..."
            brew install doxygen
        else
            echo -e "${RED}Homebrew not found. Please install Doxygen manually.${NC}"
            exit 1
        fi
    else
        exit 1
    fi
fi

# Check if Graphviz is installed for call graphs
if ! command -v dot &> /dev/null; then
    echo -e "${YELLOW}Warning: Graphviz (dot) is not installed.${NC}"
    echo "Call graphs and other diagrams will not be generated."
    echo ""
    echo "To install Graphviz:"
    echo "  macOS:         brew install graphviz"
    echo "  Ubuntu/Debian: sudo apt-get install graphviz"
    echo "  Fedora:        sudo dnf install graphviz"
    echo ""
    echo "Would you like to install Graphviz now? (macOS only) [y/N]"
    read -r response
    if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
        if command -v brew &> /dev/null; then
            echo "Installing Graphviz via Homebrew..."
            brew install graphviz
        else
            echo -e "${YELLOW}Continuing without Graphviz...${NC}"
        fi
    fi
else
    echo -e "${GREEN}Graphviz found. Call graphs will be generated.${NC}"
fi

# Check if Doxyfile exists
if [ ! -f "Doxyfile" ]; then
    echo -e "${RED}Error: Doxyfile not found in current directory.${NC}"
    echo "Please run this script from the cmdgpt root directory."
    exit 1
fi

# Generate documentation
echo -e "${GREEN}Generating documentation...${NC}"
doxygen Doxyfile

if [ $? -eq 0 ]; then
    echo -e "${GREEN}Documentation generated successfully!${NC}"
    echo ""
    echo "View documentation:"
    echo "  Open: docs/html/index.html"
    
    # Try to open in default browser on macOS
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo ""
        echo "Opening documentation in browser..."
        open docs/html/index.html
    fi
else
    echo -e "${RED}Error: Documentation generation failed.${NC}"
    exit 1
fi