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
./scripts/build_docs.sh

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
  - `src/cmdgpt.cpp` and `src/cmdgpt.h`: OpenAI API client, JSON handling, logging setup, conversation management
  - Uses cpp-httplib for HTTP requests, nlohmann/json for parsing, spdlog for logging
  - Implements modern C++ patterns with namespace `cmdgpt::`
  - Features: interactive mode, config files, output formatting, security validation
  
- **cmdgpt**: CLI executable that uses the library
  - `src/main.cpp`: Argument parsing, environment variable handling, user interface
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
- Security features: input validation, certificate verification, API key redaction, path traversal protection, secure file permissions, canonical path resolution
- Documentation: Doxygen with call graphs (requires Graphviz)
- Streaming mode: --stream flag for simulated real-time output
- Auto-retry: Exponential backoff for rate limits (429) and server errors (5xx)
- Error recovery: Automatic conversation saving and restoration in interactive mode
- Shell integration: Completion scripts for bash/zsh, multi-line pipe input support
- Response caching: SHA256-based keys, 24-hour expiration, size limits (100MB/1000 entries)
- Custom endpoints: --endpoint flag for alternative API servers
- Response history: Persistent tracking with search, stored in ~/.cmdgpt/history.json
- Template system: Built-in and custom templates with {{variable}} substitution
- Token tracking: Usage statistics and cost estimation (full display pending)
- Rate limiting: Token bucket algorithm with 3 req/sec limit and burst capacity of 5
- Async logging: Thread pool for non-blocking I/O on TRACE level logging

## Coding Style

**Style**: Allman (BSD) with 4-space indentation

**Naming Conventions**:
- Classes/Structs: `PascalCase` (e.g., `Config`, `ApiException`)
- Functions/variables: `snake_case` (e.g., `get_gpt_chat_response`, `api_key`)
- Member variables: `snake_case_` with trailing underscore (e.g., `api_key_`, `messages_`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `MAX_PROMPT_LENGTH`, `DEFAULT_MODEL`)
- Namespaces: lowercase (e.g., `cmdgpt`)

**C++ Features**:
- Modern C++17 features: `constexpr`, `string_view`, structured bindings
- RAII principles with smart pointers
- Custom exception hierarchy for error handling
- `= default` and `= delete` for special member functions

**Documentation**: Doxygen format with `@brief`, `@param`, `@return`

**Formatting**: 
- 100-character line limit
- Pointers left-aligned (`char* ptr`)
- Opening braces on new line (Allman style)
- No single-line functions or if statements

## Development Guidelines

- Make sure the code is well documented
  - Use clear, concise Doxygen comments for all functions and classes
  - Explain the purpose, parameters, return values, and any side effects
  - Include examples where appropriate
  - Document non-obvious implementation details or design decisions

- Always consider the security of the code you generate

- Always make sure everything builds from scratch and that the code quality tests succeed before committing to git