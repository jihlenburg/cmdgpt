# CmdGPT Scripts Directory

This directory contains centralized scripts for consistent code quality checks across all environments (local development, CI/CD, etc.).

## Scripts

### run_cppcheck.sh
Runs static code analysis with cppcheck using project-wide consistent settings.

**Usage:**
```bash
./scripts/run_cppcheck.sh
```

**Configuration:**
- Suppressions are defined in `scripts/cppcheck_suppressions.txt`
- Can also use `.cppcheck` project file if present

### run_clang_format.sh
Checks or fixes code formatting according to project style rules.

**Usage:**
```bash
# Check formatting (default)
./scripts/run_clang_format.sh --check

# Fix formatting issues
./scripts/run_clang_format.sh --fix

# Verbose output
./scripts/run_clang_format.sh --check --verbose
```

**Configuration:**
- Uses `.clang-format` file in project root
- Automatically uses clang-format-14 if available (for CI compatibility)

## Configuration Files

### cppcheck_suppressions.txt
Contains suppressions for cppcheck warnings that are:
- Style preferences rather than actual issues
- False positives from macro usage
- System headers we don't control

Each suppression is documented with its rationale.

## Integration

These scripts are used by:
- GitHub Actions CI (`.github/workflows/build.yml`)
- Local quality check script (`check_code_quality.sh`)
- Can be run directly during development

## Benefits

1. **Consistency**: Same checks run locally and in CI
2. **Maintainability**: Single source of truth for tool configuration
3. **Documentation**: Suppressions and settings are documented in one place
4. **Flexibility**: Easy to update settings without modifying multiple files