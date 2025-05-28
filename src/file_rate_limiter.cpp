/**
 * @file file_rate_limiter.cpp
 * @brief Implementation of file-based rate limiter
 * @author cmdgpt
 * @date 2025
 * @copyright MIT License
 */

#include "file_rate_limiter.h"
#include "file_lock.h"
#include <fstream>
#include <thread>
#include <cstring>

namespace cmdgpt
{

FileRateLimiter::FileRateLimiter(const std::filesystem::path& state_file,
                                 double rate,
                                 size_t burst_size,
                                 std::chrono::milliseconds timeout_ms)
    : state_file_(state_file),
      rate_(rate),
      burst_size_(burst_size),
      timeout_(timeout_ms)
{
    // Ensure parent directory exists
    std::filesystem::create_directories(state_file_.parent_path());
    
    // Initialize state file if it doesn't exist
    if (!std::filesystem::exists(state_file_))
    {
        TokenBucketState initial_state{
            static_cast<double>(burst_size_),  // Start with full bucket
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count(),
            rate_,
            burst_size_,
            1  // Version 1
        };
        save_state(initial_state);
    }
}

bool FileRateLimiter::try_acquire(size_t tokens)
{
    try
    {
        // Acquire exclusive lock
        FileLock lock(state_file_, LockType::EXCLUSIVE, timeout_);
        
        // Load current state
        auto state = load_state();
        
        // Update tokens based on elapsed time
        auto now = std::chrono::system_clock::now();
        update_tokens(state, now);
        
        // Check if we have enough tokens
        if (state.tokens >= static_cast<double>(tokens))
        {
            state.tokens -= static_cast<double>(tokens);
            state.last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            save_state(state);
            return true;
        }
        
        return false;
    }
    catch (const std::exception&)
    {
        // Lock timeout or other error - be conservative and deny
        return false;
    }
}

bool FileRateLimiter::acquire(size_t tokens, std::chrono::milliseconds max_wait)
{
    auto start = std::chrono::steady_clock::now();
    
    while (true)
    {
        if (try_acquire(tokens))
        {
            return true;
        }
        
        // Check timeout
        if (max_wait.count() > 0)
        {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= max_wait)
            {
                return false;
            }
        }
        
        // Wait a bit before retrying
        auto wait_time = time_until_available();
        if (wait_time.count() > 0)
        {
            std::this_thread::sleep_for(std::min(wait_time, std::chrono::milliseconds(100)));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

double FileRateLimiter::get_available_tokens() const
{
    try
    {
        // Acquire shared lock for reading
        FileLock lock(state_file_, LockType::SHARED, timeout_);
        
        auto state = load_state();
        auto now = std::chrono::system_clock::now();
        
        // Calculate current tokens without modifying state
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count() - state.last_update_ms;
        double elapsed_seconds = elapsed_ms / 1000.0;
        double new_tokens = state.tokens + (elapsed_seconds * state.rate);
        
        return std::min(new_tokens, static_cast<double>(state.burst_size));
    }
    catch (const std::exception&)
    {
        return 0.0;
    }
}

std::chrono::milliseconds FileRateLimiter::time_until_available() const
{
    double available = get_available_tokens();
    if (available >= 1.0)
    {
        return std::chrono::milliseconds(0);
    }
    
    // Calculate time needed to generate one token
    double tokens_needed = 1.0 - available;
    double seconds_needed = tokens_needed / rate_;
    
    return std::chrono::milliseconds(static_cast<int64_t>(seconds_needed * 1000));
}

void FileRateLimiter::reset()
{
    FileLock lock(state_file_, LockType::EXCLUSIVE, timeout_);
    
    TokenBucketState state{
        0.0,  // Empty bucket
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count(),
        rate_,
        burst_size_,
        1
    };
    
    save_state(state);
}

void FileRateLimiter::cleanup_stale_files(const std::filesystem::path& directory,
                                          std::chrono::hours max_age)
{
    if (!std::filesystem::exists(directory))
    {
        return;
    }
    
    auto now = std::chrono::system_clock::now();
    
    for (const auto& entry : std::filesystem::directory_iterator(directory))
    {
        if (entry.path().extension() == ".ratelimit")
        {
            try
            {
                auto file_time = std::filesystem::last_write_time(entry.path());
                // Convert file_time to system_clock time_point
                auto file_sys_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    file_time - std::filesystem::file_time_type::clock::now() + 
                    std::chrono::system_clock::now()
                );
                auto file_age = now - file_sys_time;
                
                if (file_age > max_age)
                {
                    std::filesystem::remove(entry.path());
                }
            }
            catch (...)
            {
                // Ignore errors during cleanup
            }
        }
    }
}

FileRateLimiter::TokenBucketState FileRateLimiter::load_state() const
{
    std::ifstream file(state_file_, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open rate limiter state file");
    }
    
    TokenBucketState state;
    file.read(reinterpret_cast<char*>(&state), sizeof(state));
    
    if (!file.good() || state.version != 1)
    {
        throw std::runtime_error("Invalid rate limiter state file");
    }
    
    return state;
}

void FileRateLimiter::save_state(const TokenBucketState& state)
{
    // Use atomic writer to ensure consistency
    AtomicFileWriter writer(state_file_);
    
    // Write as binary for efficiency
    writer.write(std::vector<uint8_t>(
        reinterpret_cast<const uint8_t*>(&state),
        reinterpret_cast<const uint8_t*>(&state) + sizeof(state)
    ));
    
    writer.commit();
}

void FileRateLimiter::update_tokens(TokenBucketState& state,
                                   std::chrono::system_clock::time_point now) const
{
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    auto elapsed_ms = now_ms - state.last_update_ms;
    if (elapsed_ms <= 0)
    {
        return;  // No time has passed or clock went backwards
    }
    
    double elapsed_seconds = elapsed_ms / 1000.0;
    double new_tokens = state.tokens + (elapsed_seconds * state.rate);
    
    // Cap at burst size
    state.tokens = std::min(new_tokens, static_cast<double>(state.burst_size));
    state.last_update_ms = now_ms;
}

} // namespace cmdgpt