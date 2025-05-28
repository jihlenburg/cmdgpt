# Concurrency Fixes for cmdgpt v0.6.1

## Overview

This document describes the comprehensive concurrency fixes implemented to resolve issues when multiple cmdgpt instances run in parallel.

## Problems Fixed

1. **Rate Limiter Bypass**: Multiple processes could exceed API rate limits
2. **Cache Corruption**: Concurrent writes to cache files could cause corruption  
3. **History File Corruption**: Multiple processes writing to history.json simultaneously
4. **Log File Conflicts**: All processes writing to the same log file

## Solutions Implemented

### 1. Cross-Platform File Locking (`file_lock.h/cpp`)

Implemented RAII-based file locking utilities:

- **FileLock**: Provides shared/exclusive file locking
  - Uses `flock()` on Unix/macOS
  - Uses Windows file locking API on Windows
  - Supports timeouts to prevent deadlocks
  - Move-constructible for use in containers

- **ScopedLockFile**: Simple mutex-like lock file
  - Creates a `.lock` file that exists only while held
  - Useful for simple mutual exclusion

- **AtomicFileWriter**: Safe atomic file writes
  - Writes to temporary file first
  - Atomically renames to target on commit
  - Automatically cleans up on failure

### 2. File-Based Rate Limiter (`file_rate_limiter.h/cpp`)

Implemented a cross-process rate limiter using file-based storage:

- **Token Bucket Algorithm**: Standard rate limiting approach
- **Binary State File**: Efficient storage of rate limiter state
- **Process Synchronization**: Uses file locking for atomic updates
- **Configurable Parameters**:
  - Rate: Tokens per second (default: 3.0)
  - Burst: Maximum burst capacity (default: 5)
  - Timeout: Lock acquisition timeout (default: 5s)

### 3. Process-Specific Log Files

Modified logging to use process-specific files by default:

```cpp
// Default log file: logfile.txt → logfile_<PID>.txt
if (log_file == "logfile.txt") {
    log_file = "logfile_" + std::to_string(getpid()) + ".txt";
}
```

### 4. Safe Cache Operations

Updated `ResponseCache` to use file locking:

- **Read Operations**: Use shared locks (multiple readers allowed)
- **Write Operations**: Use atomic file writer (prevents corruption)
- **Example**:
  ```cpp
  // Reading with shared lock
  FileLock lock(path, LockType::SHARED);
  
  // Writing atomically
  AtomicFileWriter writer(path);
  writer.write(cache_data.dump(2));
  writer.commit();
  ```

### 5. Safe History Operations

Updated `ResponseHistory` to prevent concurrent write conflicts:

- **Load**: Uses shared file lock
- **Save**: Uses atomic file writer
- **In-Memory Mutex**: Still used for thread safety within a process

## Usage Examples

### Running Multiple Instances

```bash
# These can now run safely in parallel
cmdgpt "Question 1" &
cmdgpt "Question 2" &
cmdgpt "Question 3" &
wait
```

### Checking Process-Specific Logs

```bash
# Each process creates its own log file
ls -la logfile_*.txt
```

### Rate Limiter State

```bash
# Shared rate limiter state file
ls -la ~/.cmdgpt/rate_limiter.state
```

## Testing

A comprehensive test suite was created (`test_file_locking.cpp`) that verifies:

1. Exclusive file locks prevent concurrent access
2. Shared file locks allow multiple readers
3. Atomic file writer prevents corruption
4. Rate limiter enforces limits across processes
5. Timeout mechanisms prevent deadlocks

## Performance Considerations

1. **File Locking Overhead**: Minimal (~1-5ms per operation)
2. **Atomic Writes**: Slightly slower than direct writes but ensure consistency
3. **Rate Limiter**: Efficient binary format minimizes I/O
4. **Log Files**: No contention as each process has its own file

## Backward Compatibility

- Existing single-process usage is unaffected
- Cache format remains unchanged
- History format remains unchanged
- Configuration files remain unchanged

## Future Enhancements

1. **Distributed Locking**: For multi-machine setups
2. **Lock-Free Algorithms**: For specific hot paths
3. **Shared Memory IPC**: For lower latency coordination
4. **Lock Monitoring**: Tools to debug lock contention

## Conclusion

These fixes ensure cmdgpt can be safely used in parallel execution scenarios, making it suitable for:
- Batch processing scripts
- CI/CD pipelines
- Concurrent user sessions
- High-throughput applications

The implementation prioritizes correctness and safety while maintaining good performance for typical usage patterns.