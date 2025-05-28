/**
 * @file file_lock.cpp
 * @brief Implementation of cross-platform file locking utilities
 * @author cmdgpt
 * @date 2025
 * @copyright MIT License
 */

#include "file_lock.h"

#include <stdexcept>
#include <chrono>
#include <thread>
#include <sstream>
#include <random>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <share.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#endif

namespace cmdgpt
{

FileLock::FileLock(const std::filesystem::path& path, 
                   LockType type,
                   std::chrono::milliseconds timeout_ms)
    : path_(path), type_(type), locked_(false)
{
    auto start = std::chrono::steady_clock::now();
    
    // Ensure parent directory exists
    std::filesystem::create_directories(path_.parent_path());
    
#ifdef _WIN32
    // Windows implementation
    DWORD access = (type == LockType::SHARED) ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE);
    DWORD share = (type == LockType::SHARED) ? FILE_SHARE_READ : 0;
    DWORD creation = OPEN_ALWAYS;
    
    while (true)
    {
        file_handle_ = CreateFileW(path_.wstring().c_str(),
                                   access,
                                   share,
                                   nullptr,
                                   creation,
                                   FILE_ATTRIBUTE_NORMAL,
                                   nullptr);
        
        if (file_handle_ != INVALID_HANDLE_VALUE)
        {
            locked_ = true;
            break;
        }
        
        if (GetLastError() != ERROR_SHARING_VIOLATION)
        {
            throw std::runtime_error("Failed to open file for locking: " + path_.string());
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout_ms)
        {
            throw std::runtime_error("Timeout acquiring lock on: " + path_.string());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#else
    // POSIX implementation
    int flags = O_CREAT | O_RDWR;
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    
    fd_ = open(path_.c_str(), flags, mode);
    if (fd_ == -1)
    {
        throw std::runtime_error("Failed to open file for locking: " + path_.string());
    }
    
    int lock_op = (type == LockType::SHARED) ? LOCK_SH : LOCK_EX;
    lock_op |= LOCK_NB; // Non-blocking
    
    while (true)
    {
        if (flock(fd_, lock_op) == 0)
        {
            locked_ = true;
            break;
        }
        
        if (errno != EWOULDBLOCK)
        {
            close(fd_);
            throw std::runtime_error("Failed to acquire lock on: " + path_.string());
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout_ms)
        {
            close(fd_);
            throw std::runtime_error("Timeout acquiring lock on: " + path_.string());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif
}

FileLock::~FileLock()
{
    unlock();
}

FileLock::FileLock(FileLock&& other) noexcept
    : path_(std::move(other.path_)),
      type_(other.type_),
      locked_(other.locked_)
{
#ifdef _WIN32
    file_handle_ = other.file_handle_;
    other.file_handle_ = INVALID_HANDLE_VALUE;
#else
    fd_ = other.fd_;
    other.fd_ = -1;
#endif
    other.locked_ = false;
}

FileLock& FileLock::operator=(FileLock&& other) noexcept
{
    if (this != &other)
    {
        unlock();
        path_ = std::move(other.path_);
        type_ = other.type_;
        locked_ = other.locked_;
#ifdef _WIN32
        file_handle_ = other.file_handle_;
        other.file_handle_ = INVALID_HANDLE_VALUE;
#else
        fd_ = other.fd_;
        other.fd_ = -1;
#endif
        other.locked_ = false;
    }
    return *this;
}

void FileLock::unlock()
{
    if (!locked_)
    {
        return;
    }
    
#ifdef _WIN32
    if (file_handle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file_handle_);
        file_handle_ = INVALID_HANDLE_VALUE;
    }
#else
    if (fd_ != -1)
    {
        flock(fd_, LOCK_UN);
        close(fd_);
        fd_ = -1;
    }
#endif
    locked_ = false;
}

bool FileLock::try_lock_shared()
{
    if (locked_ || type_ != LockType::EXCLUSIVE)
    {
        return false;
    }
    
#ifdef _WIN32
    // Windows doesn't support downgrading locks
    return false;
#else
    if (flock(fd_, LOCK_SH | LOCK_NB) == 0)
    {
        type_ = LockType::SHARED;
        return true;
    }
    return false;
#endif
}

bool FileLock::try_upgrade()
{
    if (!locked_ || type_ != LockType::SHARED)
    {
        return false;
    }
    
#ifdef _WIN32
    // Windows doesn't support upgrading locks
    return false;
#else
    if (flock(fd_, LOCK_EX | LOCK_NB) == 0)
    {
        type_ = LockType::EXCLUSIVE;
        return true;
    }
    return false;
#endif
}

ScopedLockFile::ScopedLockFile(const std::filesystem::path& path,
                               std::chrono::milliseconds timeout_ms)
    : lock_file_path_(path.string() + ".lock")
{
    auto start = std::chrono::steady_clock::now();
    
    // Ensure parent directory exists
    std::filesystem::create_directories(lock_file_path_.parent_path());
    
    while (true)
    {
        try
        {
            // Try to create the lock file exclusively
            std::ofstream lock_file(lock_file_path_, std::ios::out | std::ios::app);
            if (!lock_file.is_open())
            {
                throw std::runtime_error("Failed to create lock file");
            }
            
            // Write PID to help with debugging
            lock_file << std::to_string(
#ifdef _WIN32
                GetCurrentProcessId()
#else
                getpid()
#endif
            ) << std::endl;
            lock_file.close();
            
            // Double-check that we created it
            if (std::filesystem::exists(lock_file_path_))
            {
                locked_ = true;
                break;
            }
        }
        catch (...)
        {
            // File already exists or other error
        }
        
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (elapsed >= timeout_ms)
        {
            throw std::runtime_error("Timeout acquiring scoped lock on: " + path.string());
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

ScopedLockFile::~ScopedLockFile()
{
    unlock();
}

ScopedLockFile::ScopedLockFile(ScopedLockFile&& other) noexcept
    : lock_file_path_(std::move(other.lock_file_path_)),
      locked_(other.locked_)
{
    other.locked_ = false;
}

ScopedLockFile& ScopedLockFile::operator=(ScopedLockFile&& other) noexcept
{
    if (this != &other)
    {
        unlock();
        lock_file_path_ = std::move(other.lock_file_path_);
        locked_ = other.locked_;
        other.locked_ = false;
    }
    return *this;
}

void ScopedLockFile::unlock()
{
    if (locked_)
    {
        try
        {
            std::filesystem::remove(lock_file_path_);
        }
        catch (...)
        {
            // Ignore errors during cleanup
        }
        locked_ = false;
    }
}

AtomicFileWriter::AtomicFileWriter(const std::filesystem::path& target_path)
    : target_path_(target_path)
{
    // Generate unique temporary filename
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);
    
    temp_path_ = target_path.parent_path() / 
                 (target_path.filename().string() + ".tmp" + std::to_string(dis(gen)));
    
    // Ensure parent directory exists
    std::filesystem::create_directories(target_path.parent_path());
    
    // Open temporary file
    temp_stream_.open(temp_path_, std::ios::out | std::ios::binary);
    if (!temp_stream_.is_open())
    {
        throw std::runtime_error("Failed to create temporary file: " + temp_path_.string());
    }
}

AtomicFileWriter::~AtomicFileWriter()
{
    if (temp_stream_.is_open())
    {
        temp_stream_.close();
    }
    
    // If not committed, remove temp file
    if (!committed_ && std::filesystem::exists(temp_path_))
    {
        try
        {
            std::filesystem::remove(temp_path_);
        }
        catch (...)
        {
            // Ignore errors during cleanup
        }
    }
}

void AtomicFileWriter::write(const std::string& data)
{
    if (!temp_stream_.is_open())
    {
        throw std::runtime_error("AtomicFileWriter is not open");
    }
    
    temp_stream_ << data;
    if (!temp_stream_.good())
    {
        throw std::runtime_error("Failed to write to temporary file");
    }
}

void AtomicFileWriter::write(const std::vector<uint8_t>& data)
{
    if (!temp_stream_.is_open())
    {
        throw std::runtime_error("AtomicFileWriter is not open");
    }
    
    temp_stream_.write(reinterpret_cast<const char*>(data.data()), data.size());
    if (!temp_stream_.good())
    {
        throw std::runtime_error("Failed to write to temporary file");
    }
}

void AtomicFileWriter::commit()
{
    if (!temp_stream_.is_open())
    {
        throw std::runtime_error("AtomicFileWriter is not open");
    }
    
    // Close the stream
    temp_stream_.close();
    
    // Atomic rename
    try
    {
        std::filesystem::rename(temp_path_, target_path_);
        committed_ = true;
    }
    catch (const std::exception& e)
    {
        // On some systems, rename might fail if target exists
        // Try to remove target first and retry
        try
        {
            std::filesystem::remove(target_path_);
            std::filesystem::rename(temp_path_, target_path_);
            committed_ = true;
        }
        catch (...)
        {
            // Clean up temp file and rethrow
            try { std::filesystem::remove(temp_path_); } catch (...) {}
            throw;
        }
    }
}

// abort() is already defined inline in the header

} // namespace cmdgpt