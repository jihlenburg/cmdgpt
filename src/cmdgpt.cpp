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
#include "base64.h"
#include "file_utils.h"
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
#include <set>
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
        << "\nImage Support:\n"
        << "  -I, --image PATH        Attach an image file for Vision API analysis\n"
        << "  --images PATH1,PATH2    Attach multiple images (comma-separated)\n"
        << "  --generate-image        Generate an image using DALL-E instead of chat\n"
        << "  --image-size SIZE       Image size for generation (default: 1024x1024)\n"
        << "                          Options: 1024x1024, 1792x1024, 1024x1792\n"
        << "  --image-quality QUAL    Image quality: standard, hd (default: standard)\n"
        << "  --image-style STYLE     Image style: vivid, natural (default: vivid)\n"
        << "  --save-images           Save generated images to disk\n"
        << "\nCustom Endpoints:\n"
        << "  --endpoint URL          Use custom API endpoint (e.g., for local models)\n"
        << "\nResponse History:\n"
        << "  --history               Show recent history (last 10 entries)\n"
        << "  --clear-history         Clear all history entries\n"
        << "  --search-history QUERY  Search history by prompt content\n"
        << "\nTemplate System:\n"
        << "  --list-templates        List available prompt templates\n"
        << "  --template NAME [VARS]  Use a template with variable substitution\n"
        << "    Example: cmdgpt --template code-review \"$(cat main.cpp)\"\n"
        << "    Example: cmdgpt --template refactor \"$(cat utils.js)\" \"modularity\"\n"
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

void cmdgpt::Config::set_endpoint(std::string_view endpoint)
{
    if (endpoint.empty())
    {
        endpoint_.clear();
        return;
    }

    // Basic URL validation
    if (endpoint.length() > 4096)
    {
        throw ValidationException("Endpoint URL too long");
    }

    // Must start with http:// or https://
    if (endpoint.find("http://") != 0 && endpoint.find("https://") != 0)
    {
        throw ValidationException("Endpoint must start with http:// or https://");
    }

    endpoint_ = endpoint;
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
 * @brief Extract server URL and API path from endpoint configuration
 * @param config The configuration containing endpoint info
 * @return Pair of (server_url, api_path)
 */
static std::pair<std::string, std::string> extract_endpoint_info(const cmdgpt::Config& config)
{
    std::string server_url;
    std::string api_path;

    if (!config.endpoint().empty())
    {
        // Parse custom endpoint URL
        std::string endpoint = config.endpoint();
        size_t path_start = endpoint.find('/', 8); // Skip http:// or https://
        if (path_start != std::string::npos)
        {
            server_url = endpoint.substr(0, path_start);
            api_path = endpoint.substr(path_start);
        }
        else
        {
            server_url = endpoint;
            api_path = "/v1/chat/completions"; // Default path
        }
    }
    else
    {
        // Use default OpenAI endpoint
        server_url = std::string(cmdgpt::SERVER_URL);
        api_path = std::string(cmdgpt::API_URL);
    }

    return {server_url, api_path};
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

    // Apply rate limiting before making request
    get_rate_limiter().acquire();

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

            // Record in history (with empty token usage for cached responses)
            auto& history = get_response_history();
            history.add_entry(prompt, cached_response, config.model(), TokenUsage{}, true);

            return cached_response;
        }
    }

    // Make API call
    std::string response =
        get_gpt_chat_response(prompt, config.api_key(), config.system_prompt(), config.model());

    // Store in cache if enabled
    if (config.cache_enabled() && !response.empty())
    {
        auto& cache = get_response_cache();
        std::string cache_key = cache.generate_key(prompt, config.model(), config.system_prompt());
        cache.put(cache_key, response);
    }

    // Record in history
    // Note: Token usage will be parsed from the response in a future update
    auto& history = get_response_history();
    history.add_entry(prompt, response, config.model(), TokenUsage{}, false);

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

    // Apply rate limiting before making request
    get_rate_limiter().acquire();

    // Initialize the HTTP client with custom endpoint support
    auto [server_url, api_path] = extract_endpoint_info(config);
    httplib::Client cli{server_url};

    // Set connection and read timeouts
    cli.set_connection_timeout(CONNECTION_TIMEOUT_SECONDS, 0);
    cli.set_read_timeout(READ_TIMEOUT_SECONDS, 0);

    // Enable certificate verification for security (same as above)
    cli.enable_server_certificate_verification(true);

    // Log the request safely
    gLogger->debug("Debug: Sending conversation request with {} messages", messages.size());

    // Send the POST request
    auto res = cli.Post(api_path, headers, data.dump(), std::string(APPLICATION_JSON));

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

#include <cctype>
#include <ctime>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>

/**
 * @brief ResponseCache constructor
 */
cmdgpt::ResponseCache::ResponseCache(const std::filesystem::path& cache_dir,
                                     size_t expiration_hours)
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

    // Create cache directory if it doesn't exist with secure permissions
    std::filesystem::create_directories(cache_dir_);

    // Set secure permissions (owner read/write/execute only)
    try
    {
        std::filesystem::permissions(cache_dir_, std::filesystem::perms::owner_all,
                                     std::filesystem::perm_options::replace);
    }
    catch (const std::filesystem::filesystem_error& e)
    {
        // Security: Fail if we cannot set secure permissions
        throw std::runtime_error("Failed to set secure cache directory permissions: " +
                                 std::string(e.what()));
    }
}

/**
 * @brief Generate SHA256 hash key from request parameters
 *
 * Creates a unique cache key by hashing the combination of prompt,
 * model, and system prompt. This ensures identical requests can be
 * matched in the cache.
 *
 * @param prompt The user's input prompt
 * @param model The model name (e.g., "gpt-4")
 * @param system_prompt The system prompt for context
 * @return SHA256 hash as a 64-character hexadecimal string
 */
std::string cmdgpt::ResponseCache::generate_key(std::string_view prompt, std::string_view model,
                                                std::string_view system_prompt) const
{
    // Combine all parameters
    std::string combined =
        std::string(prompt) + "|" + std::string(model) + "|" + std::string(system_prompt);

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
 *
 * Validates the cache key and returns the full path to the cache file.
 * Includes security checks to prevent path traversal attacks.
 *
 * @param key The cache key (must be valid SHA256 hex string)
 * @return Full path to the cache file
 * @throws ValidationException if key contains invalid characters
 * @throws SecurityException if path traversal is detected
 */
std::filesystem::path cmdgpt::ResponseCache::get_cache_path(const std::string& key) const
{
    // Validate key contains only hex characters (SHA256 output)
    for (char c : key)
    {
        if (!std::isxdigit(c))
        {
            throw ValidationException("Invalid cache key format");
        }
    }

    // Ensure we stay within cache directory by using canonical paths
    auto cache_file = cache_dir_ / (key + ".json");

    // Resolve canonical paths to prevent symlink attacks
    std::error_code ec;
    auto canonical_cache_dir = std::filesystem::canonical(cache_dir_, ec);
    if (ec)
    {
        throw SecurityException("Failed to resolve cache directory: " + ec.message());
    }

    // For the file, we need to check its parent since the file might not exist yet
    auto cache_file_parent = cache_file.parent_path();
    auto canonical_file_parent = std::filesystem::canonical(cache_file_parent, ec);
    if (ec)
    {
        throw SecurityException("Failed to resolve cache file path: " + ec.message());
    }

    // Verify the file's parent directory is within our cache directory
    if (canonical_file_parent.string().find(canonical_cache_dir.string()) != 0)
    {
        throw SecurityException("Cache path escape attempt detected");
    }

    return cache_file;
}

/**
 * @brief Check if cache file is expired
 *
 * Determines if a cache file has exceeded the expiration time.
 *
 * @param path Path to the cache file
 * @return True if file is older than expiration_hours_, false otherwise
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
 *
 * Checks if a cache entry exists and is not expired.
 *
 * @param key The cache key to check
 * @return True if valid (non-expired) cache exists, false otherwise
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
 *
 * Retrieves a cached response if it exists and is valid.
 * Updates cache hit/miss statistics.
 *
 * @param key The cache key to look up
 * @return The cached response string, or empty string if not found/invalid
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
 *
 * Saves an API response to the cache with metadata. Enforces cache
 * size limits and uses atomic writes to prevent corruption.
 *
 * @param key The cache key for this response
 * @param response The API response text to cache
 */
void cmdgpt::ResponseCache::put(const std::string& key, std::string_view response)
{
    // Check cache size before adding new entry
    auto stats = get_stats();
    if (stats["count"] >= MAX_CACHE_ENTRIES)
    {
        gLogger->info("Cache full, cleaning expired entries");
        clean_expired();

        // If still too many entries, don't cache
        stats = get_stats();
        if (stats["count"] >= MAX_CACHE_ENTRIES)
        {
            gLogger->warn("Cache at maximum capacity, skipping cache write");
            return;
        }
    }

    if (stats["size_bytes"] > MAX_CACHE_SIZE_MB * 1024 * 1024)
    {
        gLogger->warn("Cache size limit exceeded, skipping cache write");
        return;
    }

    auto path = get_cache_path(key);

    try
    {
        // Create cache entry with metadata
        nlohmann::json cache_data;
        cache_data["response"] = response;
        cache_data["timestamp"] = std::time(nullptr);
        cache_data["version"] = VERSION;

        // Write atomically to prevent partial writes
        auto temp_path = path.string() + ".tmp";
        std::ofstream file(temp_path);
        if (file.is_open())
        {
            file << cache_data.dump(2);
            file.close();

            // Atomic rename
            std::filesystem::rename(temp_path, path);
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
 *
 * Removes all cached responses from the cache directory.
 *
 * @return Number of cache entries that were removed
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
 *
 * Removes cache files that have exceeded the expiration time.
 *
 * @return Number of expired entries that were removed
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
 *
 * Collects statistics about cache usage including hit/miss counts,
 * number of entries, and total size.
 *
 * @return Map containing statistics: "hits", "misses", "count", "size_bytes"
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
 *
 * Returns a reference to the singleton ResponseCache instance.
 * The cache is created on first access with default settings.
 *
 * @return Reference to the global ResponseCache instance
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
    {"gpt-4", {0.03, 0.06}}, // input, output per 1K tokens
    {"gpt-4-turbo-preview", {0.01, 0.03}},
    {"gpt-3.5-turbo", {0.0005, 0.0015}},
    {"gpt-3.5-turbo-16k", {0.003, 0.004}}};

/**
 * @brief Parse token usage from API response
 *
 * Extracts token usage information from the JSON response and
 * calculates the estimated cost based on model pricing.
 *
 * @param response_json The raw JSON response from the API
 * @param model The model name used for pricing calculation
 * @return TokenUsage struct with parsed values (zeros if parsing fails)
 */
cmdgpt::TokenUsage cmdgpt::parse_token_usage(const std::string& response_json,
                                             std::string_view model)
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
                                        usage.completion_tokens * pricing.second) /
                                       1000.0;
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
 *
 * Creates a human-readable string representation of token usage
 * including counts and estimated cost.
 *
 * @param usage The TokenUsage struct to format
 * @return Formatted string suitable for console output
 */
std::string cmdgpt::format_token_usage(const TokenUsage& usage)
{
    std::stringstream ss;
    ss << "Token Usage: " << usage.total_tokens << " total "
       << "(" << usage.prompt_tokens << " prompt + " << usage.completion_tokens << " completion)";

    if (usage.estimated_cost > 0)
    {
        ss << std::fixed << std::setprecision(4) << " ~$" << usage.estimated_cost << " USD";
    }

    return ss.str();
}

// ============================================================================
// Response History Implementation
// ============================================================================

#include <ctime>
#include <iomanip>

/**
 * @brief ResponseHistory constructor
 */
cmdgpt::ResponseHistory::ResponseHistory(const std::filesystem::path& history_file,
                                         size_t max_entries)
    : max_entries_(max_entries)
{
    if (history_file.empty())
    {
        // Default to ~/.cmdgpt/history.json
        const char* home = std::getenv("HOME");
        if (!home)
        {
            throw ConfigurationException("HOME environment variable not set");
        }
        history_file_ = std::filesystem::path(home) / ".cmdgpt" / "history.json";
    }
    else
    {
        history_file_ = history_file;
    }

    // Create directory if it doesn't exist
    std::filesystem::create_directories(history_file_.parent_path());

    // Load existing history
    try
    {
        load();
    }
    catch (const std::exception& e)
    {
        gLogger->debug("Could not load history: {}", e.what());
    }
}

/**
 * @brief Add an entry to the history
 */
void cmdgpt::ResponseHistory::add_entry(std::string_view prompt, std::string_view response,
                                        std::string_view model, const TokenUsage& usage,
                                        bool from_cache)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");

    Entry entry{ss.str(), std::string(prompt), std::string(response), std::string(model),
                usage,    from_cache};

    entries_.push_back(std::move(entry));

    // Trim if necessary
    if (max_entries_ > 0 && entries_.size() > max_entries_)
    {
        entries_.erase(entries_.begin(), entries_.begin() + (entries_.size() - max_entries_));
    }

    // Auto-save
    try
    {
        save();
    }
    catch (const std::exception& e)
    {
        gLogger->error("Failed to save history: {}", e.what());
    }
}

/**
 * @brief Get recent history entries
 */
std::vector<cmdgpt::ResponseHistory::Entry> cmdgpt::ResponseHistory::get_recent(size_t count) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (entries_.empty())
        return {};

    size_t start_idx = entries_.size() > count ? entries_.size() - count : 0;
    return std::vector<Entry>(entries_.begin() + start_idx, entries_.end());
}

/**
 * @brief Search history by prompt content
 */
std::vector<cmdgpt::ResponseHistory::Entry>
cmdgpt::ResponseHistory::search(std::string_view query) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Entry> results;
    std::string query_lower(query);
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

    for (const auto& entry : entries_)
    {
        std::string prompt_lower = entry.prompt;
        std::transform(prompt_lower.begin(), prompt_lower.end(), prompt_lower.begin(), ::tolower);

        if (prompt_lower.find(query_lower) != std::string::npos)
        {
            results.push_back(entry);
        }
    }

    return results;
}

/**
 * @brief Clear all history
 */
size_t cmdgpt::ResponseHistory::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);

    size_t count = entries_.size();
    entries_.clear();

    // Save empty history
    try
    {
        save();
    }
    catch (const std::exception& e)
    {
        gLogger->error("Failed to save cleared history: {}", e.what());
    }

    return count;
}

/**
 * @brief Save history to file
 */
void cmdgpt::ResponseHistory::save() const
{
    json j = json::array();

    for (const auto& entry : entries_)
    {
        j.push_back({{"timestamp", entry.timestamp},
                     {"prompt", entry.prompt},
                     {"response", entry.response},
                     {"model", entry.model},
                     {"token_usage",
                      {{"prompt_tokens", entry.token_usage.prompt_tokens},
                       {"completion_tokens", entry.token_usage.completion_tokens},
                       {"total_tokens", entry.token_usage.total_tokens},
                       {"estimated_cost", entry.token_usage.estimated_cost}}},
                     {"from_cache", entry.from_cache}});
    }

    std::ofstream file(history_file_);
    if (!file)
    {
        throw std::runtime_error("Failed to open history file for writing: " +
                                 history_file_.string());
    }

    file << j.dump(2);
}

/**
 * @brief Load history from file
 */
void cmdgpt::ResponseHistory::load()
{
    if (!std::filesystem::exists(history_file_))
        return;

    std::ifstream file(history_file_);
    if (!file)
    {
        throw std::runtime_error("Failed to open history file for reading: " +
                                 history_file_.string());
    }

    json j;
    file >> j;

    entries_.clear();
    for (const auto& item : j)
    {
        Entry entry{
            item["timestamp"].get<std::string>(),
            item["prompt"].get<std::string>(),
            item["response"].get<std::string>(),
            item["model"].get<std::string>(),
            TokenUsage{static_cast<size_t>(item["token_usage"]["prompt_tokens"].get<int>()),
                       static_cast<size_t>(item["token_usage"]["completion_tokens"].get<int>()),
                       static_cast<size_t>(item["token_usage"]["total_tokens"].get<int>()),
                       item["token_usage"]["estimated_cost"].get<double>()},
            item["from_cache"].get<bool>()};
        entries_.push_back(std::move(entry));
    }
}

/**
 * @brief Get singleton ResponseHistory instance
 */
cmdgpt::ResponseHistory& cmdgpt::get_response_history()
{
    static ResponseHistory history;
    return history;
}

// ============================================================================
// Template Manager Implementation
// ============================================================================

#include <regex>

/**
 * @brief TemplateManager constructor
 */
cmdgpt::TemplateManager::TemplateManager(const std::filesystem::path& template_file)
{
    if (template_file.empty())
    {
        // Default to ~/.cmdgpt/templates.json
        const char* home = std::getenv("HOME");
        if (!home)
        {
            throw ConfigurationException("HOME environment variable not set");
        }
        template_file_ = std::filesystem::path(home) / ".cmdgpt" / "templates.json";
    }
    else
    {
        template_file_ = template_file;
    }

    // Create directory if it doesn't exist
    std::filesystem::create_directories(template_file_.parent_path());

    // Initialize built-in templates
    init_builtin_templates();

    // Load user templates
    try
    {
        load();
    }
    catch (const std::exception& e)
    {
        gLogger->debug("Could not load templates: {}", e.what());
    }
}

/**
 * @brief Initialize built-in templates
 */
void cmdgpt::TemplateManager::init_builtin_templates()
{
    // Code review template
    add_template("code-review", "Review code for bugs, style, and improvements",
                 "Please review the following code:\n\n{{code}}\n\n"
                 "Check for:\n"
                 "1. Bugs and potential errors\n"
                 "2. Code style and best practices\n"
                 "3. Performance improvements\n"
                 "4. Security issues\n"
                 "5. Suggestions for improvement");

    // Explain code template
    add_template("explain", "Explain how code works",
                 "Please explain how the following code works:\n\n{{code}}\n\n"
                 "Include:\n"
                 "1. Overall purpose\n"
                 "2. Step-by-step breakdown\n"
                 "3. Key concepts used\n"
                 "4. Any potential issues");

    // Refactor template
    add_template("refactor", "Refactor code for better quality",
                 "Please refactor the following code:\n\n{{code}}\n\n"
                 "Focus on:\n"
                 "1. {{focus}}\n"
                 "2. Maintaining functionality\n"
                 "3. Improving readability\n"
                 "4. Following best practices");

    // Generate docs template
    add_template("docs", "Generate documentation for code",
                 "Please generate {{style}} documentation for:\n\n{{code}}\n\n"
                 "Include appropriate comments and docstrings.");

    // Fix error template
    add_template("fix-error", "Help fix an error",
                 "I'm getting this error:\n\n{{error}}\n\n"
                 "From this code:\n\n{{code}}\n\n"
                 "Please help me fix it.");

    // Unit test template
    add_template("unit-test", "Generate unit tests",
                 "Please generate unit tests for:\n\n{{code}}\n\n"
                 "Use {{framework}} framework and include edge cases.");
}

/**
 * @brief Extract variable names from template content
 */
std::vector<std::string> cmdgpt::TemplateManager::extract_variables(std::string_view content)
{
    std::vector<std::string> variables;
    std::regex var_regex(R"(\{\{(\w+)\}\})");
    std::string content_str(content);

    std::sregex_iterator it(content_str.begin(), content_str.end(), var_regex);
    std::sregex_iterator end;

    std::set<std::string> unique_vars;
    for (; it != end; ++it)
    {
        unique_vars.insert((*it)[1].str());
    }

    variables.assign(unique_vars.begin(), unique_vars.end());
    return variables;
}

/**
 * @brief Add or update a template
 */
void cmdgpt::TemplateManager::add_template(std::string_view name, std::string_view description,
                                           std::string_view content)
{
    std::lock_guard<std::mutex> lock(mutex_);

    Template templ{std::string(name), std::string(description), std::string(content),
                   extract_variables(content)};

    templates_[std::string(name)] = std::move(templ);

    // Don't auto-save built-in templates
    static const std::set<std::string> builtin_names = {"code-review", "explain",   "refactor",
                                                        "docs",        "fix-error", "unit-test"};

    if (builtin_names.find(std::string(name)) == builtin_names.end())
    {
        try
        {
            save();
        }
        catch (const std::exception& e)
        {
            gLogger->error("Failed to save templates: {}", e.what());
        }
    }
}

/**
 * @brief Get a template by name
 */
std::optional<cmdgpt::TemplateManager::Template>
cmdgpt::TemplateManager::get_template(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = templates_.find(std::string(name));
    if (it != templates_.end())
    {
        return it->second;
    }
    return std::nullopt;
}

/**
 * @brief Remove a template
 */
bool cmdgpt::TemplateManager::remove_template(std::string_view name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Don't remove built-in templates
    static const std::set<std::string> builtin_names = {"code-review", "explain",   "refactor",
                                                        "docs",        "fix-error", "unit-test"};

    if (builtin_names.find(std::string(name)) != builtin_names.end())
    {
        return false;
    }

    auto it = templates_.find(std::string(name));
    if (it != templates_.end())
    {
        templates_.erase(it);

        try
        {
            save();
        }
        catch (const std::exception& e)
        {
            gLogger->error("Failed to save templates: {}", e.what());
        }

        return true;
    }
    return false;
}

/**
 * @brief List all templates
 */
std::vector<cmdgpt::TemplateManager::Template> cmdgpt::TemplateManager::list_templates() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<Template> result;
    for (const auto& [name, templ] : templates_)
    {
        result.push_back(templ);
    }
    return result;
}

/**
 * @brief Apply template with variable substitution
 */
std::string
cmdgpt::TemplateManager::apply_template(std::string_view name,
                                        const std::map<std::string, std::string>& variables) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = templates_.find(std::string(name));
    if (it == templates_.end())
    {
        throw std::runtime_error("Template not found: " + std::string(name));
    }

    const Template& templ = it->second;
    std::string result = templ.content;

    // Check that all required variables are provided
    for (const auto& var : templ.variables)
    {
        if (variables.find(var) == variables.end())
        {
            throw std::runtime_error("Missing variable: " + var);
        }
    }

    // Replace variables
    for (const auto& [var_name, var_value] : variables)
    {
        std::string placeholder = "{{" + var_name + "}}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos)
        {
            result.replace(pos, placeholder.length(), var_value);
            pos += var_value.length();
        }
    }

    return result;
}

/**
 * @brief Save templates to file
 */
void cmdgpt::TemplateManager::save() const
{
    json j = json::object();

    // Only save non-built-in templates
    static const std::set<std::string> builtin_names = {"code-review", "explain",   "refactor",
                                                        "docs",        "fix-error", "unit-test"};

    for (const auto& [name, templ] : templates_)
    {
        if (builtin_names.find(name) == builtin_names.end())
        {
            j[name] = {{"description", templ.description}, {"content", templ.content}};
        }
    }

    std::ofstream file(template_file_);
    if (!file)
    {
        throw std::runtime_error("Failed to open template file for writing: " +
                                 template_file_.string());
    }

    file << j.dump(2);
}

/**
 * @brief Load templates from file
 */
void cmdgpt::TemplateManager::load()
{
    if (!std::filesystem::exists(template_file_))
        return;

    std::ifstream file(template_file_);
    if (!file)
    {
        throw std::runtime_error("Failed to open template file for reading: " +
                                 template_file_.string());
    }

    json j;
    file >> j;

    for (auto& [name, data] : j.items())
    {
        add_template(name, data["description"].get<std::string>(),
                     data["content"].get<std::string>());
    }
}

/**
 * @brief Get singleton TemplateManager instance
 */
cmdgpt::TemplateManager& cmdgpt::get_template_manager()
{
    static TemplateManager manager;
    return manager;
}

// ============================================================================
// RateLimiter Implementation
// ============================================================================

/**
 * @brief Construct rate limiter with specified limit
 */
cmdgpt::RateLimiter::RateLimiter(double requests_per_second, size_t burst_size)
    : tokens_(burst_size > 0 ? static_cast<double>(burst_size) : requests_per_second),
      max_tokens_(burst_size > 0 ? static_cast<double>(burst_size) : requests_per_second),
      refill_rate_(requests_per_second), last_refill_(std::chrono::steady_clock::now())
{
    if (requests_per_second <= 0)
    {
        throw std::invalid_argument("Requests per second must be positive");
    }
}

/**
 * @brief Refill tokens based on elapsed time
 */
void cmdgpt::RateLimiter::refill()
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_refill_);

    double tokens_to_add = (elapsed.count() / 1000.0) * refill_rate_;
    tokens_ = std::min(tokens_ + tokens_to_add, max_tokens_);
    last_refill_ = now;
}

/**
 * @brief Acquire permission to make a request
 */
void cmdgpt::RateLimiter::acquire()
{
    std::unique_lock<std::mutex> lock(mutex_);

    while (true)
    {
        refill();

        if (tokens_ >= 1.0)
        {
            tokens_ -= 1.0;
            return;
        }

        // Calculate wait time until next token
        double tokens_needed = 1.0 - tokens_;
        auto wait_ms =
            std::chrono::milliseconds(static_cast<int64_t>((tokens_needed / refill_rate_) * 1000));

        cv_.wait_for(lock, wait_ms);
    }
}

/**
 * @brief Try to acquire permission without blocking
 */
bool cmdgpt::RateLimiter::try_acquire()
{
    std::lock_guard<std::mutex> lock(mutex_);
    refill();

    if (tokens_ >= 1.0)
    {
        tokens_ -= 1.0;
        return true;
    }

    return false;
}

/**
 * @brief Get current available tokens
 */
size_t cmdgpt::RateLimiter::available_tokens() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    const_cast<RateLimiter*>(this)->refill();
    return static_cast<size_t>(tokens_);
}

/**
 * @brief Get singleton RateLimiter instance
 */
cmdgpt::RateLimiter& cmdgpt::get_rate_limiter()
{
    // Default: 3 requests per second with burst of 5
    static RateLimiter limiter(3.0, 5);
    return limiter;
}

// ============================================================================
// Multimodal Support Implementation
// ============================================================================

/**
 * @brief Build a Vision API message with images
 */
std::string cmdgpt::build_vision_message_json(const std::string& text,
                                              const std::vector<ImageData>& images)
{
    json content = json::array();
    
    // Add text content
    content.push_back({
        {"type", "text"},
        {"text", text}
    });
    
    // Add image contents
    for (const auto& img : images)
    {
        std::string base64_data = base64_encode(img.data);
        content.push_back({
            {"type", "image_url"},
            {"image_url", {
                {"url", "data:" + img.mime_type + ";base64," + base64_data}
            }}
        });
    }
    
    json message = {
        {"role", "user"},
        {"content", content}
    };
    
    return message.dump();
}

/**
 * @brief Send a chat request with image inputs (Vision API)
 */
std::string cmdgpt::get_gpt_chat_response_with_images(std::string_view prompt,
                                                      const std::vector<ImageData>& images,
                                                      const Config& config)
{
    // Validate configuration
    config.validate();
    validate_prompt(prompt);
    
    if (images.empty())
    {
        throw ValidationException("At least one image is required for Vision API");
    }
    
    // Validate all images
    for (const auto& img : images)
    {
        if (!validate_image(img.data))
        {
            throw ImageValidationException("Invalid image data for file: " + img.filename);
        }
    }
    
    // Build messages array with vision content
    json messages = json::array();
    
    // Add system prompt if configured
    if (!config.system_prompt().empty())
    {
        messages.push_back({
            {"role", "system"},
            {"content", config.system_prompt()}
        });
    }
    
    // Add user message with images
    json vision_msg = json::parse(build_vision_message_json(std::string(prompt), images));
    messages.push_back(vision_msg);
    
    // Build request data
    json data = {
        {"model", config.model()},
        {"messages", messages},
        {"max_tokens", 4096}  // Vision models support higher token limits
    };
    
    // Make API request
    httplib::Headers headers = {
        {std::string(AUTHORIZATION_HEADER), "Bearer " + config.api_key()},
        {std::string(CONTENT_TYPE_HEADER), std::string(APPLICATION_JSON)}
    };
    
    // Apply rate limiting
    get_rate_limiter().acquire();
    
    // Get endpoint
    std::string server_url = config.endpoint().empty() ? std::string(SERVER_URL) : config.endpoint();
    std::string api_path = config.endpoint().empty() ? std::string(API_URL) : "/v1/chat/completions";
    
    httplib::Client cli{server_url};
    cli.set_connection_timeout(CONNECTION_TIMEOUT_SECONDS, 0);
    cli.set_read_timeout(READ_TIMEOUT_SECONDS, 0);
    cli.enable_server_certificate_verification(true);
    
    gLogger->debug("Sending vision request with {} images", images.size());
    
    auto res = cli.Post(api_path, headers, data.dump(), std::string(APPLICATION_JSON));
    
    if (!res)
    {
        throw NetworkException("Failed to connect to API - check network connection");
    }
    
    // Handle response
    const auto status = static_cast<HttpStatus>(res->status);
    if (status != HttpStatus::OK)
    {
        std::string error_msg = "HTTP " + std::to_string(res->status);
        if (!res->body.empty())
        {
            try
            {
                json error_json = json::parse(res->body);
                if (error_json.contains("error") && error_json["error"].contains("message"))
                {
                    error_msg += ": " + error_json["error"]["message"].get<std::string>();
                }
            }
            catch (...)
            {
                error_msg += ": " + res->body;
            }
        }
        throw ApiException(status, error_msg);
    }
    
    // Parse response
    try
    {
        json response = json::parse(res->body);
        if (!response.contains("choices") || response["choices"].empty())
        {
            throw ApiException(HttpStatus::OK, "Invalid API response: missing choices");
        }
        
        std::string content = response["choices"][0]["message"]["content"];
        
        // Store in cache if enabled
        if (config.cache_enabled())
        {
            auto& cache = get_response_cache();
            std::string cache_key = cache.generate_key(
                std::string(prompt) + " [" + std::to_string(images.size()) + " images]",
                config.model(),
                config.system_prompt()
            );
            cache.put(cache_key, content);
        }
        
        return content;
    }
    catch (const json::exception& e)
    {
        throw ApiException(HttpStatus::OK, "Failed to parse API response: " + std::string(e.what()));
    }
}

/**
 * @brief Generate an image using DALL-E
 */
std::string cmdgpt::generate_image(std::string_view prompt,
                                   const Config& config,
                                   const std::string& size,
                                   const std::string& quality,
                                   const std::string& style)
{
    // Validate inputs
    config.validate();
    if (prompt.empty())
    {
        throw ValidationException("Image generation prompt cannot be empty");
    }
    
    // Validate size
    static const std::set<std::string> valid_sizes = {
        "1024x1024", "1792x1024", "1024x1792", "512x512", "256x256"
    };
    if (valid_sizes.find(size) == valid_sizes.end())
    {
        throw ValidationException("Invalid image size. Valid sizes: 1024x1024, 1792x1024, 1024x1792, 512x512, 256x256");
    }
    
    // Build request
    json data = {
        {"model", "dall-e-3"},
        {"prompt", prompt},
        {"n", 1},
        {"size", size},
        {"quality", quality},
        {"style", style},
        {"response_format", "b64_json"}  // Get base64 data directly
    };
    
    // Make API request
    httplib::Headers headers = {
        {std::string(AUTHORIZATION_HEADER), "Bearer " + config.api_key()},
        {std::string(CONTENT_TYPE_HEADER), std::string(APPLICATION_JSON)}
    };
    
    // Apply rate limiting
    get_rate_limiter().acquire();
    
    std::string server_url = config.endpoint().empty() ? std::string(SERVER_URL) : config.endpoint();
    httplib::Client cli{server_url};
    cli.set_connection_timeout(CONNECTION_TIMEOUT_SECONDS, 0);
    cli.set_read_timeout(60, 0);  // Image generation can take longer
    cli.enable_server_certificate_verification(true);
    
    gLogger->info("Generating image with DALL-E...");
    
    auto res = cli.Post("/v1/images/generations", headers, data.dump(), std::string(APPLICATION_JSON));
    
    if (!res)
    {
        throw NetworkException("Failed to connect to DALL-E API");
    }
    
    // Handle response
    const auto status = static_cast<HttpStatus>(res->status);
    if (status != HttpStatus::OK)
    {
        std::string error_msg = "HTTP " + std::to_string(res->status);
        if (!res->body.empty())
        {
            try
            {
                json error_json = json::parse(res->body);
                if (error_json.contains("error") && error_json["error"].contains("message"))
                {
                    error_msg += ": " + error_json["error"]["message"].get<std::string>();
                }
            }
            catch (...)
            {
                error_msg += ": " + res->body;
            }
        }
        throw ApiException(status, error_msg);
    }
    
    // Parse response
    try
    {
        json response = json::parse(res->body);
        if (!response.contains("data") || response["data"].empty())
        {
            throw ApiException(HttpStatus::OK, "Invalid DALL-E response: missing data");
        }
        
        std::string base64_image = response["data"][0]["b64_json"];
        
        // Log revised prompt if available
        if (response["data"][0].contains("revised_prompt"))
        {
            gLogger->info("DALL-E revised prompt: {}", 
                         response["data"][0]["revised_prompt"].get<std::string>());
        }
        
        return base64_image;
    }
    catch (const json::exception& e)
    {
        throw ApiException(HttpStatus::OK, "Failed to parse DALL-E response: " + std::string(e.what()));
    }
}