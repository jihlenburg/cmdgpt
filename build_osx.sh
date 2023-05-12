#!/bin/bash

# This script is designed to simplify the setup and build process for the CmdGPT project.
# CmdGPT is a command-line interface (CLI) tool designed to interact with the OpenAI GPT API.
# The script will first check if Homebrew, CMake, Git, and Xcode Command Line Tools are installed on the system.
# If they are not, the script will install them. It will then build the project using CMake and Make,
# making it simple for users to setup their build environment and compile the binary.

echo "This script will check if Homebrew, cmake, git, and Xcode command line tools are installed. If they are not, it will install them. It will then build the project using cmake and make. Do you wish to proceed? (y/n)"
read answer
if [ "$answer" != "${answer#[Yy]}" ] ;then
    # Check for Homebrew, install if we don't have it
    if test ! $(which brew); then
        echo "Installing homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        case $SHELL in
            */zsh)
                echo "Do you want to add Homebrew to your PATH in .zprofile (default) or .zshrc? (p/r)"
                read path_answer
                if [ "$path_answer" = "r" ]; then
                    echo 'eval $(/opt/homebrew/bin/brew shellenv)' >> ~/.zshrc
                else
                    echo 'eval $(/opt/homebrew/bin/brew shellenv)' >> ~/.zprofile
                fi
                ;;
            */bash)
                echo "Do you want to add Homebrew to your PATH in .bash_profile (default) or .bashrc? (p/r)"
                read path_answer
                if [ "$path_answer" = "r" ]; then
                    echo 'eval $(/opt/homebrew/bin/brew shellenv)' >> ~/.bashrc
                else
                    echo 'eval $(/opt/homebrew/bin/brew shellenv)' >> ~/.bash_profile
                fi
                ;;
            *)
                echo "You're not using zsh or bash, so you need to add '/opt/homebrew/bin/brew' to your PATH environment variable yourself."
                ;;
        esac
        eval $(/opt/homebrew/bin/brew shellenv)
    fi

    # Check for CMake, install if we don't have it
    if test ! $(which cmake); then
        echo "Installing cmake..."
        brew install cmake
    fi

    # Check for Git, install if we don't have it
    if test ! $(which git); then
        echo "Installing git..."
        brew install git
    fi

    # Check for Xcode CLI, install if we don't have it
    if ! xcode-select --print-path &> /dev/null; then
        echo "Installing Xcode CLI..."
        xcode-select --install
    fi

    # Create and change into the build directory
    mkdir -p build
    cd build

    # Run cmake and make
    cmake ..
    make

    # Change back into the parent directory
    cd ..
else
    echo "You chose not to proceed. Exiting..."
    exit 1
fi
