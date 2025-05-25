# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Unified build script (macOS and Linux)
./build.sh

# Standard CMake build
mkdir -p build && cd build
cmake ..
make

# Clean rebuild
rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu || echo 1)

# Build documentation
./build_docs.sh

# Build with linting and formatting checks
cd build
make && make format && make lint
```

## Test Commands

```bash
# Run all tests
./build/cmdgpt_tests

# Run tests with CTest
cd build && ctest -V
```

## Architecture Overview

This is a C++ command-line tool that interfaces with OpenAI's API. The codebase follows a library pattern with namespace encapsulation:

- **cmdgpt_lib**: Static library containing core functionality
  - `cmdgpt.cpp/h`: OpenAI API client, JSON handling, logging setup, conversation management
  - Uses cpp-httplib for HTTP requests, nlohmann/json for parsing, spdlog for logging
  - Implements modern C++ patterns with namespace `cmdgpt::`
  - Features: interactive mode, config files, output formatting, security validation
  
- **cmdgpt**: CLI executable that uses the library
  - `main.cpp`: Argument parsing, environment variable handling, user interface
  - Supports interactive REPL mode and multiple output formats
  
- **cmdgpt_tests**: Catch2-based test suite
  - Tests core functions: command parsing, API requests, JSON handling, validation

The tool supports command-line arguments, environment variables, and configuration files (`~/.cmdgptrc`). It implements multi-sink logging (console + file) with configurable log levels and comprehensive error handling through custom exceptions.

## Key Development Notes

- C++17 standard required
- OpenSSL dependency for HTTPS support
- Configuration precedence: defaults < config file < environment variables < command-line args
- Exit codes follow sysexits.h conventions (64=usage, 75=temp failure, 78=config error)
- Logs are written to `logfile.txt` by default
- Interactive mode commands: /help, /clear, /save, /load, /exit
- Output formats: plain, json, markdown, code
- Security features: input validation, certificate verification, API key redaction
- Documentation: Doxygen with call graphs (requires Graphviz)