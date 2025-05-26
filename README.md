# CmdGPT

[![Build and Test](https://github.com/jihlenburg/cmdgpt/actions/workflows/build.yml/badge.svg)](https://github.com/jihlenburg/cmdgpt/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Version](https://img.shields.io/badge/version-0.4.2-blue.svg)](CHANGELOG.md)

CmdGPT is a command-line interface (CLI) tool designed to interact with the OpenAI GPT API. It enables users to send prompts and receive responses from GPT models directly from the terminal.

## Features

- 🚀 Fast and lightweight CLI for OpenAI's GPT models
- 💬 Interactive REPL mode for conversational sessions
- 🔧 Support for multiple GPT models (GPT-4, GPT-3.5-turbo, etc.)
- 📝 Configurable system prompts for context setting
- ⚙️ Configuration file support (~/.cmdgptrc)
- 📁 Conversation history with save/load functionality
- 📤 Multiple output formats (plain, JSON, Markdown, code extraction)
- 🔐 Enhanced security with input validation and certificate verification
- 📊 Multi-level logging with file and console output
- 🖥️ Cross-platform support (Linux, macOS, Windows)
- 📚 Comprehensive Doxygen documentation with call graphs
- 🧪 Comprehensive test suite with Catch2
- 🌊 Streaming responses (simulated) for better user experience
- 🔄 Automatic retry with exponential backoff for rate limits
- 💾 Automatic conversation recovery after errors
- 🐚 Enhanced shell integration with completion scripts
- 📥 Multi-line input support from pipes and files
- 🔗 Combined stdin and prompt support for powerful command chaining

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

# Interactive mode
./cmdgpt -i
# Type '/help' for available commands

# Output formatting
./cmdgpt -f json "List 3 programming languages"
./cmdgpt -f markdown "Create a simple README template"
./cmdgpt -f code "Write a fibonacci function in Python"
```

### Example Scripts

The `examples/` directory contains ready-to-use scripts demonstrating various features:

```bash
# Basic usage examples
./examples/simple_query.sh

# Streaming responses
./examples/streaming_example.sh

# Different output formats
./examples/format_examples.sh

# Unix pipe integration
./examples/pipe_integration.sh

# Automated code review
./examples/code_reviewer.sh *.py
```

See [examples/README.md](examples/README.md) for detailed documentation.

### Command-Line Options

| Option | Long Form | Description |
|--------|-----------|-------------|
| `-h` | `--help` | Show help message and exit |
| `-v` | `--version` | Display version information |
| `-i` | `--interactive` | Start interactive REPL mode |
| | `--stream` | Enable streaming responses (simulated) |
| `-f` | `--format` | Output format: plain, json, markdown, code (default: plain) |
| `-k` | `--api_key` | Set OpenAI API key |
| `-s` | `--sys_prompt` | Set system prompt for context |
| `-m` | `--gpt_model` | Choose GPT model (default: gpt-4-turbo-preview) |
| `-l` | `--log_file` | Specify log file path |
| `-L` | `--log_level` | Set log level (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL) |

### Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `OPENAI_API_KEY` | Your OpenAI API key | (required) |
| `OPENAI_SYS_PROMPT` | System prompt for the model | "You are a helpful assistant!" |
| `OPENAI_GPT_MODEL` | GPT model to use | "gpt-4-turbo-preview" |
| `CMDGPT_LOG_FILE` | Log file path | "logfile.txt" |
| `CMDGPT_LOG_LEVEL` | Logging level | "WARN" |

### Examples

```bash
# Pipe input from another command
echo "Translate to Spanish: Hello, world!" | ./cmdgpt

# Multi-line input from a file
cat my_code.py | ./cmdgpt "Review this Python code"

# Combine piped input with a prompt (NEW in v0.4.1)
git log --oneline -10 | ./cmdgpt "summarize these commits in 3 bullets"
curl -s api.example.com/data.json | ./cmdgpt "extract all email addresses"
cat error.log | ./cmdgpt "find the root cause of these errors"

# Use with shell scripting
RESPONSE=$(./cmdgpt "Generate a random UUID")
echo "Generated UUID: $RESPONSE"

# Enable streaming for real-time output
./cmdgpt --stream "Write a short story about a robot"

# Enable debug logging
./cmdgpt -L DEBUG -l debug.log "Debug this prompt"

# Use a different model with custom system prompt
./cmdgpt -m gpt-3.5-turbo -s "You are a pirate" "Tell me about treasure"
```

### Configuration File

Create a `.cmdgptrc` file in your home directory to set default values:

```ini
# ~/.cmdgptrc
api_key=your-api-key-here
model=gpt-4-turbo-preview
system_prompt=You are a helpful coding assistant
log_level=INFO
log_file=/tmp/cmdgpt.log
```

### Interactive Mode

Interactive mode provides a REPL interface for continuous conversations:

```bash
./cmdgpt -i

# Available commands:
# /help     - Show available commands
# /clear    - Clear conversation history
# /save [file] - Save conversation to JSON file
# /load [file] - Load conversation from JSON file
# /exit     - Exit interactive mode
```

**Error Recovery**: If the API encounters an error during interactive mode, the conversation is automatically saved to `.cmdgpt_recovery.json`. You'll be prompted to restore it on the next session.

### Shell Completion

CmdGPT includes shell completion scripts for enhanced command-line experience:

#### Bash Completion
```bash
# Install for current session
source scripts/cmdgpt-completion.bash

# Install permanently (add to ~/.bashrc)
echo "source /path/to/cmdgpt/scripts/cmdgpt-completion.bash" >> ~/.bashrc
```

#### Zsh Completion
```bash
# Add to fpath (in ~/.zshrc)
fpath+=(/path/to/cmdgpt/scripts)

# Or copy to standard location
cp scripts/cmdgpt-completion.zsh /usr/local/share/zsh/site-functions/_cmdgpt
```

The completion scripts provide:
- Option name completion
- Format option values (plain, json, markdown, code)
- Model name suggestions
- Log level completions

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
│   ├── cmdgpt_tests.cpp
│   └── README.md     # Testing guide
├── scripts/          # Utility scripts
│   ├── cmdgpt-completion.bash    # Bash completion script
│   ├── cmdgpt-completion.zsh     # Zsh completion script
│   ├── install-completions.sh    # Completion installation script
│   └── README.md     # Scripts documentation
├── examples/         # Usage examples
│   ├── simple_query.sh           # Basic usage
│   ├── streaming_example.sh      # Streaming demo
│   ├── format_examples.sh        # Output formats
│   ├── pipe_integration.sh       # Unix pipe usage
│   ├── code_reviewer.sh          # Automated reviews
│   └── README.md     # Examples guide
├── build/            # Build artifacts (generated)
│   └── README.md     # Build directory info
├── docs/             # Documentation
│   ├── images/       # Documentation images
│   ├── mainpage.dox  # Documentation main page
│   └── README.md     # Documentation guide
├── build.sh          # Unified build script
├── CMakeLists.txt    # CMake configuration
├── LICENSE           # MIT License
├── README.md         # This file
├── CHANGELOG.md      # Version history
├── CLAUDE.md         # AI assistant guide
├── Doxyfile          # Doxygen configuration
└── build_docs.sh     # Documentation build script
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

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details on:

- Development setup
- Coding standards
- Testing requirements
- Submission process

Quick steps:
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes and test them
4. Commit your changes (`git commit -m 'feat: add amazing feature'`)
5. Push to the branch (`git push origin feature/amazing-feature`)
6. Open a Pull Request

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