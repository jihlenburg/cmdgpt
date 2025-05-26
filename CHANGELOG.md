# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
- Documentation build script (`build_docs.sh`) with auto-install option
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