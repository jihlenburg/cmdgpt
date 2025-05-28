/**
 * @file file_rate_limiter.h
 * @brief File-based rate limiter for cross-process synchronization
 * @author cmdgpt
 * @date 2025
 * @copyright MIT License
 */

#ifndef FILE_RATE_LIMITER_H
#define FILE_RATE_LIMITER_H

#include <filesystem>
#include <chrono>
#include <atomic>
#include <memory>

namespace cmdgpt
{

/**
 * @brief File-based token bucket rate limiter for cross-process synchronization
 *
 * This class implements a token bucket algorithm using file-based storage
 * to ensure rate limiting works across multiple processes. It uses file
 * locking to ensure atomic updates.
 *
 * Example usage:
 * @code
 * FileRateLimiter limiter("/var/tmp/cmdgpt_rate_limit", 3.0, 5);
 * if (limiter.try_acquire()) {
 *     // Make API call
 * } else {
 *     // Rate limited, wait before retrying
 * }
 * @endcode
 */
class FileRateLimiter
{
  public:
    /**
     * @brief Construct a file-based rate limiter
     *
     * @param state_file Path to the shared state file
     * @param rate Tokens per second to generate
     * @param burst_size Maximum burst capacity (bucket size)
     * @param timeout_ms Timeout for acquiring lock (milliseconds)
     */
    explicit FileRateLimiter(const std::filesystem::path& state_file,
                            double rate = 3.0,
                            size_t burst_size = 5,
                            std::chrono::milliseconds timeout_ms = std::chrono::milliseconds(5000));

    /**
     * @brief Destructor
     */
    ~FileRateLimiter() = default;

    // Disable copy
    FileRateLimiter(const FileRateLimiter&) = delete;
    FileRateLimiter& operator=(const FileRateLimiter&) = delete;

    // Enable move
    FileRateLimiter(FileRateLimiter&&) = default;
    FileRateLimiter& operator=(FileRateLimiter&&) = default;

    /**
     * @brief Try to acquire a token
     *
     * Non-blocking. Returns immediately if a token is available,
     * otherwise returns false.
     *
     * @param tokens Number of tokens to acquire (default: 1)
     * @return True if tokens were acquired, false if rate limited
     */
    bool try_acquire(size_t tokens = 1);

    /**
     * @brief Acquire tokens, blocking until available
     *
     * @param tokens Number of tokens to acquire (default: 1)
     * @param max_wait Maximum time to wait (0 = infinite)
     * @return True if tokens were acquired, false if timeout
     */
    bool acquire(size_t tokens = 1, 
                 std::chrono::milliseconds max_wait = std::chrono::milliseconds(0));

    /**
     * @brief Get current number of available tokens
     *
     * @return Number of tokens available in the bucket
     */
    double get_available_tokens() const;

    /**
     * @brief Get time until next token is available
     *
     * @return Duration until a token becomes available (0 if tokens available)
     */
    std::chrono::milliseconds time_until_available() const;

    /**
     * @brief Reset the rate limiter (clear all tokens)
     *
     * Useful for testing or administrative purposes.
     */
    void reset();

    /**
     * @brief Clean up stale state files
     *
     * Removes state files that haven't been updated in over an hour,
     * which likely indicates crashed processes.
     *
     * @param directory Directory containing state files
     * @param max_age Maximum age before considering file stale
     */
    static void cleanup_stale_files(const std::filesystem::path& directory,
                                   std::chrono::hours max_age = std::chrono::hours(1));

  private:
    struct TokenBucketState
    {
        double tokens;                    ///< Current token count
        int64_t last_update_ms;          ///< Last update timestamp (ms since epoch)
        double rate;                     ///< Tokens per second
        size_t burst_size;               ///< Maximum bucket capacity
        uint32_t version;                ///< Format version for future compatibility
    };

    std::filesystem::path state_file_;   ///< Path to shared state file
    double rate_;                        ///< Tokens per second
    size_t burst_size_;                  ///< Maximum burst capacity
    std::chrono::milliseconds timeout_;  ///< Lock acquisition timeout

    /**
     * @brief Load current state from file
     *
     * @return Current token bucket state
     */
    TokenBucketState load_state() const;

    /**
     * @brief Save state to file
     *
     * @param state Token bucket state to save
     */
    void save_state(const TokenBucketState& state);

    /**
     * @brief Update token count based on elapsed time
     *
     * @param state Current state to update
     * @param now Current timestamp
     */
    void update_tokens(TokenBucketState& state, 
                      std::chrono::system_clock::time_point now) const;
};

} // namespace cmdgpt

#endif // FILE_RATE_LIMITER_H