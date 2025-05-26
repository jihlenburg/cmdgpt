/**
 * @file cmdgpt.h
 * @brief Command-line interface for OpenAI GPT API
 * @author Joern Ihlenburg
 * @date 2023-2024
 * @version 0.5.0-dev
 *
 * This file contains the main API declarations for cmdgpt, a command-line
 * tool for interacting with OpenAI's GPT models. It provides features including:
 * - Interactive REPL mode for conversational AI
 * - Multiple output formats (plain, JSON, Markdown, code)
 * - Configuration file support
 * - Comprehensive error handling
 * - Security features including input validation
 *
 * @copyright Copyright (c) 2023 Joern Ihlenburg
 * @author Joern Ihlenburg
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

#ifndef CMDGPT_H
#define CMDGPT_H

#include "spdlog/spdlog.h"
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/**
 * @namespace cmdgpt
 * @brief Main namespace for cmdgpt functionality
 *
 * Contains all classes, functions, and constants for the cmdgpt
 * command-line tool. Organized into several logical components:
 * - Configuration management (Config, ConfigFile)
 * - Conversation handling (Message, Conversation)
 * - API interaction (get_gpt_chat_response functions)
 * - Output formatting (OutputFormat, format_output)
 * - Error handling (exception hierarchy)
 */
namespace cmdgpt
{

/// @brief Current version of cmdgpt
inline constexpr std::string_view VERSION = "v0.5.0-dev";

/// @name Default Configuration Values
/// @{
inline constexpr std::string_view DEFAULT_MODEL = "gpt-4"; ///< Default GPT model
inline constexpr std::string_view DEFAULT_SYSTEM_PROMPT =
    "You are a helpful assistant!"; ///< Default system prompt
inline constexpr spdlog::level::level_enum DEFAULT_LOG_LEVEL =
    spdlog::level::warn; ///< Default log level
/// @}

/// @name HTTP Headers
/// @{
inline constexpr std::string_view AUTHORIZATION_HEADER =
    "Authorization"; ///< HTTP Authorization header
inline constexpr std::string_view CONTENT_TYPE_HEADER =
    "Content-Type"; ///< HTTP Content-Type header
inline constexpr std::string_view APPLICATION_JSON = "application/json"; ///< JSON content type
/// @}

/// @name OpenAI API JSON Keys
/// @{
inline constexpr std::string_view SYSTEM_ROLE = "system";    ///< System role identifier
inline constexpr std::string_view USER_ROLE = "user";        ///< User role identifier
inline constexpr std::string_view MODEL_KEY = "model";       ///< Model key in API request
inline constexpr std::string_view MESSAGES_KEY = "messages"; ///< Messages array key
inline constexpr std::string_view ROLE_KEY = "role";         ///< Role key in message
inline constexpr std::string_view CONTENT_KEY = "content";   ///< Content key in message
inline constexpr std::string_view CHOICES_KEY = "choices";   ///< Choices array in response
inline constexpr std::string_view FINISH_REASON_KEY =
    "finish_reason"; ///< Finish reason in response
/// @}

/// @name API Configuration
/// @{
inline constexpr std::string_view API_URL =
    "/v1/chat/completions"; ///< OpenAI chat completions endpoint
inline constexpr std::string_view SERVER_URL = "https://api.openai.com"; ///< OpenAI API server URL
/// @}

/// @name Security and Validation Limits
/// @{
inline constexpr size_t MAX_PROMPT_LENGTH = 1024 * 1024;        ///< Maximum prompt length (1MB)
inline constexpr size_t MAX_RESPONSE_LENGTH = 10 * 1024 * 1024; ///< Maximum response length (10MB)
inline constexpr size_t MAX_API_KEY_LENGTH = 256;               ///< Maximum API key length
inline constexpr int CONNECTION_TIMEOUT_SECONDS = 30;           ///< Connection timeout in seconds
inline constexpr int READ_TIMEOUT_SECONDS = 60;                 ///< Read timeout in seconds
inline constexpr size_t MAX_CACHE_SIZE_MB = 100;                ///< Maximum cache size in MB
inline constexpr size_t MAX_CACHE_ENTRIES = 1000;               ///< Maximum number of cache entries
/// @}

/**
 * @brief HTTP status codes used by the API
 */
enum class HttpStatus : int
{
    EMPTY_RESPONSE = -1,        ///< No response received or empty response body
    OK = 200,                   ///< Request successful
    BAD_REQUEST = 400,          ///< Bad request format or parameters
    UNAUTHORIZED = 401,         ///< Invalid or missing API key
    FORBIDDEN = 403,            ///< Access forbidden
    NOT_FOUND = 404,            ///< Endpoint not found
    TOO_MANY_REQUESTS = 429,    ///< Rate limit exceeded
    INTERNAL_SERVER_ERROR = 500 ///< Server error
};

/**
 * @brief Output format options for response formatting
 *
 * Determines how the API response will be formatted before display
 */
enum class OutputFormat
{
    PLAIN,    ///< Plain text output (default)
    MARKDOWN, ///< Markdown formatted output with headers
    JSON,     ///< JSON structured output with metadata
    CODE      ///< Extract and display only code blocks
};

/**
 * @brief Convert string to OutputFormat enum
 *
 * Case-insensitive parsing of format strings. Supports abbreviations.
 *
 * @param format String representation of format ("plain", "json", "markdown"/"md", "code")
 * @return Corresponding OutputFormat enum value, defaults to PLAIN if unknown
 *
 * @code
 * auto fmt = parse_output_format("json");  // Returns OutputFormat::JSON
 * auto fmt2 = parse_output_format("md");   // Returns OutputFormat::MARKDOWN
 * @endcode
 */
OutputFormat parse_output_format(std::string_view format);

// Custom exception classes for better error handling

/**
 * @brief Base exception class for cmdgpt errors
 *
 * This is the base class for all cmdgpt-specific exceptions.
 * Derives from std::runtime_error to provide standard exception interface.
 *
 * @see ApiException, NetworkException, ConfigurationException, ValidationException
 */
class CmdGptException : public std::runtime_error
{
  public:
    explicit CmdGptException(const std::string& message) : std::runtime_error(message)
    {
    }
};

/**
 * @brief Exception for API-related errors
 *
 * Thrown when the OpenAI API returns an error response or when
 * the API response cannot be parsed correctly. Stores the HTTP
 * status code for programmatic error handling.
 *
 * Common status codes:
 * - 400: Bad Request (invalid parameters)
 * - 401: Unauthorized (invalid API key)
 * - 429: Too Many Requests (rate limit exceeded)
 * - 500: Internal Server Error
 * - 503: Service Unavailable
 */
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

/**
 * @brief Exception for network-related errors
 *
 * Thrown when network connectivity issues occur, including:
 * - Connection timeouts
 * - DNS resolution failures
 * - SSL/TLS certificate errors
 * - Socket errors
 * - Proxy connection failures
 */
class NetworkException : public CmdGptException
{
  public:
    explicit NetworkException(const std::string& message)
        : CmdGptException("Network Error: " + message)
    {
    }
};

/**
 * @brief Exception for configuration errors
 *
 * Thrown when configuration is invalid or missing, including:
 * - Missing API key
 * - Invalid model names
 * - Malformed configuration files
 * - Invalid environment variables
 * - Incompatible configuration combinations
 */
class ConfigurationException : public CmdGptException
{
  public:
    explicit ConfigurationException(const std::string& message)
        : CmdGptException("Configuration Error: " + message)
    {
    }
};

/**
 * @brief Exception for validation errors
 *
 * Thrown when input validation fails, including:
 * - Invalid API key format
 * - Empty or malformed prompts
 * - Input containing potential security risks
 * - Exceeding size limits
 * - Invalid characters or control sequences
 */
class ValidationException : public CmdGptException
{
  public:
    explicit ValidationException(const std::string& message)
        : CmdGptException("Validation Error: " + message)
    {
    }
};

/**
 * @brief Exception for security violations
 *
 * Thrown when potential security issues are detected, including:
 * - Path traversal attempts
 * - Suspicious input patterns
 * - Permission violations
 * - Cache tampering attempts
 */
class SecurityException : public CmdGptException
{
  public:
    explicit SecurityException(const std::string& message)
        : CmdGptException("Security Error: " + message)
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

    /**
     * @brief Set API key with validation
     * @param key OpenAI API key
     * @throws ValidationException if key is invalid
     */
    void set_api_key(std::string_view key);

    /**
     * @brief Set system prompt with validation
     * @param prompt System prompt for context
     * @throws ValidationException if prompt exceeds size limits
     */
    void set_system_prompt(std::string_view prompt);

    /**
     * @brief Set model name with validation
     * @param model GPT model name (e.g., "gpt-4", "gpt-3.5-turbo")
     * @throws ValidationException if model name is invalid
     */
    void set_model(std::string_view model);

    /**
     * @brief Set log file path with validation
     * @param file Path to log file
     * @throws ValidationException if path is invalid
     */
    void set_log_file(std::string_view file);

    /**
     * @brief Set logging level
     * @param level spdlog log level
     */
    void set_log_level(spdlog::level::level_enum level);

    /**
     * @brief Get configured API key
     * @return API key string
     */
    const std::string& api_key() const noexcept
    {
        return api_key_;
    }

    /**
     * @brief Get configured system prompt
     * @return System prompt string
     */
    const std::string& system_prompt() const noexcept
    {
        return system_prompt_;
    }

    /**
     * @brief Get configured model name
     * @return Model name string
     */
    const std::string& model() const noexcept
    {
        return model_;
    }

    /**
     * @brief Get configured log file path
     * @return Log file path string
     */
    const std::string& log_file() const noexcept
    {
        return log_file_;
    }

    /**
     * @brief Get configured log level
     * @return spdlog log level
     */
    spdlog::level::level_enum log_level() const noexcept
    {
        return log_level_;
    }

    /**
     * @brief Enable or disable streaming mode
     * @param enable True to enable streaming, false to disable
     */
    void set_streaming_mode(bool enable) noexcept
    {
        streaming_mode_ = enable;
    }

    /**
     * @brief Check if streaming mode is enabled
     * @return True if streaming is enabled
     */
    bool streaming_mode() const noexcept
    {
        return streaming_mode_;
    }

    /**
     * @brief Enable or disable response caching
     * @param enable True to enable caching, false to disable
     */
    void set_cache_enabled(bool enable) noexcept
    {
        cache_enabled_ = enable;
    }

    /**
     * @brief Check if caching is enabled
     * @return True if caching is enabled
     */
    bool cache_enabled() const noexcept
    {
        return cache_enabled_;
    }

    /**
     * @brief Enable or disable token usage display
     * @param enable True to show token usage, false to hide
     */
    void set_show_tokens(bool enable) noexcept
    {
        show_tokens_ = enable;
    }

    /**
     * @brief Check if token usage display is enabled
     * @return True if token usage should be shown
     */
    bool show_tokens() const noexcept
    {
        return show_tokens_;
    }

    /**
     * @brief Load configuration from environment variables
     *
     * Reads from:
     * - OPENAI_API_KEY
     * - OPENAI_SYS_PROMPT
     * - OPENAI_GPT_MODEL
     * - CMDGPT_LOG_FILE
     * - CMDGPT_LOG_LEVEL
     */
    void load_from_environment();

    /**
     * @brief Validate all configuration values
     * @throws ValidationException if any configuration is invalid
     */
    void validate() const;

  private:
    std::string api_key_;                                    ///< OpenAI API key for authentication
    std::string system_prompt_{DEFAULT_SYSTEM_PROMPT};       ///< System prompt to set AI behavior
    std::string model_{DEFAULT_MODEL};                       ///< OpenAI model name (e.g., "gpt-4")
    std::string log_file_{"logfile.txt"};                    ///< Path to debug log file
    spdlog::level::level_enum log_level_{DEFAULT_LOG_LEVEL}; ///< Logging verbosity level
    bool streaming_mode_{false};                             ///< Whether to stream responses
    bool cache_enabled_{true};                               ///< Whether to use response cache
    bool show_tokens_{false};                                ///< Whether to display token usage
};

/**
 * @brief Represents a single message in a conversation
 *
 * Messages are the fundamental unit of conversation with the AI model.
 * Each message has a role (system, user, assistant) and content.
 */
struct Message
{
    std::string role;    ///< Role of the message sender ("system", "user", "assistant")
    std::string content; ///< Text content of the message

    /**
     * @brief Construct a new Message
     * @param r Role of the message sender
     * @param c Content of the message
     */
    Message(std::string_view r, std::string_view c) : role(r), content(c)
    {
    }
};

/**
 * @brief Manages conversation history and context
 *
 * The Conversation class maintains a history of messages exchanged with the AI model,
 * automatically managing context length to stay within token limits. It provides
 * persistence through JSON serialization and file I/O.
 *
 * @code
 * Conversation conv;
 * conv.add_message("system", "You are a helpful assistant");
 * conv.add_message("user", "Hello!");
 * conv.save_to_file("conversation.json");
 * @endcode
 */
class Conversation
{
  public:
    /// Default constructor creates empty conversation
    Conversation() = default;

    /**
     * @brief Add a message to the conversation
     *
     * Automatically trims old messages if context length exceeds limits
     *
     * @param role Role of the message sender ("system", "user", "assistant")
     * @param content Text content of the message
     */
    void add_message(std::string_view role, std::string_view content);

    /**
     * @brief Clear all messages from the conversation
     */
    void clear();

    /**
     * @brief Get all messages in the conversation
     * @return Const reference to the vector of messages
     */
    const std::vector<Message>& get_messages() const
    {
        return messages_;
    }

    /**
     * @brief Save conversation to a JSON file
     *
     * @param path Filesystem path to save the conversation
     * @throws std::runtime_error if file cannot be opened for writing
     */
    void save_to_file(const std::filesystem::path& path) const;

    /**
     * @brief Load conversation from a JSON file
     *
     * @param path Filesystem path to load the conversation from
     * @throws std::runtime_error if file cannot be opened or parsed
     */
    void load_from_file(const std::filesystem::path& path);

    /**
     * @brief Get conversation as formatted JSON string
     * @return JSON representation of the conversation with proper formatting
     */
    std::string to_json() const;

    /**
     * @brief Estimate token count for the conversation
     *
     * Uses rough heuristic of 1 token ≈ 4 characters
     *
     * @return Estimated number of tokens in the conversation
     */
    size_t estimate_tokens() const;

  private:
    std::vector<Message> messages_;                      ///< Ordered list of conversation messages
    static constexpr size_t MAX_CONTEXT_LENGTH = 100000; ///< Maximum context length (~25k tokens)
};

/**
 * @brief Configuration file manager
 *
 * Handles loading and saving configuration from/to .cmdgptrc files.
 * Supports simple key=value format with comment lines starting with #.
 *
 * @code
 * ConfigFile config;
 * if (config.load(ConfigFile::get_default_path())) {
 *     config.apply_to(my_config);
 * }
 * @endcode
 *
 * Example .cmdgptrc file:
 * @code
 * # cmdgpt configuration
 * api_key=sk-...
 * model=gpt-4
 * system_prompt=You are a helpful assistant
 * log_level=INFO
 * @endcode
 */
class ConfigFile
{
  public:
    /// Default constructor
    ConfigFile() = default;

    /**
     * @brief Load configuration from file
     *
     * Parses key=value pairs, ignoring comments and empty lines
     *
     * @param path Path to configuration file
     * @return true if file was successfully loaded, false otherwise
     */
    bool load(const std::filesystem::path& path);

    /**
     * @brief Save configuration to file
     *
     * Writes all key=value pairs with header comment
     *
     * @param path Path to save configuration file
     * @throws std::runtime_error if file cannot be opened for writing
     */
    void save(const std::filesystem::path& path) const;

    /**
     * @brief Apply loaded configuration to a Config object
     *
     * Only sets values that exist in the configuration file
     *
     * @param config Config object to update with loaded values
     */
    void apply_to(Config& config) const;

    /**
     * @brief Get the default configuration file path
     *
     * @return Path to ~/.cmdgptrc
     * @throws std::runtime_error if HOME environment variable is not set
     */
    static std::filesystem::path get_default_path();

    /**
     * @brief Check if default config file exists
     *
     * @return true if ~/.cmdgptrc exists, false otherwise
     */
    static bool exists();

  private:
    std::map<std::string, std::string> values_; ///< Key-value pairs from config file
};

/**
 * @brief Streaming response callback type
 *
 * Function signature for callbacks that receive streaming response chunks.
 * Called repeatedly as data arrives from the API.
 *
 * @param chunk Partial response text received from the API
 */
using StreamCallback = std::function<void(const std::string& chunk)>;

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
 * @see Implementation for detailed parameter documentation
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

/**
 * @brief Sends a chat completion request with conversation history
 *
 * @param conversation The conversation history
 * @param config Configuration object containing API settings
 * @return The API response text
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 */
std::string get_gpt_chat_response(const Conversation& conversation, const Config& config);

/**
 * @brief Sends a streaming chat completion request
 *
 * @param prompt The user's input prompt
 * @param config Configuration object containing API settings
 * @param callback Function called for each response chunk
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 */
void get_gpt_chat_response_stream(std::string_view prompt, const Config& config,
                                  StreamCallback callback);

/**
 * @brief Sends a streaming chat completion request with conversation history
 *
 * @param conversation The conversation history
 * @param config Configuration object containing API settings
 * @param callback Function called for each response chunk
 * @throws ApiException on HTTP errors
 * @throws NetworkException on network/connection errors
 * @throws ConfigurationException on invalid configuration
 * @throws ValidationException on invalid input data
 */
void get_gpt_chat_response_stream(const Conversation& conversation, const Config& config,
                                  StreamCallback callback);

/**
 * @brief Format output based on the specified format
 *
 * @param content The content to format
 * @param format The desired output format
 * @return Formatted string
 */
std::string format_output(const std::string& content, OutputFormat format);

/**
 * @brief Run interactive REPL mode
 *
 * @param config Configuration object
 */
void run_interactive_mode(const Config& config);

/**
 * @brief Send chat request with automatic retry on failure
 *
 * @param prompt The user's input prompt
 * @param config Configuration object containing API settings
 * @param max_retries Maximum number of retry attempts (default: 3)
 * @return The API response text
 *
 * @throws ApiException on non-retryable errors or after max retries
 * @throws NetworkException on network errors after max retries
 *
 * @note Automatically retries on rate limits (429) and server errors (5xx)
 * @note Uses exponential backoff between retries
 */
std::string get_gpt_chat_response_with_retry(std::string_view prompt, const Config& config,
                                             int max_retries = 3);

/**
 * @brief Send chat request with conversation history and automatic retry
 *
 * @param conversation The conversation history
 * @param config Configuration object containing API settings
 * @param max_retries Maximum number of retry attempts (default: 3)
 * @return The API response text
 *
 * @throws ApiException on non-retryable errors or after max retries
 * @throws NetworkException on network errors after max retries
 *
 * @note Automatically retries on rate limits (429) and server errors (5xx)
 * @note Uses exponential backoff between retries
 */
std::string get_gpt_chat_response_with_retry(const Conversation& conversation, const Config& config,
                                             int max_retries = 3);

// ============================================================================
// Cache Management
// ============================================================================

/**
 * @brief Response cache for avoiding duplicate API calls
 *
 * Caches API responses to disk using a hash of the request parameters.
 * Default cache location: ~/.cmdgpt/cache/
 * Default expiration: 24 hours
 */
class ResponseCache
{
public:
    /**
     * @brief Construct cache with custom directory and expiration
     * @param cache_dir Directory to store cache files
     * @param expiration_hours Hours before cache entries expire (default: 24)
     */
    explicit ResponseCache(const std::filesystem::path& cache_dir = "",
                          size_t expiration_hours = 24);

    /**
     * @brief Generate cache key from request parameters
     * @param prompt User prompt
     * @param model Model name
     * @param system_prompt System prompt
     * @return SHA256 hash as hex string
     */
    std::string generate_key(std::string_view prompt, std::string_view model,
                           std::string_view system_prompt) const;

    /**
     * @brief Check if a valid cache entry exists
     * @param key Cache key
     * @return true if valid cache exists, false otherwise
     */
    bool has_valid_cache(const std::string& key) const;

    /**
     * @brief Retrieve cached response
     * @param key Cache key
     * @return Cached response or empty string if not found
     */
    std::string get(const std::string& key) const;

    /**
     * @brief Store response in cache
     * @param key Cache key
     * @param response Response to cache
     */
    void put(const std::string& key, std::string_view response);

    /**
     * @brief Clear all cache entries
     * @return Number of entries cleared
     */
    size_t clear();

    /**
     * @brief Clean expired cache entries
     * @return Number of entries removed
     */
    size_t clean_expired();

    /**
     * @brief Get cache statistics
     * @return Map of statistics (size, count, hits, misses)
     */
    std::map<std::string, size_t> get_stats() const;

private:
    std::filesystem::path cache_dir_;
    size_t expiration_hours_;
    mutable size_t cache_hits_ = 0;
    mutable size_t cache_misses_ = 0;

    std::filesystem::path get_cache_path(const std::string& key) const;
    bool is_expired(const std::filesystem::path& path) const;
};

/**
 * @brief Get or create the global response cache instance
 * @return Reference to the global cache
 */
ResponseCache& get_response_cache();

/**
 * @brief Token usage information from API response
 */
struct TokenUsage
{
    size_t prompt_tokens = 0;     ///< Tokens in the prompt
    size_t completion_tokens = 0; ///< Tokens in the completion
    size_t total_tokens = 0;      ///< Total tokens used
    double estimated_cost = 0.0;  ///< Estimated cost in USD
};

/**
 * @brief Complete API response including content and metadata
 */
struct ApiResponse
{
    std::string content;     ///< The actual response text
    TokenUsage token_usage;  ///< Token usage information
    bool from_cache = false; ///< Whether response was from cache
};

/**
 * @brief Parse token usage from API response
 * @param response_json JSON response from API
 * @param model Model name for cost calculation
 * @return TokenUsage struct with parsed values
 */
TokenUsage parse_token_usage(const std::string& response_json, std::string_view model);

/**
 * @brief Format token usage for display
 * @param usage Token usage information
 * @return Formatted string for display
 */
std::string format_token_usage(const TokenUsage& usage);

} // namespace cmdgpt

// Global logger instance
extern std::shared_ptr<spdlog::logger> gLogger;

#endif // CMDGPT_H
