# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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