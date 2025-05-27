# Scripts Directory

This directory contains utility scripts for the cmdgpt project, including code quality tools and shell completion support.

## Code Quality Scripts

### check_all.sh
Master script that runs all code quality checks in sequence:
- Code formatting verification (clang-format)
- Static analysis (cppcheck)
- Build verification
- Unit test execution

Usage:
```bash
./scripts/check_all.sh
```

### run_clang_format.sh
Checks or fixes code formatting according to project standards using the `.clang-format` configuration file in this directory:

```bash
./run_clang_format.sh --check    # Check formatting (default)
./run_clang_format.sh --fix      # Auto-fix formatting issues
./run_clang_format.sh --verbose  # Show detailed output
```

### run_cppcheck.sh
Runs static analysis on the codebase:
- Checks all C++ source files
- Uses suppression file for false positives
- Reports potential bugs, memory leaks, and code issues

```bash
./run_cppcheck.sh
```

### run_clang_tidy.sh
Runs clang-tidy static analysis using the `.clang-tidy` configuration file in this directory:

```bash
./run_clang_tidy.sh              # Run analysis
./run_clang_tidy.sh --fix        # Apply automatic fixes
./run_clang_tidy.sh --verbose    # Show detailed output
```

Note: Requires `compile_commands.json` in the build directory. Generate it with:
```bash
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
```

### Configuration Files

#### .clang-format
Defines code formatting rules for the project, including:
- Indentation style and width
- Brace placement (Allman style)
- Line length limits
- Pointer alignment
- Include sorting rules

#### .clang-tidy
Configures static analysis checks, including:
- Bug-prone code patterns
- Performance improvements
- Modernization suggestions
- Readability enhancements

#### cppcheck_suppressions.txt
Configuration file for cppcheck containing:
- Suppressed warnings that are false positives
- Style warnings that don't apply to this project
- Platform-specific suppressions

## Shell Completion Scripts

### cmdgpt-completion.bash
Bash completion script providing intelligent tab completion:
- Command-line option completion (`--help`, `--version`, etc.)
- Format value completion (`plain`, `json`, `markdown`, `code`)
- Model name suggestions (`gpt-4`, `gpt-3.5-turbo`)
- Log level completion (`trace`, `debug`, `info`, `warn`, `error`, `critical`)

Installation:
```bash
# For current session
source scripts/cmdgpt-completion.bash

# For permanent installation
echo "source /path/to/cmdgpt/scripts/cmdgpt-completion.bash" >> ~/.bashrc
```

### cmdgpt-completion.zsh
Zsh completion script with enhanced features:
- Full option descriptions in completion menu
- Grouped completions by category
- Context-aware value hints
- File path completion for log files

Installation:
```bash
# Add to fpath
fpath+=(/path/to/cmdgpt/scripts)

# Or copy to system location
cp scripts/cmdgpt-completion.zsh /usr/local/share/zsh/site-functions/_cmdgpt
```

### install-completions.sh
Automated installation script for shell completions:

```bash
# Install for current user (default)
./install-completions.sh

# Install system-wide (requires sudo)
./install-completions.sh --system

# Show help
./install-completions.sh --help
```

Features:
- Auto-detects current shell (bash/zsh)
- Checks for existing installations
- Creates necessary directories
- Updates shell configuration files
- Provides post-installation instructions
- Supports both user and system-wide installation

## Usage Examples

### Running Code Quality Checks
```bash
# Run all checks before committing
./scripts/check_all.sh

# Fix formatting issues
./scripts/run_clang_format.sh --fix

# Run only static analysis
./scripts/run_cppcheck.sh
```

### Installing Shell Completions
```bash
# Quick install for current user
./scripts/install-completions.sh

# System-wide install for all users
sudo ./scripts/install-completions.sh --system
```

### Using Shell Completions
```bash
# After installation, use TAB to complete
cmdgpt --<TAB>           # Shows all options
cmdgpt --format <TAB>    # Shows format options
cmdgpt -m <TAB>          # Shows model options
```

## Requirements

### For Code Quality Scripts
- `clang-format` (version 10 or higher)
- `cppcheck` (version 1.90 or higher)
- CMake and make (for build verification)
- Standard Unix tools (find, grep, sed)

### For Shell Completions
- Bash 4.0+ or Zsh 5.0+
- Write access to home directory (user install)
- sudo access (system install)

## Troubleshooting

### Code Formatting Issues
If `run_clang_format.sh --fix` doesn't resolve all issues:
1. Check that you have the correct clang-format version
2. Ensure no files are open in editors that might lock them
3. Run with `--verbose` to see which files have issues

### Completion Not Working
If completions don't work after installation:
1. Start a new shell session or run `source ~/.bashrc` / `source ~/.zshrc`
2. Check that the completion files exist in the scripts directory
3. For zsh, ensure `compinit` is called in your `.zshrc`
4. Run `which cmdgpt` to ensure the binary is in your PATH

### Static Analysis False Positives
If cppcheck reports false positives:
1. Add suppressions to `cppcheck_suppressions.txt`
2. Use inline suppressions in code: `// cppcheck-suppress [warning-id]`
3. Update cppcheck to the latest version

## Contributing

When adding new scripts:
1. Make them executable: `chmod +x script-name.sh`
2. Add proper error handling and exit codes
3. Include usage information with `--help`
4. Update this README with documentation
5. Test on both Linux and macOS