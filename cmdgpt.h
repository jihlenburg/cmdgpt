/*
MIT License

Copyright (c) 2023 Joern Ihlenburg

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

#ifndef CMDGPT_H
#define CMDGPT_H

#include "spdlog/spdlog.h"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// Forward declarations
namespace cmdgpt
{

// Version information
inline constexpr std::string_view VERSION = "v0.2";

// Model and prompt defaults
inline constexpr std::string_view DEFAULT_MODEL = "gpt-4";
inline constexpr std::string_view DEFAULT_SYSTEM_PROMPT = "You are a helpful assistant!";
inline constexpr spdlog::level::level_enum DEFAULT_LOG_LEVEL = spdlog::level::warn;

// HTTP headers
inline constexpr std::string_view AUTHORIZATION_HEADER = "Authorization";
inline constexpr std::string_view CONTENT_TYPE_HEADER = "Content-Type";
inline constexpr std::string_view APPLICATION_JSON = "application/json";

// JSON keys for OpenAI API
inline constexpr std::string_view SYSTEM_ROLE = "system";
inline constexpr std::string_view USER_ROLE = "user";
inline constexpr std::string_view MODEL_KEY = "model";
inline constexpr std::string_view MESSAGES_KEY = "messages";
inline constexpr std::string_view ROLE_KEY = "role";
inline constexpr std::string_view CONTENT_KEY = "content";
inline constexpr std::string_view CHOICES_KEY = "choices";
inline constexpr std::string_view FINISH_REASON_KEY = "finish_reason";

// API configuration
inline constexpr std::string_view API_URL = "/v1/chat/completions";
inline constexpr std::string_view SERVER_URL = "https://api.openai.com";

// Security and validation limits
inline constexpr size_t MAX_PROMPT_LENGTH = 1024 * 1024;        // 1MB max prompt
inline constexpr size_t MAX_RESPONSE_LENGTH = 10 * 1024 * 1024; // 10MB max response
inline constexpr size_t MAX_API_KEY_LENGTH = 256;               // Max API key length
inline constexpr int CONNECTION_TIMEOUT_SECONDS = 30;
inline constexpr int READ_TIMEOUT_SECONDS = 60;

// HTTP status codes
enum class HttpStatus : int
{
    EMPTY_RESPONSE = -1,
    OK = 200,
    BAD_REQUEST = 400,
    UNAUTHORIZED = 401,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    INTERNAL_SERVER_ERROR = 500
};

// Custom exception classes for better error handling
class CmdGptException : public std::runtime_error
{
  public:
    explicit CmdGptException(const std::string& message) : std::runtime_error(message)
    {
    }
};

class ApiException : public CmdGptException
{
  public:
    ApiException(HttpStatus status, const std::string& message)
        : CmdGptException("API Error [" + std::to_string(static_cast<int>(status)) +
                          "]: " + message),
          status_code_(status)
    {
    }

    HttpStatus status_code() const noexcept
    {
        return status_code_;
    }

  private:
    HttpStatus status_code_;
};

class NetworkException : public CmdGptException
{
  public:
    explicit NetworkException(const std::string& message)
        : CmdGptException("Network Error: " + message)
    {
    }
};

class ConfigurationException : public CmdGptException
{
  public:
    explicit ConfigurationException(const std::string& message)
        : CmdGptException("Configuration Error: " + message)
    {
    }
};

class ValidationException : public CmdGptException
{
  public:
    explicit ValidationException(const std::string& message)
        : CmdGptException("Validation Error: " + message)
    {
    }
};

/**
 * @brief Configuration management class using modern C++ features
 */
class Config
{
  public:
    Config() = default;
    ~Config() = default;

    // Non-copyable but movable
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = default;
    Config& operator=(Config&&) = default;

    // Configuration setters with validation
    void set_api_key(std::string_view key);
    void set_system_prompt(std::string_view prompt);
    void set_model(std::string_view model);
    void set_log_file(std::string_view file);
    void set_log_level(spdlog::level::level_enum level);

    // Configuration getters
    const std::string& api_key() const noexcept
    {
        return api_key_;
    }
    const std::string& system_prompt() const noexcept
    {
        return system_prompt_;
    }
    const std::string& model() const noexcept
    {
        return model_;
    }
    const std::string& log_file() const noexcept
    {
        return log_file_;
    }
    spdlog::level::level_enum log_level() const noexcept
    {
        return log_level_;
    }

    // Load configuration from environment variables
    void load_from_environment();

    // Validate all configuration values
    void validate() const;

  private:
    std::string api_key_;
    std::string system_prompt_{DEFAULT_SYSTEM_PROMPT};
    std::string model_{DEFAULT_MODEL};
    std::string log_file_{"logfile.txt"};
    spdlog::level::level_enum log_level_{DEFAULT_LOG_LEVEL};
};

/**
 * @brief Validates and sanitizes API key input
 * @param api_key The API key to validate
 * @throws ValidationException if API key is invalid
 */
void validate_api_key(std::string_view api_key);

/**
 * @brief Validates input prompt length and content
 * @param prompt The prompt to validate
 * @throws ValidationException if prompt is invalid
 */
void validate_prompt(std::string_view prompt);

/**
 * @brief Returns a redacted version of API key for logging
 * @param api_key The API key to redact
 * @return Safely redacted API key for logging
 */
std::string redact_api_key(std::string_view api_key);

/**
 * @brief Prints the help message to the console
 */
void print_help();

/**
 * @brief Sends a chat completion request to the OpenAI API (legacy interface)
 *
 * @param prompt The user's input prompt
 * @param api_key OpenAI API key (empty string uses environment variable)
 * @param system_prompt System prompt to set context (empty string uses default)
 * @param model The model to use (empty string uses default)
 * @return The API response text
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 */
std::string get_gpt_chat_response(std::string_view prompt, std::string_view api_key = "",
                                  std::string_view system_prompt = "", std::string_view model = "");

/**
 * @brief Sends a chat completion request to the OpenAI API (modern interface)
 *
 * @param prompt The user's input prompt
 * @param config Configuration object containing API settings
 * @return The API response text
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 */
std::string get_gpt_chat_response(std::string_view prompt, const Config& config);

} // namespace cmdgpt

// Global logger instance
extern std::shared_ptr<spdlog::logger> gLogger;

#endif // CMDGPT_H
