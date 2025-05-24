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

using json = nlohmann::json;
using namespace cmdgpt;
namespace fs = std::filesystem;

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
              << "  -v, --version           Print the version of the program and exit\n"
              << "  -i, --interactive       Run in interactive mode (REPL)\n"
              << "  --stream                Enable streaming responses (not yet implemented)\n"
              << "  -f, --format FORMAT     Output format: plain, markdown, json, code\n"
              << "  -k, --api_key KEY       Set the OpenAI API key to KEY\n"
              << "  -s, --sys_prompt PROMPT Set the system prompt to PROMPT\n"
              << "  -l, --log_file FILE     Set the log file to FILE\n"
              << "  -m, --gpt_model MODEL   Set the GPT model to MODEL\n"
              << "  -L, --log_level LEVEL   Set the log level to LEVEL\n"
              << "                          (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)\n"
              << "\nprompt:\n"
              << "  The text prompt to send to the OpenAI GPT API. If not provided, the program\n"
              << "  will read from stdin (unless in interactive mode).\n"
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

/**
 * @brief Conversation class implementation
 */
void cmdgpt::Conversation::add_message(std::string_view role, std::string_view content)
{
    messages_.emplace_back(role, content);

    // Trim conversation if it gets too long
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
        total += (msg.role.length() + msg.content.length()) / 4;
    }
    return total;
}

/**
 * @brief ConfigFile class implementation
 */
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

    // Enable certificate verification for security
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

/**
 * @brief Format output based on specified format
 */
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

/**
 * @brief Run interactive REPL mode
 */
void cmdgpt::run_interactive_mode(Config& config)
{
    std::cout << "cmdgpt " << VERSION << " - Interactive Mode\n";
    std::cout << "Type '/help' for commands, '/exit' to quit\n\n";

    Conversation conversation;

    // Add system prompt to conversation
    if (!config.system_prompt().empty())
    {
        conversation.add_message(SYSTEM_ROLE, config.system_prompt());
    }

    std::string line;
    while (true)
    {
        std::cout << "> ";
        if (!std::getline(std::cin, line))
            break;

        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty())
            continue;

        // Handle commands
        if (line[0] == '/')
        {
            if (line == "/exit" || line == "/quit")
            {
                break;
            }
            else if (line == "/clear")
            {
                conversation.clear();
                if (!config.system_prompt().empty())
                {
                    conversation.add_message(SYSTEM_ROLE, config.system_prompt());
                }
                std::cout << "Conversation cleared.\n";
                continue;
            }
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
            std::string response = get_gpt_chat_response(conversation, config);

            // Add assistant response to conversation
            conversation.add_message("assistant", response);

            // Display response
            std::cout << response << "\n\n";
        }
        catch (const std::exception& e)
        {
            std::cerr << "Error: " << e.what() << "\n\n";
        }
    }

    std::cout << "\nGoodbye!\n";
}