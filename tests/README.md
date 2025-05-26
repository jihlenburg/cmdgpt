# Tests Directory

This directory contains the test suite for cmdgpt using the Catch2 testing framework.

## Test Structure

### cmdgpt_tests.cpp
The main test file containing all unit tests organized by feature area:

1. **Basic Functionality Tests**
   - Constants verification
   - Configuration defaults
   - Version string format

2. **Configuration Tests**
   - Environment variable loading
   - Configuration validation
   - Setting and getting configuration values

3. **Utility Function Tests**
   - Output formatting (plain, JSON, markdown, code)
   - Prompt validation
   - API key validation
   - API key redaction for logs

4. **Command Parsing Tests**
   - Interactive mode command parsing
   - Command validation
   - Argument extraction

5. **Conversation Management Tests**
   - Message creation and storage
   - Conversation serialization/deserialization
   - Token estimation
   - Context trimming

6. **Error Handling Tests**
   - Exception types and messages
   - Error propagation
   - Recovery mechanisms

## Running Tests

### Run All Tests
```bash
# From build directory
./cmdgpt_tests

# Or using CTest
cd build && ctest -V
```

### Run Specific Test
```bash
# Run by test name
./cmdgpt_tests "parse_command handles exit command"

# Run by tag
./cmdgpt_tests "[configuration]"
```

### Verbose Output
```bash
# Show all test output
./cmdgpt_tests -v

# Show test durations
./cmdgpt_tests -d yes
```

### List Available Tests
```bash
./cmdgpt_tests --list-tests
```

## Writing New Tests

### Test Structure
```cpp
TEST_CASE("Feature being tested", "[category]")
{
    // Setup
    cmdgpt::Config config;
    
    // Test
    SECTION("Specific behavior")
    {
        // Arrange
        config.set_api_key("test-key");
        
        // Act
        auto result = some_function(config);
        
        // Assert
        REQUIRE(result == expected_value);
    }
}
```

### Best Practices

1. **Use Descriptive Names**
   - Test case: What is being tested
   - Section: Specific behavior or edge case

2. **Follow AAA Pattern**
   - Arrange: Set up test data
   - Act: Execute the code under test
   - Assert: Verify the results

3. **Use Sections for Related Tests**
   ```cpp
   TEST_CASE("API key validation")
   {
       SECTION("rejects empty keys")
       SECTION("rejects whitespace-only keys")
       SECTION("accepts valid keys")
   }
   ```

4. **Test Edge Cases**
   - Empty inputs
   - Maximum size inputs
   - Invalid characters
   - Boundary conditions

5. **Use Appropriate Assertions**
   - `REQUIRE()` - Test fails if false
   - `CHECK()` - Test continues if false
   - `REQUIRE_THROWS()` - Expects exception
   - `REQUIRE_NOTHROW()` - Expects no exception

## Test Coverage Areas

### Currently Tested
- ✅ Configuration management
- ✅ Output formatting
- ✅ Input validation
- ✅ Command parsing
- ✅ Conversation handling
- ✅ Exception handling
- ✅ Utility functions

### Planned Tests
- 🔄 Streaming response simulation
- 🔄 Retry logic with mocked failures
- 🔄 Context recovery mechanisms
- 🔄 Shell integration features
- 🔄 Rate limit handling

## Testing New Features

When adding new features:

1. **Write Tests First (TDD)**
   - Define expected behavior
   - Write failing tests
   - Implement feature
   - Make tests pass

2. **Test All Code Paths**
   - Success cases
   - Error cases
   - Edge cases
   - Integration with existing features

3. **Mock External Dependencies**
   - API calls (future improvement)
   - File system operations
   - Network connections

## Debugging Tests

### When Tests Fail

1. **Run Single Test**
   ```bash
   ./cmdgpt_tests "exact test name"
   ```

2. **Enable Debug Output**
   ```bash
   CMDGPT_LOG_LEVEL=DEBUG ./cmdgpt_tests
   ```

3. **Use Debugger**
   ```bash
   lldb ./cmdgpt_tests
   (lldb) run "test name"
   ```

4. **Check Test Output**
   - Look for line numbers in assertions
   - Check REQUIRE vs CHECK failures
   - Verify test data setup

## Continuous Integration

Tests are automatically run:
- On every push via GitHub Actions
- On all pull requests
- Across multiple platforms (Linux, macOS, Windows)

### CI Requirements
- All tests must pass
- No new warnings from static analysis
- Code coverage should not decrease
- Tests must complete within 5 minutes

## Future Improvements

1. **Mocking Framework**
   - Mock HTTP client for API tests
   - Mock file system for I/O tests

2. **Performance Tests**
   - Response time benchmarks
   - Memory usage tests
   - Stress tests for conversation size

3. **Integration Tests**
   - End-to-end command execution
   - Real API interaction tests (with test key)
   - Shell completion verification

4. **Property-Based Testing**
   - Fuzz testing for input validation
   - Random input generation
   - Invariant checking