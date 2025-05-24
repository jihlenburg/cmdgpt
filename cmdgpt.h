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

/**
 * @brief Prints the help message to the console
 */
void print_help();

/**
 * @brief Sends a chat completion request to the OpenAI API
 *
 * @param prompt The user's input prompt
 * @param api_key OpenAI API key (empty string uses environment variable)
 * @param system_prompt System prompt to set context (empty string uses default)
 * @param model The model to use (empty string uses default)
 * @return The API response text
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 */
std::string get_gpt_chat_response(std::string_view prompt, std::string_view api_key = "",
                                  std::string_view system_prompt = "", std::string_view model = "");

} // namespace cmdgpt

// Global logger instance
extern std::shared_ptr<spdlog::logger> gLogger;

#endif // CMDGPT_H
