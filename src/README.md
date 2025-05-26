# Source Code Directory

This directory contains the core source code for cmdgpt.

## Files

- **cmdgpt.h** - Header file with class declarations, function prototypes, and constants
- **cmdgpt.cpp** - Core implementation including:
  - OpenAI API client functionality
  - Response caching system
  - History tracking
  - Template management
  - JSON parsing and validation
  - Rate limiting
  - Logging setup
- **main.cpp** - Command-line interface implementation:
  - Argument parsing
  - Interactive REPL mode
  - Environment variable handling
  - User interface

## Architecture

The code follows a library pattern where `cmdgpt.cpp` is compiled into a static library (`cmdgpt_lib`) that is then linked by both the main executable and the test suite. All functionality is encapsulated in the `cmdgpt` namespace.

## Dependencies

- **cpp-httplib** - HTTP client library
- **nlohmann/json** - JSON parsing
- **spdlog** - Logging framework
- **OpenSSL** - HTTPS support