#!/bin/bash
# Installation script for cmdgpt shell completions
#
# This script installs shell completion files for bash and zsh
# Usage: ./install-completions.sh [--user|--system]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default to user installation
INSTALL_MODE="user"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --user     Install for current user only (default)"
    echo "  --system   Install system-wide (requires sudo)"
    echo "  --help     Show this help message"
    echo ""
    echo "This script installs shell completion files for cmdgpt."
}

print_status() {
    echo -e "${GREEN}[INSTALL]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --user)
            INSTALL_MODE="user"
            shift
            ;;
        --system)
            INSTALL_MODE="system"
            shift
            ;;
        --help|-h)
            print_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Check if completion files exist
if [ ! -f "$SCRIPT_DIR/cmdgpt-completion.bash" ]; then
    print_error "Bash completion file not found: $SCRIPT_DIR/cmdgpt-completion.bash"
    exit 1
fi

if [ ! -f "$SCRIPT_DIR/cmdgpt-completion.zsh" ]; then
    print_error "Zsh completion file not found: $SCRIPT_DIR/cmdgpt-completion.zsh"
    exit 1
fi

# Detect current shell
CURRENT_SHELL=$(basename "$SHELL")
print_info "Detected shell: $CURRENT_SHELL"

# Install based on mode
if [ "$INSTALL_MODE" = "system" ]; then
    print_status "Installing system-wide completions (requires sudo)..."
    
    # Bash system-wide installation
    if command -v bash >/dev/null 2>&1; then
        BASH_COMPLETION_DIR="/etc/bash_completion.d"
        if [ -d "$BASH_COMPLETION_DIR" ]; then
            print_status "Installing bash completion to $BASH_COMPLETION_DIR..."
            sudo cp "$SCRIPT_DIR/cmdgpt-completion.bash" "$BASH_COMPLETION_DIR/cmdgpt"
            print_status "Bash completion installed system-wide"
        else
            print_warning "Bash completion directory not found: $BASH_COMPLETION_DIR"
        fi
    fi
    
    # Zsh system-wide installation
    if command -v zsh >/dev/null 2>&1; then
        # Try common zsh completion directories
        ZSH_DIRS=(
            "/usr/local/share/zsh/site-functions"
            "/usr/share/zsh/site-functions"
            "/usr/share/zsh/vendor-completions"
        )
        
        ZSH_INSTALLED=false
        for ZSH_DIR in "${ZSH_DIRS[@]}"; do
            if [ -d "$ZSH_DIR" ]; then
                print_status "Installing zsh completion to $ZSH_DIR..."
                sudo cp "$SCRIPT_DIR/cmdgpt-completion.zsh" "$ZSH_DIR/_cmdgpt"
                print_status "Zsh completion installed system-wide"
                ZSH_INSTALLED=true
                break
            fi
        done
        
        if [ "$ZSH_INSTALLED" = false ]; then
            print_warning "No suitable zsh completion directory found"
        fi
    fi
else
    # User-specific installation
    print_status "Installing user-specific completions..."
    
    # Bash user installation
    if [ "$CURRENT_SHELL" = "bash" ] || command -v bash >/dev/null 2>&1; then
        BASHRC="$HOME/.bashrc"
        if [ ! -f "$BASHRC" ]; then
            BASHRC="$HOME/.bash_profile"
        fi
        
        if [ -f "$BASHRC" ]; then
            # Check if already installed
            if grep -q "cmdgpt-completion.bash" "$BASHRC" 2>/dev/null; then
                print_info "Bash completion already installed in $BASHRC"
            else
                print_status "Adding bash completion to $BASHRC..."
                echo "" >> "$BASHRC"
                echo "# CmdGPT completion" >> "$BASHRC"
                echo "[ -f \"$SCRIPT_DIR/cmdgpt-completion.bash\" ] && source \"$SCRIPT_DIR/cmdgpt-completion.bash\"" >> "$BASHRC"
                print_status "Bash completion installed for user"
                print_info "Run 'source $BASHRC' or start a new shell to enable completions"
            fi
        else
            print_warning "Could not find .bashrc or .bash_profile"
        fi
    fi
    
    # Zsh user installation
    if [ "$CURRENT_SHELL" = "zsh" ] || command -v zsh >/dev/null 2>&1; then
        ZSHRC="$HOME/.zshrc"
        
        if [ -f "$ZSHRC" ]; then
            # Check if already installed
            if grep -q "cmdgpt.*fpath" "$ZSHRC" 2>/dev/null; then
                print_info "Zsh completion already installed in $ZSHRC"
            else
                print_status "Adding zsh completion to $ZSHRC..."
                
                # Create user completions directory if it doesn't exist
                USER_ZSH_COMPLETIONS="$HOME/.zsh/completions"
                mkdir -p "$USER_ZSH_COMPLETIONS"
                
                # Copy completion file
                cp "$SCRIPT_DIR/cmdgpt-completion.zsh" "$USER_ZSH_COMPLETIONS/_cmdgpt"
                
                # Add to fpath in .zshrc
                echo "" >> "$ZSHRC"
                echo "# CmdGPT completion" >> "$ZSHRC"
                echo "fpath=(\"$USER_ZSH_COMPLETIONS\" \$fpath)" >> "$ZSHRC"
                echo "autoload -Uz compinit && compinit" >> "$ZSHRC"
                
                print_status "Zsh completion installed for user"
                print_info "Run 'source $ZSHRC' or start a new shell to enable completions"
            fi
        else
            print_warning "Could not find .zshrc"
        fi
    fi
fi

print_status "Installation complete!"
print_info ""
print_info "To use completions:"
print_info "  - Bash: Type 'cmdgpt' and press TAB"
print_info "  - Zsh: Type 'cmdgpt' and press TAB"
print_info ""
print_info "Available completions include:"
print_info "  - Command options (--help, --version, etc.)"
print_info "  - Format options (plain, json, markdown, code)"
print_info "  - Model names (gpt-4, gpt-3.5-turbo)"
print_info "  - Log levels (debug, info, warn, error)"