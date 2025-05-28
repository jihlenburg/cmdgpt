/**
 * @file file_lock.h
 * @brief Cross-platform file locking utilities for concurrent access
 * @author Joern Ihlenburg
 * @date 2025
 * @version 0.7.0
 *
 * This file provides RAII-based file locking mechanisms to ensure
 * safe concurrent access to shared files across multiple processes.
 */

/*
MIT License

Copyright (c) 2025 Joern Ihlenburg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef FILE_LOCK_H
#define FILE_LOCK_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace cmdgpt
{

/**
 * @brief Lock type for file operations
 */
enum class LockType
{
    SHARED,    ///< Multiple readers allowed
    EXCLUSIVE  ///< Single writer, no readers
};

/**
 * @brief RAII file lock for cross-platform file locking
 *
 * Provides automatic lock acquisition and release using RAII principles.
 * Supports both shared (read) and exclusive (write) locks.
 *
 * @code
 * {
 *     FileLock lock("data.json", LockType::EXCLUSIVE);
 *     // Exclusive access to file
 *     // Lock automatically released when lock goes out of scope
 * }
 * @endcode
 */
class FileLock
{
public:
    /**
     * @brief Construct and acquire file lock
     *
     * @param path Path to file to lock
     * @param type Lock type (SHARED or EXCLUSIVE)
     * @param timeout_ms Maximum time to wait for lock (0 = wait forever)
     * @throws std::runtime_error if lock cannot be acquired
     */
    explicit FileLock(const std::filesystem::path& path, 
                     LockType type = LockType::EXCLUSIVE,
                     std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(5000));

    /**
     * @brief Release lock and close file
     */
    ~FileLock();

    // Non-copyable
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;
    
    // Movable
    FileLock(FileLock&&) noexcept;
    FileLock& operator=(FileLock&&) noexcept;

    /**
     * @brief Check if lock is currently held
     * @return true if lock is active
     */
    bool is_locked() const noexcept { return locked_; }

    /**
     * @brief Get the locked file path
     * @return Path to locked file
     */
    const std::filesystem::path& path() const noexcept { return path_; }
    
    /**
     * @brief Manually unlock the file
     * @note Usually not needed as destructor will unlock
     */
    void unlock();
    
    /**
     * @brief Try to downgrade exclusive lock to shared
     * @return true if successful, false otherwise
     */
    bool try_lock_shared();
    
    /**
     * @brief Try to upgrade shared lock to exclusive
     * @return true if successful, false otherwise
     */
    bool try_upgrade();

private:
    std::filesystem::path path_;
    LockType type_;
    bool locked_ = false;

#ifdef _WIN32
    HANDLE file_handle_ = INVALID_HANDLE_VALUE;
#else
    int fd_ = -1;
#endif

    /**
     * @brief Platform-specific lock acquisition
     */
    void acquire_lock(std::chrono::milliseconds timeout_ms);

    /**
     * @brief Platform-specific lock release
     */
    void release_lock();
};

/**
 * @brief Scoped lock file for simple mutex-like behavior
 *
 * Creates a lock file that exists only while the lock is held.
 * Useful for simple mutual exclusion without file access.
 *
 * @code
 * {
 *     ScopedLockFile lock("app.lock");
 *     // Critical section
 * } // Lock file deleted here
 * @endcode
 */
class ScopedLockFile
{
public:
    /**
     * @brief Create lock file
     *
     * @param path Path to lock file
     * @param timeout_ms Maximum time to wait
     * @throws std::runtime_error if lock file already exists after timeout
     */
    explicit ScopedLockFile(const std::filesystem::path& path,
                           std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(5000));

    /**
     * @brief Remove lock file
     */
    ~ScopedLockFile();

    // Non-copyable
    ScopedLockFile(const ScopedLockFile&) = delete;
    ScopedLockFile& operator=(const ScopedLockFile&) = delete;
    
    // Movable
    ScopedLockFile(ScopedLockFile&&) noexcept;
    ScopedLockFile& operator=(ScopedLockFile&&) noexcept;
    
    /**
     * @brief Manually unlock
     */
    void unlock();

private:
    std::filesystem::path lock_file_path_;
    bool locked_ = false;
};

/**
 * @brief Atomic file writer for safe concurrent writes
 *
 * Writes to a temporary file and atomically renames it to the target.
 * This ensures readers never see partial writes.
 */
class AtomicFileWriter
{
public:
    /**
     * @brief Prepare atomic write to file
     *
     * @param path Target file path
     */
    explicit AtomicFileWriter(const std::filesystem::path& path);

    /**
     * @brief Commit the write (atomic rename)
     */
    ~AtomicFileWriter();

    /**
     * @brief Get output stream for writing
     * @return Output file stream
     */
    std::ofstream& stream() { return temp_stream_; }

    /**
     * @brief Write string data
     * @param data String to write
     */
    void write(const std::string& data);
    
    /**
     * @brief Write binary data
     * @param data Binary data to write
     */
    void write(const std::vector<uint8_t>& data);

    /**
     * @brief Commit changes explicitly
     * @return true if successful
     */
    void commit();

    /**
     * @brief Abort changes (don't rename)
     */
    void abort() { aborted_ = true; }

private:
    std::filesystem::path target_path_;
    std::filesystem::path temp_path_;
    std::ofstream temp_stream_;
    bool committed_ = false;
    bool aborted_ = false;
};

} // namespace cmdgpt

#endif // FILE_LOCK_H