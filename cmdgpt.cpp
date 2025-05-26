/**
 * @file cmdgpt.cpp
 * @brief Implementation of cmdgpt functionality
 * @author Joern Ihlenburg
 * @date 2023-2024
 *
 * This file contains the implementation of all core cmdgpt functionality
 * including API communication, configuration management, conversation
 * handling, and output formatting.
 */

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
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

using json = nlohmann::json;
using namespace cmdgpt;
namespace fs = std::filesystem;

// ============================================================================
// Global Variables and Constants
// ============================================================================

/**
 * @brief Map of string log levels to spdlog::level::level_enum values
 *
 * Used for parsing log level strings from environment variables and config files.
 * Supports standard log levels: TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL.
 */
const std::map<std::string, spdlog::level::level_enum> log_levels = {
    {"TRACE", spdlog::level::trace}, {"DEBUG", spdlog::level::debug},
    {"INFO", spdlog::level::info},   {"WARN", spdlog::level::warn},
    {"ERROR", spdlog::level::err},   {"CRITICAL", spdlog::level::critical},
};

/**
 * @brief Global logger instance for application-wide logging
 *
 * Initialized based on configuration settings. Can write to file or console.
 * Thread-safe and supports multiple log levels for debugging.
 */
std::shared_ptr<spdlog::logger> gLogger;

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Prints the help message to the console.
 */
void cmdgpt::print_help()
{
    std::cout
        << "Usage: cmdgpt [options] [prompt]\n"
        << "Options:\n"
        << "  -h, --help              Show this help message and exit\n"
        << "  -v, --version           Print the version of the program and exit\n"
        << "  -i, --interactive       Run in interactive mode (REPL)\n"
        << "  --stream                Enable streaming responses (simulated)\n"
        << "  -f, --format FORMAT     Output format: plain, markdown, json, code\n"
        << "  -k, --api_key KEY       Set the OpenAI API key to KEY\n"
        << "  -s, --sys_prompt PROMPT Set the system prompt to PROMPT\n"
        << "  -l, --log_file FILE     Set the log file to FILE\n"
        << "  -m, --gpt_model MODEL   Set the GPT model to MODEL\n"
        << "  -L, --log_level LEVEL   Set the log level to LEVEL\n"
        << "                          (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)\n"
        << "\nCache Options:\n"
        << "  --no-cache              Disable response caching for this request\n"
        << "  --clear-cache           Clear all cached responses and exit\n"
        << "  --cache-stats           Display cache statistics and exit\n"
        << "\nToken Usage:\n"
        << "  --show-tokens           Display token usage and cost after response\n"
        << "\nprompt:\n"
        << "  The text prompt to send to the OpenAI GPT API. If not provided, the program\n"
        << "  will read from stdin (unless in interactive mode).\n"
        << "\n"
        << "  When both stdin and prompt are provided, they are combined:\n"
        << "    command | cmdgpt \"instruction\"\n"
        << "  The stdin content becomes the context, and the prompt becomes the instruction.\n"
        << "\nInteractive Mode Commands:\n"
        << "  /help     Show available commands\n"
        << "  /clear    Clear conversation history\n"
        << "  /save     Save conversation to file\n"
        << "  /load     Load conversation from file\n"
        << "  /exit     Exit interactive mode\n"
        << "\nConfiguration File:\n"
        << "  ~/.cmdgptrc    Configuration file with key=value pairs\n"
        << "\nEnvironment Variables:\n"
        << "  OPENAI_API_KEY     API key for the OpenAI GPT API\n"
        << "  OPENAI_SYS_PROMPT  System prompt for the OpenAI GPT API\n"
        << "  CMDGPT_LOG_FILE    Logfile to record messages\n"
        << "  OPENAI_GPT_MODEL   GPT model to use\n"
        << "  CMDGPT_LOG_LEVEL   Log level\n";
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
    // Only allow printable ASCII characters (32-126) to prevent:
    // - Control characters that could affect terminal output
    // - Non-ASCII characters that might cause encoding issues
    // - Potential injection attacks via special characters
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
 * @brief Parse output format from string
 */
OutputFormat cmdgpt::parse_output_format(std::string_view format)
{
    std::string fmt{format};
    std::transform(fmt.begin(), fmt.end(), fmt.begin(), ::tolower);

    if (fmt == "markdown" || fmt == "md")
        return OutputFormat::MARKDOWN;
    else if (fmt == "json")
        return OutputFormat::JSON;
    else if (fmt == "code")
        return OutputFormat::CODE;
    else
        return OutputFormat::PLAIN;
}

// ============================================================================
// Config Class Implementation
// ============================================================================

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
    system_prompt_ = prompt.empty() ? std::string(DEFAULT_SYSTEM_PROMPT) : std::string(prompt);
}

void cmdgpt::Config::set_model(std::string_view model)
{
    if (model.empty() || model.length() > 100)
    {
        throw ValidationException("Invalid model name");
    }
    model_ = std::string(model);
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
    // Load configuration from environment variables
    // Environment variables take precedence over defaults but not command-line args
    // This allows users to set defaults without modifying code or config files

    // Load API key from environment if available
    // OPENAI_API_KEY is the standard environment variable for OpenAI API authentication
    if (const char* env_key = std::getenv("OPENAI_API_KEY"))
    {
        api_key_ = env_key;
    }

    // Load system prompt from environment if available
    // OPENAI_SYS_PROMPT allows customizing AI behavior without command-line args
    if (const char* env_prompt = std::getenv("OPENAI_SYS_PROMPT"))
    {
        system_prompt_ = env_prompt;
    }

    // Load model from environment if available
    // OPENAI_GPT_MODEL allows switching models (e.g., gpt-4, gpt-3.5-turbo)
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
        static const std::map<std::string, spdlog::level::level_enum> level_map = {
            {"TRACE", spdlog::level::trace}, {"DEBUG", spdlog::level::debug},
            {"INFO", spdlog::level::info},   {"WARN", spdlog::level::warn},
            {"ERROR", spdlog::level::err},   {"CRITICAL", spdlog::level::critical},
        };

        const auto it = level_map.find(env_log_level);
        if (it != level_map.end())
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

// ============================================================================
// API Communication Functions
// ============================================================================

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
    // Connection timeout (10s): Reasonable time for establishing HTTPS connection
    // Read timeout (30s): Allows for OpenAI's processing time on complex prompts
    // These prevent indefinite hangs while accommodating normal API response times
    cli.set_connection_timeout(CONNECTION_TIMEOUT_SECONDS, 0);
    cli.set_read_timeout(READ_TIMEOUT_SECONDS, 0);

    // Enable certificate verification for security
    // This ensures we're actually talking to OpenAI's servers and not a MITM attacker
    // Certificate validation includes:
    // - Checking certificate chain to trusted root CA
    // - Verifying certificate hasn't expired
    // - Ensuring hostname matches certificate's CN/SAN
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
    // OpenAI API returns standard HTTP status codes:
    // - 2xx: Success
    // - 4xx: Client errors (bad request, auth issues, rate limits)
    // - 5xx: Server errors (OpenAI service issues)
    const auto status = static_cast<HttpStatus>(res->status);
    switch (status)
    {
    case HttpStatus::OK:
        // Success - continue processing
        break;
    case HttpStatus::BAD_REQUEST:
        // Usually means invalid model, malformed JSON, or invalid parameters
        throw ApiException(status, "Bad request - check your input parameters");
    case HttpStatus::UNAUTHORIZED:
        // Invalid or missing API key
        throw ApiException(status, "Unauthorized - check your API key");
    case HttpStatus::FORBIDDEN:
        // Account doesn't have access to the requested model or feature
        throw ApiException(status, "Forbidden - insufficient permissions");
    case HttpStatus::NOT_FOUND:
        // Wrong API endpoint or model name
        throw ApiException(status, "API endpoint not found");
    case HttpStatus::INTERNAL_SERVER_ERROR:
        // OpenAI service issue - usually temporary
        throw ApiException(status, "OpenAI server error - try again later");
    default:
        throw ApiException(status, "Unexpected HTTP status code: " + std::to_string(res->status));
    }

    // Validate response size to prevent DoS attacks
    // Large responses could consume excessive memory or indicate malicious activity
    if (res->body.length() > MAX_RESPONSE_LENGTH)
    {
        throw ValidationException("Response exceeds maximum allowed size");
    }

    // Parse and validate response body
    // Empty responses indicate server issues or network problems
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

    // Check cache if enabled
    if (config.cache_enabled())
    {
        auto& cache = get_response_cache();
        std::string cache_key = cache.generate_key(prompt, config.model(), config.system_prompt());
        
        std::string cached_response = cache.get(cache_key);
        if (!cached_response.empty())
        {
            gLogger->info("Using cached response for prompt");
            return cached_response;
        }
    }

    // Make API call
    std::string response = get_gpt_chat_response(prompt, config.api_key(), config.system_prompt(), config.model());
    
    // Store in cache if enabled
    if (config.cache_enabled() && !response.empty())
    {
        auto& cache = get_response_cache();
        std::string cache_key = cache.generate_key(prompt, config.model(), config.system_prompt());
        cache.put(cache_key, response);
    }
    
    return response;
}

// ============================================================================
// Conversation Class Implementation
// ============================================================================

void cmdgpt::Conversation::add_message(std::string_view role, std::string_view content)
{
    messages_.emplace_back(role, content);

    // Trim conversation if it gets too long
    // This implements a sliding window approach to maintain conversation context
    // within the model's token limits while preserving the most important messages:
    // 1. Always keep the system message (index 0) for consistent behavior
    // 2. Keep at least one user message to maintain conversation continuity
    // 3. Remove messages from oldest to newest (FIFO) starting after system message
    while (estimate_tokens() > MAX_CONTEXT_LENGTH)
    {
        if (messages_.size() <= 2) // Keep at least system + one user message
            break;
        messages_.erase(messages_.begin() + 1); // Remove oldest non-system message
    }
}

void cmdgpt::Conversation::clear()
{
    messages_.clear();
}

void cmdgpt::Conversation::save_to_file(const fs::path& path) const
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for writing: " + path.string());
    }

    file << to_json();
}

void cmdgpt::Conversation::load_from_file(const fs::path& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for reading: " + path.string());
    }

    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try
    {
        json j = json::parse(content);
        messages_.clear();

        for (const auto& msg : j["messages"])
        {
            messages_.emplace_back(msg["role"].get<std::string>(),
                                   msg["content"].get<std::string>());
        }
    }
    catch (const json::exception& e)
    {
        throw std::runtime_error("Failed to parse conversation file: " + std::string(e.what()));
    }
}

std::string cmdgpt::Conversation::to_json() const
{
    json j;
    j["messages"] = json::array();

    for (const auto& msg : messages_)
    {
        j["messages"].push_back({{"role", msg.role}, {"content", msg.content}});
    }

    return j.dump(2);
}

size_t cmdgpt::Conversation::estimate_tokens() const
{
    size_t total = 0;
    for (const auto& msg : messages_)
    {
        // Rough estimate: 1 token ~= 4 characters
        // This is based on OpenAI's guidance that on average, 1 token represents
        // approximately 4 characters of English text. This is a conservative
        // estimate that works well for English but may vary for other languages.
        // For more accurate token counting, consider using tiktoken library.
        total += (msg.role.length() + msg.content.length()) / 4;
    }
    return total;
}

// ============================================================================
// ConfigFile Class Implementation
// ============================================================================

bool cmdgpt::ConfigFile::load(const fs::path& path)
{
    if (!fs::exists(path))
        return false;

    std::ifstream file(path);
    if (!file)
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#')
            continue;

        // Parse key=value pairs
        size_t pos = line.find('=');
        if (pos != std::string::npos)
        {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            values_[key] = value;
        }
    }

    return true;
}

void cmdgpt::ConfigFile::save(const fs::path& path) const
{
    std::ofstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open config file for writing: " + path.string());
    }

    file << "# cmdgpt configuration file\n";
    file << "# Generated by cmdgpt " << VERSION << "\n\n";

    for (const auto& [key, value] : values_)
    {
        file << key << "=" << value << "\n";
    }
}

void cmdgpt::ConfigFile::apply_to(Config& config) const
{
    // Helper lambda to safely retrieve config values
    // Returns std::optional to handle missing keys gracefully
    auto get_value = [this](const std::string& key) -> std::optional<std::string>
    {
        auto it = values_.find(key);
        return it != values_.end() ? std::optional<std::string>(it->second) : std::nullopt;
    };

    if (auto val = get_value("api_key"))
        config.set_api_key(*val);
    if (auto val = get_value("system_prompt"))
        config.set_system_prompt(*val);
    if (auto val = get_value("model"))
        config.set_model(*val);
    if (auto val = get_value("log_file"))
        config.set_log_file(*val);
    if (auto val = get_value("log_level"))
    {
        auto it = log_levels.find(*val);
        if (it != log_levels.end())
            config.set_log_level(it->second);
    }
}

fs::path cmdgpt::ConfigFile::get_default_path()
{
    const char* home = std::getenv("HOME");
    if (!home)
    {
        throw std::runtime_error("HOME environment variable not set");
    }

    return fs::path(home) / ".cmdgptrc";
}

bool cmdgpt::ConfigFile::exists()
{
    return fs::exists(get_default_path());
}

/**
 * @brief Send chat request with conversation history
 */
std::string cmdgpt::get_gpt_chat_response(const Conversation& conversation, const Config& config)
{
    // Validate configuration
    config.validate();

    if (config.api_key().empty())
    {
        throw ConfigurationException("API key not configured");
    }

    // Build messages array from conversation
    json messages = json::array();
    for (const auto& msg : conversation.get_messages())
    {
        messages.push_back(
            {{std::string(ROLE_KEY), msg.role}, {std::string(CONTENT_KEY), msg.content}});
    }

    // Setup headers for the POST request
    httplib::Headers headers = {{std::string(AUTHORIZATION_HEADER), "Bearer " + config.api_key()},
                                {std::string(CONTENT_TYPE_HEADER), std::string(APPLICATION_JSON)}};

    // Create the JSON payload
    json data = {{std::string(MODEL_KEY), config.model()}, {std::string(MESSAGES_KEY), messages}};

    // Initialize the HTTP client with security settings
    httplib::Client cli{std::string(SERVER_URL)};

    // Set connection and read timeouts
    cli.set_connection_timeout(CONNECTION_TIMEOUT_SECONDS, 0);
    cli.set_read_timeout(READ_TIMEOUT_SECONDS, 0);

    // Enable certificate verification for security (same as above)
    cli.enable_server_certificate_verification(true);

    // Log the request safely
    gLogger->debug("Debug: Sending conversation request with {} messages", messages.size());

    // Send the POST request
    auto res = cli.Post(std::string(API_URL), headers, data.dump(), std::string(APPLICATION_JSON));

    // Check if response was received
    if (!res)
    {
        throw NetworkException("Failed to connect to OpenAI API - check network connection");
    }

    // Handle HTTP response status codes
    const auto status = static_cast<HttpStatus>(res->status);
    if (status != HttpStatus::OK)
    {
        // Construct error message with fallback handling
        std::string error_msg = "HTTP " + std::to_string(res->status);
        if (!res->body.empty())
        {
            try
            {
                // Try to extract structured error message from OpenAI's error response
                json error_json = json::parse(res->body);
                if (error_json.contains("error") && error_json["error"].contains("message"))
                {
                    error_msg += ": " + error_json["error"]["message"].get<std::string>();
                }
            }
            catch (...)
            {
                // If JSON parsing fails, use generic HTTP status message
                // This handles cases where the error response isn't valid JSON
            }
        }
        throw ApiException(status, error_msg);
    }

    // Parse response
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

    // Extract response content
    if (!res_json.contains(CHOICES_KEY) || res_json[CHOICES_KEY].empty())
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE, "No choices in API response");
    }

    const auto& first_choice = res_json[CHOICES_KEY][0];
    if (!first_choice.contains("message") || !first_choice["message"].contains(CONTENT_KEY))
    {
        throw ApiException(HttpStatus::EMPTY_RESPONSE, "No content in API response");
    }

    return first_choice["message"][CONTENT_KEY].get<std::string>();
}

// ============================================================================
// Retry Helper Functions
// ============================================================================

/**
 * @brief Execute a function with exponential backoff retry logic
 *
 * @tparam Func The function type to execute
 * @tparam Args The argument types for the function
 * @param func The function to execute
 * @param max_retries Maximum number of retry attempts
 * @param initial_delay_ms Initial delay in milliseconds
 * @param args Arguments to pass to the function
 * @return The result of the function call
 *
 * @throws The last exception if all retries fail
 *
 * @note Retries on network errors and rate limits (429)
 * @note Uses exponential backoff: delay doubles after each retry
 * @note Maximum delay is capped at 30 seconds
 */
template <typename Func, typename... Args>
auto retry_with_backoff(Func func, int max_retries, int initial_delay_ms, Args&&... args)
    -> decltype(func(std::forward<Args>(args)...))
{
    int delay_ms = initial_delay_ms;
    const int max_delay_ms = 30000; // 30 seconds max

    for (int attempt = 0; attempt <= max_retries; ++attempt)
    {
        try
        {
            return func(std::forward<Args>(args)...);
        }
        catch (const cmdgpt::ApiException& e)
        {
            // Only retry on rate limits and server errors
            if (e.status_code() != cmdgpt::HttpStatus::TOO_MANY_REQUESTS &&
                e.status_code() != cmdgpt::HttpStatus::INTERNAL_SERVER_ERROR)
            {
                throw; // Don't retry client errors
            }

            if (attempt == max_retries)
            {
                throw; // Last attempt failed
            }

            gLogger->warn("Request failed (attempt {}/{}): {}. Retrying in {} ms...", attempt + 1,
                          max_retries + 1, e.what(), delay_ms);

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

            // Exponential backoff with jitter
            delay_ms = std::min(delay_ms * 2 + (rand() % 100), max_delay_ms);
        }
        catch (const cmdgpt::NetworkException& e)
        {
            if (attempt == max_retries)
            {
                throw; // Last attempt failed
            }

            gLogger->warn("Network error (attempt {}/{}): {}. Retrying in {} ms...", attempt + 1,
                          max_retries + 1, e.what(), delay_ms);

            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

            // Exponential backoff with jitter
            delay_ms = std::min(delay_ms * 2 + (rand() % 100), max_delay_ms);
        }
    }

    // Should never reach here
    throw std::logic_error("Retry logic error");
}

// ============================================================================
// Streaming Helper Functions
// ============================================================================

/**
 * @brief Process a single Server-Sent Events (SSE) line from streaming response
 *
 * @param line The SSE line to process
 * @param callback The callback to invoke with extracted content
 * @return true if processing was successful, false otherwise
 *
 * @note SSE format: "data: {json}" or "data: [DONE]"
 * @note This function is designed to be secure against malformed JSON
 */
static bool process_sse_line(const std::string& line, const cmdgpt::StreamCallback& callback)
{
    // Skip empty lines (SSE spec allows empty lines between events)
    if (line.empty() || line == "\r")
        return true;

    // Check for SSE data prefix (6 chars: "data: ")
    if (line.length() < 6 || line.substr(0, 6) != "data: ")
        return true; // Not an error, just not a data line

    std::string data_str = line.substr(6);

    // Check for stream termination marker
    if (data_str == "[DONE]")
        return true;

    try
    {
        // Parse JSON chunk with security considerations:
        // - nlohmann::json throws on invalid JSON (prevents injection)
        // - We only extract specific expected fields
        json chunk_json = json::parse(data_str);

        // Validate expected structure to prevent unexpected data processing
        if (chunk_json.contains("choices") && chunk_json["choices"].is_array() &&
            !chunk_json["choices"].empty())
        {
            const auto& choice = chunk_json["choices"][0];

            // Check for delta content (streaming responses use "delta" not "message")
            if (choice.contains("delta") && choice["delta"].is_object() &&
                choice["delta"].contains("content") && choice["delta"]["content"].is_string())
            {
                std::string content = choice["delta"]["content"].get<std::string>();
                if (!content.empty())
                {
                    callback(content);
                }
            }
        }
    }
    catch (const json::exception& e)
    {
        // Log but don't throw - streaming can have partial/malformed chunks
        gLogger->warn("Failed to parse streaming chunk: {}", e.what());
        return false;
    }

    return true;
}

/**
 * @brief Process streaming response data with buffering for partial lines
 *
 * @param data Raw data received from server
 * @param data_length Length of the data
 * @param buffer Persistent buffer for accumulating partial lines
 * @param callback The callback to invoke with extracted content
 *
 * @note This function handles partial JSON objects that may span multiple chunks
 * @note Security: Limits buffer size to prevent memory exhaustion attacks
 */
static void process_streaming_data(const char* data, size_t data_length, std::string& buffer,
                                   const cmdgpt::StreamCallback& callback)
{
    // Security: Limit buffer size to prevent memory exhaustion
    const size_t MAX_BUFFER_SIZE = 1024 * 1024; // 1MB max buffer

    if (buffer.length() + data_length > MAX_BUFFER_SIZE)
    {
        gLogger->error("Streaming buffer size exceeded maximum allowed");
        buffer.clear();
        throw cmdgpt::ValidationException("Streaming response too large");
    }

    // Append new data to buffer
    buffer.append(data, data_length);

    // Process complete lines
    size_t start = 0;
    size_t pos;

    while ((pos = buffer.find('\n', start)) != std::string::npos)
    {
        std::string line = buffer.substr(start, pos - start);
        start = pos + 1;

        process_sse_line(line, callback);
    }

    // Keep remaining partial line in buffer
    if (start < buffer.length())
    {
        buffer = buffer.substr(start);
    }
    else
    {
        buffer.clear();
    }
}

/**
 * @brief Handle streaming API error responses
 *
 * @param status The HTTP status code
 * @param body The response body (may contain error details)
 *
 * @throws ApiException with appropriate error message
 */
static void handle_streaming_error(cmdgpt::HttpStatus status, const std::string& body)
{
    std::string error_msg = "Unknown error";

    // Try to extract error message from JSON response
    try
    {
        if (!body.empty())
        {
            json error_json = json::parse(body);
            if (error_json.contains("error") && error_json["error"].is_object() &&
                error_json["error"].contains("message") &&
                error_json["error"]["message"].is_string())
            {
                error_msg = error_json["error"]["message"].get<std::string>();
            }
        }
    }
    catch (...)
    {
        // If JSON parsing fails, use generic message
    }

    // Throw appropriate exception based on status code
    switch (status)
    {
    case cmdgpt::HttpStatus::BAD_REQUEST:
        throw cmdgpt::ApiException(status, "Bad request: " + error_msg);
    case cmdgpt::HttpStatus::UNAUTHORIZED:
        throw cmdgpt::ApiException(status, "Unauthorized - check your API key");
    case cmdgpt::HttpStatus::FORBIDDEN:
        throw cmdgpt::ApiException(status, "Forbidden - insufficient permissions");
    case cmdgpt::HttpStatus::NOT_FOUND:
        throw cmdgpt::ApiException(status, "API endpoint not found");
    case cmdgpt::HttpStatus::TOO_MANY_REQUESTS:
        throw cmdgpt::ApiException(status, "Rate limit exceeded - try again later");
    case cmdgpt::HttpStatus::INTERNAL_SERVER_ERROR:
        throw cmdgpt::ApiException(status, "OpenAI server error - try again later");
    default:
        throw cmdgpt::ApiException(
            status, "HTTP error " + std::to_string(static_cast<int>(status)) + ": " + error_msg);
    }
}

// ============================================================================
// Streaming API Functions
// ============================================================================

/**
 * @brief Send streaming chat request with prompt
 *
 * @param prompt The user's input prompt
 * @param config Configuration object containing API settings
 * @param callback Function called for each response chunk
 *
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 *
 * @note This function simulates streaming by making a regular request
 *       and then calling the callback with chunks of the response.
 *       True SSE streaming requires a different HTTP client library.
 * @note Security: Validates all inputs and enforces size limits
 */
void cmdgpt::get_gpt_chat_response_stream(std::string_view prompt, const Config& config,
                                          StreamCallback callback)
{
    // Validate configuration and input
    config.validate();
    validate_prompt(prompt);

    if (!callback)
    {
        throw ValidationException("Stream callback function is required");
    }

    // For now, we'll use non-streaming API and simulate streaming output
    // True SSE streaming would require a different approach with cpp-httplib
    // or switching to a library that supports SSE out of the box

    try
    {
        // Get the complete response
        std::string response = get_gpt_chat_response(prompt, config);

        // Simulate streaming by sending the response in chunks
        // This provides a better UX even without true streaming
        const size_t chunk_size = 20; // Characters per chunk
        for (size_t i = 0; i < response.length(); i += chunk_size)
        {
            size_t len = std::min(chunk_size, response.length() - i);
            callback(response.substr(i, len));

            // Small delay to simulate streaming effect
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    catch (const std::exception& e)
    {
        // Re-throw with same type
        throw;
    }
}

/**
 * @brief Send streaming chat request with conversation history
 *
 * @param conversation The conversation history
 * @param config Configuration object containing API settings
 * @param callback Function called for each response chunk
 *
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 *
 * @note Simulates streaming responses based on full conversation context
 * @note Security: Validates conversation size to prevent abuse
 */
void cmdgpt::get_gpt_chat_response_stream(const Conversation& conversation, const Config& config,
                                          StreamCallback callback)
{
    // Validate inputs
    config.validate();

    if (!callback)
    {
        throw ValidationException("Stream callback function is required");
    }

    if (conversation.get_messages().empty())
    {
        throw ValidationException("Conversation cannot be empty");
    }

    // Security: Check conversation size to prevent token limit abuse
    // Using a reasonable token limit (4096 tokens is typical for many models)
    const size_t MAX_TOKENS = 4096;
    if (conversation.estimate_tokens() > MAX_TOKENS)
    {
        throw ValidationException("Conversation exceeds maximum token limit");
    }

    // For now, we'll use non-streaming API and simulate streaming output
    // True SSE streaming would require a different approach with cpp-httplib
    // or switching to a library that supports SSE out of the box

    try
    {
        // Get the complete response
        std::string response = get_gpt_chat_response(conversation, config);

        // Simulate streaming by sending the response in chunks
        // This provides a better UX even without true streaming
        const size_t chunk_size = 20; // Characters per chunk
        for (size_t i = 0; i < response.length(); i += chunk_size)
        {
            size_t len = std::min(chunk_size, response.length() - i);
            callback(response.substr(i, len));

            // Small delay to simulate streaming effect
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }
    catch (const std::exception& e)
    {
        // Re-throw with same type
        throw;
    }
}

// ============================================================================
// Output Formatting Functions
// ============================================================================

std::string cmdgpt::format_output(const std::string& content, OutputFormat format)
{
    switch (format)
    {
    case OutputFormat::JSON:
    {
        json j;
        j["response"] = content;
        j["timestamp"] = std::time(nullptr);
        j["version"] = VERSION;
        return j.dump(2);
    }

    case OutputFormat::MARKDOWN:
    {
        // Basic markdown formatting
        return "## Response\n\n" + content + "\n\n---\n*Generated by cmdgpt " +
               std::string(VERSION) + "*\n";
    }

    case OutputFormat::CODE:
    {
        // Extract code blocks if present
        // Regex pattern explanation:
        // ``` - Match opening triple backticks
        // (?:\w+)? - Optional language identifier (e.g., python, cpp)
        // \n - Newline after language identifier
        // ([^`]+) - Capture group 1: code content (any char except backtick)
        // ``` - Match closing triple backticks
        std::regex code_regex(R"(```(?:\w+)?\n([^`]+)```)");
        std::smatch match;
        if (std::regex_search(content, match, code_regex))
        {
            return match[1].str();
        }
        return content;
    }

    case OutputFormat::PLAIN:
    default:
        return content;
    }
}

// ============================================================================
// Interactive Mode
// ============================================================================

/**
 * @brief Run interactive REPL mode
 */
void cmdgpt::run_interactive_mode(const Config& config)
{
    std::cout << "cmdgpt " << VERSION << " - Interactive Mode\n";
    std::cout << "Type '/help' for commands, '/exit' to quit\n";

    Conversation conversation;

    // Check for recovery file
    const std::string recovery_file = ".cmdgpt_recovery.json";
    if (fs::exists(recovery_file))
    {
        std::cout << "\nRecovery file found from previous session.\n";
        std::cout << "Load it? (y/n): ";
        std::string response;
        if (std::getline(std::cin, response) && (response == "y" || response == "Y"))
        {
            try
            {
                conversation.load_from_file(recovery_file);
                std::cout << "Previous conversation restored.\n";
                // Remove recovery file after successful load
                fs::remove(recovery_file);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Failed to load recovery file: " << e.what() << "\n";
            }
        }
    }

    std::cout << "\n";

    // Add system prompt to conversation if not already present
    if (!config.system_prompt().empty() &&
        (conversation.get_messages().empty() || conversation.get_messages()[0].role != SYSTEM_ROLE))
    {
        conversation.add_message(SYSTEM_ROLE, config.system_prompt());
    }

    std::string line;
    while (true)
    {
        std::cout << "> ";
        if (!std::getline(std::cin, line))
            break;

        // Trim whitespace from both ends of input
        // This prevents issues with accidental spaces and ensures clean command parsing
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty())
            continue;

        // Handle special commands starting with '/'
        // Commands provide control over the interactive session
        if (line[0] == '/')
        {
            // Exit commands - terminate the interactive session
            if (line == "/exit" || line == "/quit")
            {
                break;
            }
            // Clear command - reset conversation history but keep system prompt
            else if (line == "/clear")
            {
                conversation.clear();
                // Re-add system prompt to maintain consistent AI behavior
                if (!config.system_prompt().empty())
                {
                    conversation.add_message(SYSTEM_ROLE, config.system_prompt());
                }
                std::cout << "Conversation cleared.\n";
                continue;
            }
            // Help command - display available commands
            else if (line == "/help")
            {
                std::cout << "Available commands:\n";
                std::cout << "  /help     - Show this help message\n";
                std::cout << "  /clear    - Clear conversation history\n";
                std::cout << "  /save     - Save conversation to file\n";
                std::cout << "  /load     - Load conversation from file\n";
                std::cout << "  /exit     - Exit interactive mode\n";
                continue;
            }
            // Save command - persist conversation to JSON file
            // Usage: /save [filename] (defaults to conversation.json)
            else if (line.substr(0, 5) == "/save")
            {
                std::string filename = "conversation.json";
                if (line.length() > 6)
                {
                    filename = line.substr(6);
                }
                try
                {
                    conversation.save_to_file(filename);
                    std::cout << "Conversation saved to " << filename << "\n";
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Error saving conversation: " << e.what() << "\n";
                }
                continue;
            }
            // Load command - restore conversation from JSON file
            // Usage: /load [filename] (defaults to conversation.json)
            else if (line.substr(0, 5) == "/load")
            {
                std::string filename = "conversation.json";
                if (line.length() > 6)
                {
                    filename = line.substr(6);
                }
                try
                {
                    conversation.load_from_file(filename);
                    std::cout << "Conversation loaded from " << filename << "\n";
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Error loading conversation: " << e.what() << "\n";
                }
                continue;
            }
            else
            {
                // Unknown command - guide user to help
                std::cout << "Unknown command. Type '/help' for available commands.\n";
                continue;
            }
        }

        // Add user message to conversation
        conversation.add_message(USER_ROLE, line);

        // Get response
        try
        {
            std::cout << "\n";

            if (config.streaming_mode())
            {
                // Streaming mode - output chunks as they arrive
                std::string full_response;

                get_gpt_chat_response_stream(conversation, config,
                                             [&full_response](const std::string& chunk)
                                             {
                                                 std::cout << chunk << std::flush;
                                                 full_response += chunk;
                                             });

                std::cout << "\n\n";

                // Add complete response to conversation history
                conversation.add_message("assistant", full_response);
            }
            else
            {
                // Non-streaming mode - wait for complete response with retry
                std::string response = get_gpt_chat_response_with_retry(conversation, config);

                // Add assistant response to conversation
                conversation.add_message("assistant", response);

                // Display response
                std::cout << response << "\n\n";
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << "\n";

            // Auto-save conversation on error for recovery
            try
            {
                const std::string error_recovery_file = ".cmdgpt_recovery.json";
                conversation.save_to_file(error_recovery_file);
                std::cerr << "Conversation saved to " << error_recovery_file << " for recovery.\n";
                std::cerr << "Use '/load " << error_recovery_file << "' to restore.\n";
            }
            catch (const std::exception& save_error)
            {
                std::cerr << "Failed to save recovery file: " << save_error.what() << "\n";
            }

            std::cerr << "\n";
        }
    }

    std::cout << "\nGoodbye!\n";
}

// ============================================================================
// Retry-Enabled API Functions
// ============================================================================

/**
 * @brief Send chat request with automatic retry on failure
 */
std::string cmdgpt::get_gpt_chat_response_with_retry(std::string_view prompt, const Config& config,
                                                     int max_retries)
{
    // Use lambda to wrap the API call for retry logic
    auto api_call = [&prompt, &config]() { return get_gpt_chat_response(prompt, config); };

    // Initial delay of 1 second, doubles on each retry
    return retry_with_backoff(api_call, max_retries, 1000);
}

/**
 * @brief Send chat request with conversation history and automatic retry
 */
std::string cmdgpt::get_gpt_chat_response_with_retry(const Conversation& conversation,
                                                     const Config& config, int max_retries)
{
    // Use lambda to wrap the API call for retry logic
    auto api_call = [&conversation, &config]()
    { return get_gpt_chat_response(conversation, config); };

    // Initial delay of 1 second, doubles on each retry
    return retry_with_backoff(api_call, max_retries, 1000);
}

// ============================================================================
// Cache Implementation
// ============================================================================

#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <ctime>

/**
 * @brief ResponseCache constructor
 */
cmdgpt::ResponseCache::ResponseCache(const std::filesystem::path& cache_dir, size_t expiration_hours)
    : expiration_hours_(expiration_hours)
{
    if (cache_dir.empty())
    {
        // Default to ~/.cmdgpt/cache/
        const char* home = std::getenv("HOME");
        if (!home)
        {
            throw ConfigurationException("HOME environment variable not set");
        }
        cache_dir_ = std::filesystem::path(home) / ".cmdgpt" / "cache";
    }
    else
    {
        cache_dir_ = cache_dir;
    }

    // Create cache directory if it doesn't exist
    std::filesystem::create_directories(cache_dir_);
}

/**
 * @brief Generate SHA256 hash key from request parameters
 */
std::string cmdgpt::ResponseCache::generate_key(std::string_view prompt, std::string_view model,
                                               std::string_view system_prompt) const
{
    // Combine all parameters
    std::string combined = std::string(prompt) + "|" + std::string(model) + "|" + std::string(system_prompt);
    
    // Calculate SHA256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(combined.c_str()), combined.length(), hash);
    
    // Convert to hex string
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

/**
 * @brief Get cache file path for a key
 */
std::filesystem::path cmdgpt::ResponseCache::get_cache_path(const std::string& key) const
{
    return cache_dir_ / (key + ".json");
}

/**
 * @brief Check if cache file is expired
 */
bool cmdgpt::ResponseCache::is_expired(const std::filesystem::path& path) const
{
    auto file_time = std::filesystem::last_write_time(path);
    auto now = std::filesystem::file_time_type::clock::now();
    auto age = std::chrono::duration_cast<std::chrono::hours>(now - file_time);
    
    return age.count() >= static_cast<long>(expiration_hours_);
}

/**
 * @brief Check if valid cache exists
 */
bool cmdgpt::ResponseCache::has_valid_cache(const std::string& key) const
{
    auto path = get_cache_path(key);
    
    if (!std::filesystem::exists(path))
    {
        return false;
    }
    
    return !is_expired(path);
}

/**
 * @brief Get cached response
 */
std::string cmdgpt::ResponseCache::get(const std::string& key) const
{
    auto path = get_cache_path(key);
    
    if (!has_valid_cache(key))
    {
        cache_misses_++;
        return "";
    }
    
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            cache_misses_++;
            return "";
        }
        
        // Read JSON and extract response
        nlohmann::json cache_data;
        file >> cache_data;
        
        cache_hits_++;
        return cache_data["response"].get<std::string>();
    }
    catch (const std::exception& e)
    {
        gLogger->warn("Failed to read cache {}: {}", key, e.what());
        cache_misses_++;
        return "";
    }
}

/**
 * @brief Store response in cache
 */
void cmdgpt::ResponseCache::put(const std::string& key, std::string_view response)
{
    auto path = get_cache_path(key);
    
    try
    {
        // Create cache entry with metadata
        nlohmann::json cache_data;
        cache_data["response"] = response;
        cache_data["timestamp"] = std::time(nullptr);
        cache_data["version"] = VERSION;
        
        std::ofstream file(path);
        if (file.is_open())
        {
            file << cache_data.dump(2);
            gLogger->debug("Cached response with key: {}", key);
        }
    }
    catch (const std::exception& e)
    {
        gLogger->warn("Failed to write cache {}: {}", key, e.what());
    }
}

/**
 * @brief Clear all cache entries
 */
size_t cmdgpt::ResponseCache::clear()
{
    size_t count = 0;
    
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(cache_dir_))
        {
            if (entry.path().extension() == ".json")
            {
                std::filesystem::remove(entry.path());
                count++;
            }
        }
    }
    catch (const std::exception& e)
    {
        gLogger->warn("Failed to clear cache: {}", e.what());
    }
    
    return count;
}

/**
 * @brief Clean expired cache entries
 */
size_t cmdgpt::ResponseCache::clean_expired()
{
    size_t count = 0;
    
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(cache_dir_))
        {
            if (entry.path().extension() == ".json" && is_expired(entry.path()))
            {
                std::filesystem::remove(entry.path());
                count++;
            }
        }
    }
    catch (const std::exception& e)
    {
        gLogger->warn("Failed to clean cache: {}", e.what());
    }
    
    return count;
}

/**
 * @brief Get cache statistics
 */
std::map<std::string, size_t> cmdgpt::ResponseCache::get_stats() const
{
    std::map<std::string, size_t> stats;
    stats["hits"] = cache_hits_;
    stats["misses"] = cache_misses_;
    
    size_t count = 0;
    size_t total_size = 0;
    
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(cache_dir_))
        {
            if (entry.path().extension() == ".json")
            {
                count++;
                total_size += std::filesystem::file_size(entry.path());
            }
        }
    }
    catch (const std::exception& e)
    {
        gLogger->warn("Failed to get cache stats: {}", e.what());
    }
    
    stats["count"] = count;
    stats["size_bytes"] = total_size;
    
    return stats;
}

/**
 * @brief Get global cache instance
 */
cmdgpt::ResponseCache& cmdgpt::get_response_cache()
{
    static ResponseCache cache;
    return cache;
}

// ============================================================================
// Token Usage Implementation
// ============================================================================

/**
 * @brief Model pricing per 1K tokens (as of 2024)
 */
static const std::map<std::string, std::pair<double, double>> MODEL_PRICING = {
    {"gpt-4", {0.03, 0.06}},           // input, output per 1K tokens
    {"gpt-4-turbo-preview", {0.01, 0.03}},
    {"gpt-3.5-turbo", {0.0005, 0.0015}},
    {"gpt-3.5-turbo-16k", {0.003, 0.004}}
};

/**
 * @brief Parse token usage from API response
 */
cmdgpt::TokenUsage cmdgpt::parse_token_usage(const std::string& response_json, std::string_view model)
{
    TokenUsage usage;
    
    try
    {
        auto json = nlohmann::json::parse(response_json);
        
        if (json.contains("usage"))
        {
            auto& usage_json = json["usage"];
            usage.prompt_tokens = usage_json.value("prompt_tokens", 0);
            usage.completion_tokens = usage_json.value("completion_tokens", 0);
            usage.total_tokens = usage_json.value("total_tokens", 0);
            
            // Calculate cost if model pricing is known
            std::string model_str(model);
            if (MODEL_PRICING.count(model_str) > 0)
            {
                const auto& pricing = MODEL_PRICING.at(model_str);
                usage.estimated_cost = (usage.prompt_tokens * pricing.first + 
                                      usage.completion_tokens * pricing.second) / 1000.0;
            }
        }
    }
    catch (const std::exception& e)
    {
        gLogger->debug("Failed to parse token usage: {}", e.what());
    }
    
    return usage;
}

/**
 * @brief Format token usage for display
 */
std::string cmdgpt::format_token_usage(const TokenUsage& usage)
{
    std::stringstream ss;
    ss << "Token Usage: " << usage.total_tokens << " total "
       << "(" << usage.prompt_tokens << " prompt + "
       << usage.completion_tokens << " completion)";
    
    if (usage.estimated_cost > 0)
    {
        ss << std::fixed << std::setprecision(4)
           << " ~$" << usage.estimated_cost << " USD";
    }
    
    return ss.str();
}