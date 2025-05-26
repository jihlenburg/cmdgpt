# Contributing to CmdGPT

Thank you for your interest in contributing to CmdGPT! This guide will help you get started with contributing to the project.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Documentation](#documentation)
- [Submitting Changes](#submitting-changes)
- [Review Process](#review-process)

## Code of Conduct

Please note that this project adheres to a code of conduct. By participating, you are expected to:
- Be respectful and inclusive
- Welcome newcomers and help them get started
- Focus on what is best for the community
- Show empathy towards other community members

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR-USERNAME/cmdgpt.git
   cd cmdgpt
   ```
3. **Add upstream remote**:
   ```bash
   git remote add upstream https://github.com/jihlenburg/cmdgpt.git
   ```
4. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

### Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.13 or higher
- Git
- OpenSSL development libraries

### Building the Project

```bash
# Use the unified build script
./build.sh

# Or manually
mkdir build && cd build
cmake ..
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu || echo 1)
```

### Setting Up Your Environment

1. **Install development tools**:
   ```bash
   # Code formatting
   # macOS: brew install clang-format
   # Linux: sudo apt-get install clang-format
   
   # Static analysis
   # macOS: brew install cppcheck
   # Linux: sudo apt-get install cppcheck
   ```

2. **Install shell completions** (optional):
   ```bash
   ./scripts/install-completions.sh
   ```

3. **Set up your editor** with:
   - C++ language server (clangd recommended)
   - EditorConfig support
   - Trailing whitespace removal

## Making Changes

### Before You Start

1. **Check existing issues** to avoid duplicate work
2. **Discuss major changes** by opening an issue first
3. **Keep changes focused** - one feature/fix per pull request

### Development Workflow

1. **Update your fork**:
   ```bash
   git fetch upstream
   git checkout main
   git merge upstream/main
   ```

2. **Make your changes**:
   - Write clean, readable code
   - Follow the coding standards
   - Add tests for new functionality
   - Update documentation as needed

3. **Test your changes**:
   ```bash
   # Run all quality checks
   ./check_code_quality.sh
   
   # Or individually:
   ./scripts/run_clang_format.sh --check
   ./scripts/run_cppcheck.sh
   ./build/cmdgpt_tests
   ```

4. **Commit your changes**:
   ```bash
   git add .
   git commit -m "feat: add new feature X
   
   - Detailed description of what changed
   - Why this change was needed
   - Any breaking changes or notes"
   ```

### Commit Message Format

Follow conventional commits format:
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation changes
- `style:` Code style changes (formatting, etc.)
- `refactor:` Code refactoring
- `test:` Test additions or changes
- `chore:` Build process or auxiliary tool changes

## Coding Standards

### C++ Style Guide

This project follows the Allman (BSD) style with these specifics:

```cpp
namespace cmdgpt
{

class ExampleClass
{
  public:
    ExampleClass() = default;
    
    void example_method(int param_name)
    {
        if (param_name > 0)
        {
            // 4 space indentation
            do_something();
        }
    }
    
  private:
    int member_variable_; // Trailing underscore
};

} // namespace cmdgpt
```

### Key Points

- **Brace style**: Allman (opening braces on new line)
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 100 characters maximum
- **Naming**:
  - Classes: `PascalCase`
  - Functions/variables: `snake_case`
  - Constants: `UPPER_SNAKE_CASE`
  - Members: `snake_case_` (trailing underscore)
- **Modern C++**: Use C++17 features appropriately
- **RAII**: Prefer smart pointers and RAII patterns
- **Documentation**: All public APIs must have Doxygen comments

### Automatic Formatting

Always run formatting before committing:
```bash
./scripts/run_clang_format.sh --fix
```

## Testing

### Writing Tests

- Add tests for all new functionality
- Use descriptive test names
- Follow the AAA pattern (Arrange, Act, Assert)
- Test edge cases and error conditions

Example:
```cpp
TEST_CASE("validate_api_key rejects empty keys", "[validation]")
{
    // Arrange
    std::string empty_key = "";
    
    // Act & Assert
    REQUIRE_THROWS_AS(
        cmdgpt::validate_api_key(empty_key),
        cmdgpt::ValidationException
    );
}
```

### Running Tests

```bash
# Run all tests
./build/cmdgpt_tests

# Run specific test
./build/cmdgpt_tests "[validation]"

# Run with verbose output
./build/cmdgpt_tests -v
```

## Documentation

### Code Documentation

- Use Doxygen format for all public APIs
- Include `@brief`, `@param`, `@return`, and `@throws`
- Add usage examples for complex features

```cpp
/**
 * @brief Validate API key format
 * 
 * Ensures the API key meets OpenAI's format requirements.
 * Keys must start with "sk-" and be non-empty.
 * 
 * @param api_key The API key to validate
 * @throws ValidationException if key is invalid
 * 
 * @code
 * cmdgpt::validate_api_key("sk-1234567890");
 * @endcode
 */
```

### User Documentation

- Update README.md for user-facing changes
- Add examples to the examples/ directory
- Update CHANGELOG.md following Keep a Changelog format
- Document new configuration options

### Building Documentation

```bash
./build_docs.sh --open
```

## Submitting Changes

1. **Ensure all tests pass**:
   ```bash
   ./check_code_quality.sh
   ```

2. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```

3. **Create a Pull Request**:
   - Go to the original repository on GitHub
   - Click "New Pull Request"
   - Select your fork and branch
   - Fill out the PR template with:
     - Description of changes
     - Related issue numbers
     - Testing performed
     - Breaking changes (if any)

### Pull Request Guidelines

- **One feature per PR** - Keep PRs focused and small
- **Include tests** - All new code should have tests
- **Update docs** - Include documentation updates
- **Pass CI** - All GitHub Actions checks must pass
- **Clean history** - Squash commits if needed

## Review Process

### What to Expect

1. **Automated checks** run immediately (build, tests, formatting)
2. **Code review** by maintainers within a few days
3. **Feedback** may be provided for changes
4. **Approval** once all feedback is addressed
5. **Merge** by a maintainer

### During Review

- Respond to feedback promptly
- Push additional commits to address comments
- Don't force-push during review (makes it hard to see changes)
- Ask questions if feedback is unclear

## Getting Help

- **Questions**: Open a discussion on GitHub
- **Bugs**: Open an issue with reproduction steps
- **Ideas**: Open an issue to discuss first
- **Chat**: Join our community discussions

## Recognition

Contributors will be:
- Added to the contributors list
- Mentioned in release notes
- Given credit in commit messages

Thank you for contributing to CmdGPT!