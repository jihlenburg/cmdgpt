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
```

## Test Commands

```bash
# Run all tests
./build/cmdgpt_tests

# Run tests with CTest
cd build && ctest -V
```

## Architecture Overview

This is a C++ command-line tool that interfaces with OpenAI's API. The codebase follows a library pattern:

- **cmdgpt_lib**: Static library containing core functionality
  - `cmdgpt.cpp/h`: OpenAI API client, JSON handling, logging setup
  - Uses cpp-httplib for HTTP requests, nlohmann/json for parsing, spdlog for logging
  
- **cmdgpt**: CLI executable that uses the library
  - `main.cpp`: Argument parsing, environment variable handling, user interface
  
- **cmdgpt_tests**: Catch2-based test suite
  - Tests core functions: command parsing, API requests, JSON handling

The tool supports both command-line arguments and environment variables for configuration. It implements multi-sink logging (console + file) with configurable log levels.

## Key Development Notes

- C++17 standard required
- OpenSSL dependency for HTTPS support
- Environment variables take precedence over defaults but command-line args override both
- Exit codes follow sysexits.h conventions (64=usage, 75=temp failure, 78=config error)
- Logs are written to `logfile.txt` by default