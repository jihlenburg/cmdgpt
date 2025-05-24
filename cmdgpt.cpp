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

#include "cmdgpt.h"
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;
using namespace cmdgpt;

// Map of string log levels to spdlog::level::level_enum values
const std::map<std::string, spdlog::level::level_enum> log_levels = {
    {"TRACE", spdlog::level::trace}, {"DEBUG", spdlog::level::debug},
    {"INFO", spdlog::level::info},   {"WARN", spdlog::level::warn},
    {"ERROR", spdlog::level::err},   {"CRITICAL", spdlog::level::critical},
};

// Global logger variable accessible by all functions
std::shared_ptr<spdlog::logger> gLogger;

/**
 * @brief Prints the help message to the console.
 */
void cmdgpt::print_help()
{
    std::cout << "Usage: cmdgpt [options] [prompt]\n"
              << "Options:\n"
              << "  -h, --help              Show this help message and exit\n"
              << "  -k, --api_key KEY       Set the OpenAI API key to KEY\n"
              << "  -s, --sys_prompt PROMPT Set the system prompt to PROMPT\n"
              << "  -l, --log_file FILE     Set the log file to FILE\n"
              << "  -m, --gpt_model MODEL   Set the GPT model to MODEL\n"
              << "  -L, --log_level LEVEL   Set the log level to LEVEL\n"
              << "                          (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)\n"
              << "  -v, --version           Print the version of the program and exit\n"
              << "prompt:\n"
              << "  The text prompt to send to the OpenAI GPT API. If not provided, the program "
                 "will read from stdin.\n"
              << "\nEnvironment Variables:\n"
              << "  OPENAI_API_KEY     API key for the OpenAI GPT API.\n"
              << "  OPENAI_SYS_PROMPT  System prompt for the OpenAI GPT API.\n"
              << "  CMDGPT_LOG_FILE    Logfile to record messages.\n"
              << "  OPENAI_GPT_MODEL   GPT model to use.\n"
              << "  CMDGPT_LOG_LEVEL   Log level.\n";
}

/**
 * @brief Validates and sanitizes API key input
 */
void cmdgpt::validate_api_key(std::string_view api_key)
{
    if (api_key.empty())
    {
        throw ValidationException("API key cannot be empty");
    }

    if (api_key.length() > MAX_API_KEY_LENGTH)
    {
        throw ValidationException("API key exceeds maximum allowed length");
    }

    // Check for potentially dangerous characters
    for (char c : api_key)
    {
        if (c < 32 || c > 126) // Only allow printable ASCII
        {
            throw ValidationException("API key contains invalid characters");
        }
    }
}

/**
 * @brief Validates input prompt length and content
 */
void cmdgpt::validate_prompt(std::string_view prompt)
{
    if (prompt.empty())
    {
        throw ValidationException("Prompt cannot be empty");
    }

    if (prompt.length() > MAX_PROMPT_LENGTH)
    {
        throw ValidationException("Prompt exceeds maximum allowed length of " +
                                  std::to_string(MAX_PROMPT_LENGTH) + " characters");
    }
}

/**
 * @brief Returns a redacted version of API key for logging
 */
std::string cmdgpt::redact_api_key(std::string_view api_key)
{
    if (api_key.empty())
    {
        return "[EMPTY]";
    }

    if (api_key.length() <= 8)
    {
        return std::string(api_key.length(), '*');
    }

    // Show first 4 and last 4 characters, redact the middle
    std::string result;
    result += api_key.substr(0, 4);
    result += std::string(api_key.length() - 8, '*');
    result += api_key.substr(api_key.length() - 4);
    return result;
}

/**
 * @brief Config class implementation
 */
void cmdgpt::Config::set_api_key(std::string_view key)
{
    validate_api_key(key);
    api_key_ = key;
}

void cmdgpt::Config::set_system_prompt(std::string_view prompt)
{
    if (prompt.length() > MAX_PROMPT_LENGTH)
    {
        throw ValidationException("System prompt exceeds maximum allowed length");
    }
    system_prompt_ = prompt.empty() ? std::string{DEFAULT_SYSTEM_PROMPT} : std::string{prompt};
}

void cmdgpt::Config::set_model(std::string_view model)
{
    if (model.empty() || model.length() > 100)
    {
        throw ValidationException("Invalid model name");
    }
    model_ = model.empty() ? std::string{DEFAULT_MODEL} : std::string{model};
}

void cmdgpt::Config::set_log_file(std::string_view file)
{
    if (file.empty() || file.length() > 4096)
    {
        throw ValidationException("Invalid log file path");
    }
    log_file_ = file;
}

void cmdgpt::Config::set_log_level(spdlog::level::level_enum level)
{
    log_level_ = level;
}

void cmdgpt::Config::load_from_environment()
{
    // Load API key from environment if available
    if (const char* env_key = std::getenv("OPENAI_API_KEY"))
    {
        api_key_ = env_key;
    }

    // Load system prompt from environment if available
    if (const char* env_prompt = std::getenv("OPENAI_SYS_PROMPT"))
    {
        system_prompt_ = env_prompt;
    }

    // Load model from environment if available
    if (const char* env_model = std::getenv("OPENAI_GPT_MODEL"))
    {
        model_ = env_model;
    }

    // Load log file from environment if available
    if (const char* env_log_file = std::getenv("CMDGPT_LOG_FILE"))
    {
        log_file_ = env_log_file;
    }

    // Load log level from environment if available
    if (const char* env_log_level = std::getenv("CMDGPT_LOG_LEVEL"))
    {
        static const std::map<std::string, spdlog::level::level_enum> log_levels = {
            {"TRACE", spdlog::level::trace}, {"DEBUG", spdlog::level::debug},
            {"INFO", spdlog::level::info},   {"WARN", spdlog::level::warn},
            {"ERROR", spdlog::level::err},   {"CRITICAL", spdlog::level::critical},
        };

        const auto it = log_levels.find(env_log_level);
        if (it != log_levels.end())
        {
            log_level_ = it->second;
        }
    }
}

void cmdgpt::Config::validate() const
{
    if (!api_key_.empty())
    {
        validate_api_key(api_key_);
    }

    if (system_prompt_.length() > MAX_PROMPT_LENGTH)
    {
        throw ValidationException("System prompt exceeds maximum allowed length");
    }

    if (model_.empty() || model_.length() > 100)
    {
        throw ValidationException("Invalid model configuration");
    }

    if (log_file_.empty() || log_file_.length() > 4096)
    {
        throw ValidationException("Invalid log file configuration");
    }
}

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
std::string cmdgpt::get_gpt_chat_response(std::string_view prompt, std::string_view api_key,
                                          std::string_view system_prompt, std::string_view model)
{
    // Validate input prompt first
    validate_prompt(prompt);

    // Use environment variable if API key not provided
    std::string actual_api_key{api_key};
    if (actual_api_key.empty())
    {
        const char* env_key = std::getenv("OPENAI_API_KEY");
        if (!env_key)
        {
            throw ConfigurationException(
                "API key must be provided via parameter or OPENAI_API_KEY environment variable");
        }
        actual_api_key = env_key;
    }

    // Validate API key
    validate_api_key(actual_api_key);

    // Use default values if not provided
    std::string actual_system_prompt{system_prompt.empty() ? DEFAULT_SYSTEM_PROMPT : system_prompt};
    std::string actual_model{model.empty() ? DEFAULT_MODEL : model};

    // Validate system prompt if provided
    if (!system_prompt.empty())
    {
        if (system_prompt.length() > MAX_PROMPT_LENGTH)
        {
            throw ValidationException("System prompt exceeds maximum allowed length");
        }
    }

    // Setup headers for the POST request
    httplib::Headers headers = {{std::string(AUTHORIZATION_HEADER), "Bearer " + actual_api_key},
                                {std::string(CONTENT_TYPE_HEADER), std::string(APPLICATION_JSON)}};

    // Prepare the JSON data for the POST request
    json data = {{std::string(MODEL_KEY), actual_model},
                 {std::string(MESSAGES_KEY),
                  {{{std::string(ROLE_KEY), std::string(SYSTEM_ROLE)},
                    {std::string(CONTENT_KEY), actual_system_prompt}},
                   {{std::string(ROLE_KEY), std::string(USER_ROLE)},
                    {std::string(CONTENT_KEY), std::string(prompt)}}}}};

    // Initialize the HTTP client with security settings
    httplib::Client cli{std::string(SERVER_URL)};

    // Set connection and read timeouts using security constants
    cli.set_connection_timeout(CONNECTION_TIMEOUT_SECONDS, 0);
    cli.set_read_timeout(READ_TIMEOUT_SECONDS, 0);

    // Enable certificate verification for security
    cli.enable_server_certificate_verification(true);

    // Log the request safely (without exposing API key)
    gLogger->debug("Debug: Sending POST request to {} with API key: {}", API_URL,
                   redact_api_key(actual_api_key));

    // Send the POST request
    auto res = cli.Post(std::string(API_URL), headers, data.dump(), std::string(APPLICATION_JSON));

    // Check if response was received
    if (!res)
    {
        throw NetworkException("Failed to connect to OpenAI API - check network connection");
    }

    gLogger->debug("Debug: Received HTTP response with status {} and body: {}", res->status,
                   res->body);

    // Handle HTTP response status codes
    const auto status = static_cast<HttpStatus>(res->status);
    switch (status)
    {
    case HttpStatus::OK:
        // Success - continue processing
        break;
    case HttpStatus::BAD_REQUEST:
        throw ApiException(status, "Bad request - check your input parameters");
    case HttpStatus::UNAUTHORIZED:
        throw ApiException(status, "Unauthorized - check your API key");
    case HttpStatus::FORBIDDEN:
        throw ApiException(status, "Forbidden - insufficient permissions");
    case HttpStatus::NOT_FOUND:
        throw ApiException(status, "API endpoint not found");
    case HttpStatus::INTERNAL_SERVER_ERROR:
        throw ApiException(status, "OpenAI server error - try again later");
    default:
        throw ApiException(status, "Unexpected HTTP status code: " + std::to_string(res->status));
    }

    // Validate response size to prevent DoS attacks
    if (res->body.length() > MAX_RESPONSE_LENGTH)
    {
        throw ValidationException("Response exceeds maximum allowed size");
    }

    // Parse and validate response body
    if (res->body.empty())
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE, "Received empty response from API");
    }

    json res_json;
    try
    {
        res_json = json::parse(res->body);
    }
    catch (const json::parse_error& e)
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE,
                           "Invalid JSON response: " + std::string(e.what()));
    }

    // Validate response structure
    const std::string choices_key{CHOICES_KEY};
    const std::string content_key{CONTENT_KEY};
    const std::string finish_reason_key{FINISH_REASON_KEY};

    if (!res_json.contains(choices_key) || res_json[choices_key].empty())
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE,
                           "API response missing or empty 'choices' array");
    }

    const auto& first_choice = res_json[choices_key][0];
    if (!first_choice.contains(finish_reason_key))
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE,
                           "API response missing 'finish_reason' field");
    }

    if (!first_choice.contains("message") || !first_choice["message"].contains(content_key))
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE, "API response missing message content");
    }

    // Extract and return the response
    std::string finish_reason = first_choice[finish_reason_key].get<std::string>();
    gLogger->debug("Finish reason: {}", finish_reason);

    return first_choice["message"][content_key].get<std::string>();
}

/**
 * @brief Modern config-based API function using RAII and smart pointers
 */
std::string cmdgpt::get_gpt_chat_response(std::string_view prompt, const Config& config)
{
    // Validate configuration and input
    config.validate();
    validate_prompt(prompt);

    if (config.api_key().empty())
    {
        throw ConfigurationException("API key not configured");
    }

    // Use the legacy function for now, but with validated config
    return get_gpt_chat_response(prompt, config.api_key(), config.system_prompt(), config.model());
}