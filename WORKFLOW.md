# Development Workflow Guide

## Overview

This document describes the development workflow for the CmdGPT project, including branching strategy, release process, and contribution guidelines.

## Git Branching Strategy

### Branch Types

- **main**: Production-ready code, contains only released versions
- **staging**: Integration branch for testing features before release
- **feature/***: Feature development branches

### Branch Protection

- `main` branch:
  - Direct pushes disabled
  - Requires pull request reviews
  - All CI checks must pass
  
- `staging` branch:
  - Accepts pull requests from feature branches
  - Used for integration testing

## Development Process

### 1. Starting New Features

```bash
# Create feature branch from staging
git checkout staging
git pull origin staging
git checkout -b feature/your-feature-name

# Make your changes
# ... edit files ...

# Run tests
cmake -B build -DBUILD_TESTS=ON
cmake --build build
ctest --test-dir build

# Build documentation
./build_docs.sh
```

### 2. Committing Changes

```bash
# Stage changes
git add -p  # Interactive staging recommended

# Commit with descriptive message
git commit -m "Add feature: brief description

- Detailed change 1
- Detailed change 2"
```

### 3. Creating Pull Requests

```bash
# Push feature branch
git push -u origin feature/your-feature-name

# Create PR via GitHub CLI
gh pr create --base staging --title "Feature: Your feature" --body "Description"
```

### 4. Code Review Process

- All PRs require at least one review
- Address review feedback promptly
- Ensure CI checks pass
- Update documentation if needed

## Release Process

### 1. Prepare Release

```bash
# Ensure staging is up to date
git checkout staging
git pull origin staging

# Update version in CMakeLists.txt
# Update CHANGELOG.md

# Commit version bump
git add CMakeLists.txt CHANGELOG.md
git commit -m "Bump version to X.Y.Z"
git push origin staging
```

### 2. Create Release PR

```bash
# Create PR from staging to main
gh pr create --base main --head staging --title "Release v$VERSION" \
  --body "Release notes from CHANGELOG.md"
```

### 3. Tag and Release

After PR is merged to main:

```bash
# Checkout main and pull
git checkout main
git pull origin main

# Create annotated tag
git tag -a v$VERSION -m "Release v$VERSION"
git push origin v$VERSION

# Create GitHub release
gh release create v$VERSION --title "v$VERSION" \
  --notes "$(cat CHANGELOG.md | sed -n '/## \[X.Y.Z\]/,/## \[/p' | head -n -1)"
```

### 4. Post-Release

```bash
# Merge main back to staging
git checkout staging
git merge main
git push origin staging
```

## Building and Testing

### Local Development

```bash
# Clean build with tests
rm -rf build
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Run tests
ctest --test-dir build --verbose

# Build documentation
./build_docs.sh --open  # Opens in browser
```

### Documentation

- Update inline documentation for new/modified functions
- Run Doxygen to ensure no warnings
- Update README.md for user-facing changes
- Update CLAUDE.md for architectural changes

## Code Quality Standards

### Before Submitting PR

1. **Code Style**
   - Follow existing code conventions
   - Use consistent indentation (4 spaces)
   - Add Doxygen comments for public APIs

2. **Testing**
   - Add tests for new functionality
   - Ensure all existing tests pass
   - Test edge cases and error conditions

3. **Security**
   - Validate all external inputs
   - Never log sensitive data (API keys, passwords)
   - Use secure defaults

4. **Documentation**
   - Update relevant .md files
   - Add inline comments for complex logic
   - Include examples for new features

## Continuous Integration

### CI Pipeline Checks

1. Build compilation (Debug and Release)
2. Unit test execution
3. Documentation generation
4. Code coverage analysis (if configured)

### Local CI Simulation

```bash
# Run full CI-like check locally
./scripts/ci-check.sh  # If available
# Or manually:
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
cmake --build build-debug
ctest --test-dir build-debug

cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
```

## Troubleshooting

### Common Issues

1. **Build Failures**
   - Ensure OpenSSL is installed: `brew install openssl`
   - Check CMake version: `cmake --version` (>= 3.10)

2. **Documentation Warnings**
   - Run `./build_docs.sh` to check for Doxygen warnings
   - Ensure no duplicate @mainpage directives

3. **Test Failures**
   - Check test output: `ctest --test-dir build --verbose`
   - Verify API keys are properly set in tests

## Version Numbering

We follow [Semantic Versioning](https://semver.org/):

- **MAJOR**: Incompatible API changes
- **MINOR**: New functionality, backwards compatible
- **PATCH**: Bug fixes, backwards compatible

Example: v0.3.0
- 0 = Major (pre-1.0 development)
- 3 = Minor (feature additions)
- 0 = Patch (initial release of 0.3)