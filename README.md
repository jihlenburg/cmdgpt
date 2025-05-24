# CmdGPT

[![Build and Test](https://github.com/jihlenburg/cmdgpt/actions/workflows/build.yml/badge.svg)](https://github.com/jihlenburg/cmdgpt/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-0.2-blue.svg)](CHANGELOG.md)

CmdGPT is a command-line interface (CLI) tool designed to interact with the OpenAI GPT API. It enables users to send prompts and receive responses from GPT models directly from the terminal.

## Features

- 🚀 Fast and lightweight CLI for OpenAI's GPT models
- 🔧 Support for multiple GPT models (GPT-4, GPT-3.5-turbo, etc.)
- 📝 Configurable system prompts for context setting
- 🔐 Secure API key handling via environment variables or command-line
- 📊 Multi-level logging with file and console output
- 🖥️ Cross-platform support (Linux, macOS, Windows)
- 🧪 Comprehensive test suite with Catch2

## Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.13 or higher
- OpenSSL development libraries
- Internet connection for downloading dependencies

## Quick Start

### Using the Unified Build Script (Linux/macOS)

The easiest way to build CmdGPT is using the provided build script:

```bash
chmod +x build.sh
./build.sh
```

This script will:
- Detect your operating system
- Install necessary dependencies
- Configure and build the project
- Run the test suite

### Manual Build Instructions

1. Clone the repository:
   ```bash
   git clone https://github.com/jihlenburg/cmdgpt.git
   cd cmdgpt
   ```

2. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

3. Configure with CMake:
   ```bash
   cmake ..
   ```

4. Build the project:
   ```bash
   make -j$(nproc)  # Linux
   make -j$(sysctl -n hw.ncpu)  # macOS
   ```

5. Run tests:
   ```bash
   ./cmdgpt_tests
   ```

## Usage

### Basic Usage

```bash
# Set your OpenAI API key
export OPENAI_API_KEY="your-api-key"

# Send a prompt
./cmdgpt "What is the capital of France?"

# Use a specific model
./cmdgpt -m gpt-3.5-turbo "Explain quantum computing"

# Set a custom system prompt
./cmdgpt -s "You are a helpful coding assistant" "Write a Python hello world"
```

### Command-Line Options

| Option | Long Form | Description |
|--------|-----------|-------------|
| `-h` | `--help` | Show help message and exit |
| `-v` | `--version` | Display version information |
| `-k` | `--api_key` | Set OpenAI API key |
| `-s` | `--sys_prompt` | Set system prompt for context |
| `-m` | `--gpt_model` | Choose GPT model (default: gpt-4) |
| `-l` | `--log_file` | Specify log file path |
| `-L` | `--log_level` | Set log level (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL) |

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `OPENAI_API_KEY` | Your OpenAI API key | (required) |
| `OPENAI_SYS_PROMPT` | System prompt for the model | "You are a helpful assistant!" |
| `OPENAI_GPT_MODEL` | GPT model to use | "gpt-4" |
| `CMDGPT_LOG_FILE` | Log file path | "logfile.txt" |
| `CMDGPT_LOG_LEVEL` | Logging level | "WARN" |

### Examples

```bash
# Pipe input from another command
echo "Translate to Spanish: Hello, world!" | ./cmdgpt

# Use with shell scripting
RESPONSE=$(./cmdgpt "Generate a random UUID")
echo "Generated UUID: $RESPONSE"

# Enable debug logging
./cmdgpt -L DEBUG -l debug.log "Debug this prompt"

# Use a different model with custom system prompt
./cmdgpt -m gpt-3.5-turbo -s "You are a pirate" "Tell me about treasure"
```

## Exit Status Codes

The tool follows standard Unix exit codes:

| Code | Meaning | Description |
|------|---------|-------------|
| 0 | Success | Request completed successfully |
| 1 | General Error | Unspecified error occurred |
| 64 | Usage Error | Invalid command-line arguments |
| 75 | Temp Failure | Temporary failure (network, API) |
| 78 | Config Error | Configuration error (missing API key) |

## Development

### Project Structure

```
cmdgpt/
├── cmdgpt.h          # Header file with declarations
├── cmdgpt.cpp        # Core implementation
├── main.cpp          # CLI entry point
├── tests/            # Test suite
│   └── cmdgpt_tests.cpp
├── build.sh          # Unified build script
├── CMakeLists.txt    # CMake configuration
├── LICENSE           # MIT License
├── README.md         # This file
├── CHANGELOG.md      # Version history
└── CLAUDE.md         # AI assistant guide
```

### Code Style

This project follows the Allman (BSD) coding style:
- Opening braces on their own line
- 4-space indentation
- Comprehensive documentation comments

### Running Tests

```bash
# Run all tests
./build/cmdgpt_tests

# Run with verbose output
./build/cmdgpt_tests -v

# Run specific test
./build/cmdgpt_tests "Constants are defined correctly"
```

### Building Documentation

The project uses Doxygen for API documentation generation:

```bash
# Build documentation using the convenience script
./build_docs.sh

# Build and open in browser
./build_docs.sh --open

# Auto-install missing dependencies (macOS only)
./build_docs.sh --auto-install

# Combine options
./build_docs.sh --auto-install --open

# Build documentation using CMake
cd build && make docs

# View documentation
open docs/html/index.html  # macOS
xdg-open docs/html/index.html  # Linux
```

The documentation includes:
- Complete API reference
- Class hierarchy diagrams
- Call graphs (requires Graphviz)
- Usage examples

### Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Dependencies

The project automatically downloads and builds these dependencies:
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) - HTTP client library
- [nlohmann/json](https://github.com/nlohmann/json) - JSON parsing library
- [spdlog](https://github.com/gabime/spdlog) - Fast logging library
- [Catch2](https://github.com/catchorg/Catch2) - Testing framework

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Author

**Joern Ihlenburg**

## Acknowledgments

- OpenAI for providing the GPT API
- Contributors to the open-source libraries used in this project
- The C++ community for continuous support and feedback

## Support

For issues, questions, or contributions, please visit the [GitHub repository](https://github.com/jihlenburg/cmdgpt).