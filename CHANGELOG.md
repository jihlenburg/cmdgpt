# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.6.0] - 2025-05-27

### Added
- Image and PDF file support for multimodal AI interactions
- Vision API integration for image analysis
- Base64 encoding/decoding utilities
- Command-line flags for file inputs (-I, --image)
- File type detection and validation
- Streaming support with cpp-httplib v0.20.1

### Changed
- Upgraded cpp-httplib from v0.14.3 to v0.20.1
- Enhanced HTTP client with native streaming capabilities
- Improved multipart form data handling

### Security
- Updated cpp-httplib includes security fixes (CVE-2025-46728)

## [0.5.1] - 2025-05-27

### Changed
- Reorganized project structure for better maintainability
  - Moved header file from `src/` to dedicated `include/` directory
  - Relocated `.clang-format` and `.clang-tidy` files to `scripts/` directory
  - Updated all build scripts and CMake configuration to reflect new structure
  - Fixed documentation inconsistencies and improved code documentation

### Fixed
- Documentation version inconsistencies across files
- Missing Doxygen documentation for some functions
- Duplicate parameter documentation warnings in Doxygen

## [0.5.0] - 2025-05-26

### Added
- Rate limiting system to prevent API overload
  - Token bucket algorithm with configurable rate and burst capacity
  - Default: 3 requests/second with burst of 5
  - Thread-safe implementation with condition variables
  - Prevents concurrent request timeouts
- Asynchronous logging for improved performance
  - Thread pool with queue size of 8192
  - Non-blocking I/O prevents delays on TRACE level
  - Flush only on errors for better performance
- Response caching system to avoid duplicate API calls
  - SHA256-based cache keys for secure and consistent hashing
  - 24-hour default cache expiration
  - Cache size limits (100MB, 1000 entries) to prevent disk exhaustion
  - `--no-cache` flag to bypass cache for current request
  - `--clear-cache` command to clear all cached responses
  - `--cache-stats` command to display cache statistics
- Custom API endpoint support
  - `--endpoint URL` flag to use alternative API endpoints
  - Support for local models and OpenAI-compatible services
  - Automatic URL parsing for server and path components
- Response history tracking
  - Automatic persistence to ~/.cmdgpt/history.json
  - `--history` command to show recent history (last 10 entries)
  - `--clear-history` command to clear all history
  - `--search-history QUERY` to search history by prompt content
  - Token usage and cache status tracking per entry
- Template system for reusable prompts
  - Built-in templates for common tasks (code review, explain, refactor, docs, fix-error, unit-test)
  - `--list-templates` to show available templates
  - `--template NAME [VARS]` to use templates with variable substitution
  - Custom user templates stored in ~/.cmdgpt/templates.json
  - Variable extraction and validation
- Token usage tracking infrastructure
  - `TokenUsage` struct for tracking prompt/completion tokens and costs
  - `--show-tokens` flag to display token usage after responses
  - Model-specific pricing calculations (Note: full integration pending)
- Security improvements
  - Cache directory with restricted permissions (700)
  - Path traversal protection in cache operations
  - Atomic file writes to prevent corruption
  - Input validation for cache keys and endpoints
  - API key security audit and improved handling

### Security
- Enhanced path traversal protection using canonical path resolution
- Fail-fast on insecure cache directory permissions
- Improved thread safety in rate limiter implementation
- Fixed potential path traversal vulnerability in cache file operations
- Added secure file permissions for cache directory and history
- Implemented cache size limits to prevent disk exhaustion attacks
- Added validation for cache key format and endpoint URLs
- Ensured API keys are never logged or stored in cache

### Changed
- Enhanced Config class with cache, token display, and endpoint settings
- Improved error handling with new SecurityException type
- Refactored API communication to support custom endpoints
- Updated help text with comprehensive feature documentation

### Fixed
- Concurrent request timeout issues
- TRACE level logging performance problems
- Config file processing delays
- Code quality issues identified by static analysis
- Variable shadowing in template display code
- Type conversion warnings in history loading

## [0.4.2] - 2025-05-26

### Added
- Enhanced combined stdin+prompt input handling documentation
- Additional examples for pipe integration

### Fixed
- Code formatting issues detected by clang-format
- Cppcheck informational messages suppression

### Changed
- Improved input handling documentation with clear usage examples
- Updated examples to demonstrate the new combined input mode

## [0.4.1] - 2025-05-26

### Added
- Combined stdin and command-line prompt support for powerful command chaining
  - Example: `git log | cmdgpt "summarize in 5 bullets"`
  - Stdin provides context, command-line argument provides instruction

### Fixed
- CI build failures due to shadow variable warnings
- Missing `streaming_mode` variable declaration in main.cpp
- String initialization consistency (changed from `std::string{}` to `std::string()`)

### Changed
- Centralized code quality tooling in scripts directory
- Improved `.gitignore` to exclude temporary files and build artifacts
- Enhanced input handling to support three modes: stdin-only, prompt-only, or combined

### Removed
- Unnecessary temporary files (BATCH1_SUMMARY.md, build 2/, etc.)

## [0.4.0] - 2025-05-26

### Added
- Streaming responses with `--stream` flag (simulated streaming for better UX)
- Automatic retry with exponential backoff for rate limits and network errors
- Context preservation and recovery after errors in interactive mode
- Enhanced shell integration:
  - Multi-line input support from pipes and files
  - Better detection of terminal vs pipe input
  - Shell completion scripts for bash and zsh
  - Installation script for shell completions
- `get_gpt_chat_response_with_retry()` functions with configurable retry count
- TOO_MANY_REQUESTS (429) HTTP status code handling

### Changed
- Interactive mode now uses retry-enabled API calls by default
- Improved error messages with recovery instructions
- Enhanced pipe handling with proper EOF detection

### Security
- Added buffer size limits for streaming responses
- Validated all inputs in streaming functions
- Secure retry logic that doesn't retry on authentication errors

## [0.3.1] - 2025-01-25

### Fixed
- Build errors with undefined `streaming_mode` variable
- GitHub Actions CI configuration for code quality checks
- Added proper suppressions for non-critical cppcheck warnings
- Applied consistent code formatting with clang-format

### Added
- `.cppcheck` configuration file for consistent static analysis
- `.clang-tidy` configuration file for additional code quality checks
- Development workflow documentation (`WORKFLOW.md`)

### Changed
- Suppressed style-only warnings in CI (useStlAlgorithm, unusedStructMember, knownConditionTrueFalse)
- These warnings are about coding preferences, not actual issues

## [0.3.0] - 2025-01-25

### Added
- Interactive REPL mode with conversation management (`-i`/`--interactive`)
- Configuration file support (`~/.cmdgptrc`)
- Multiple output formats: plain, JSON, Markdown, code extraction (`-f`/`--format`)
- Conversation history with save/load functionality in interactive mode
- Comprehensive Doxygen documentation with call graphs
- Documentation build script (`scripts/build_docs.sh`) with auto-install option
- Enhanced security features:
  - Input validation for API keys and prompts
  - Certificate verification for HTTPS connections
  - API key redaction in logs
  - Response size limits to prevent DoS
- Exception-based error handling with custom exception hierarchy
- Namespace organization (`cmdgpt::`)
- Modern C++ features (constexpr, string_view, RAII)
- Context management with automatic trimming for long conversations
- Comprehensive inline documentation and code comments

### Changed
- Refactored to library pattern with namespace encapsulation
- Modernized code to use C++17 features throughout
- Updated default model to `gpt-4-turbo-preview`
- Improved error messages with specific exception types
- Enhanced logging with better API key protection
- Reorganized code structure with clear section markers
- Updated build system to include documentation generation
- Improved test coverage for new features

### Fixed
- Potential security vulnerabilities in input handling
- Memory safety issues with modern C++ patterns
- Build warnings and deprecated function usage

### Security
- Added input validation for all user inputs
- Implemented secure API key handling with redaction
- Enabled SSL/TLS certificate verification
- Added protection against oversized responses

## [0.2] - 2024-01-24

### Added
- MIT license headers to all source files
- Comprehensive GitHub Actions CI/CD workflow for Linux, macOS, and Windows
- Unified build script (`build.sh`) supporting both macOS and Linux
- Code quality checks with cppcheck in CI pipeline
- Better test coverage with Catch2 test fixtures
- Input validation for command-line arguments
- Proper error handling with sysexits.h exit codes
- Documentation comments (Doxygen format) for all public functions
- CLAUDE.md file for Claude Code AI assistance

### Changed
- Code formatting to Allman (BSD) style throughout the project
- Improved error messages and logging
- Enhanced command-line argument parsing with bounds checking
- Better organization of constants in header file
- Updated test framework usage with proper logger initialization

### Fixed
- Memory safety issues in argument parsing
- Potential segmentation faults from missing argument validation
- Logger initialization in test environment

### Removed
- Platform-specific `build_osx.sh` script (replaced with unified `build.sh`)

## [0.1] - 2023-12-01

### Added
- Initial release
- Basic OpenAI GPT API integration
- Command-line interface
- Environment variable support
- Multi-sink logging (console and file)
- CMake build system
- Basic test suite with Catch2
- macOS build script