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

    // Use default values if not provided
    std::string actual_system_prompt{system_prompt.empty() ? DEFAULT_SYSTEM_PROMPT : system_prompt};
    std::string actual_model{model.empty() ? DEFAULT_MODEL : model};

    // Validate inputs
    if (prompt.empty())
    {
        throw ConfigurationException("Prompt cannot be empty");
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

    // Initialize the HTTP client
    httplib::Client cli{std::string(SERVER_URL)};

    // Set connection timeout
    cli.set_connection_timeout(30, 0); // 30 seconds
    cli.set_read_timeout(60, 0);       // 60 seconds

    // Log the data being sent
    gLogger->debug("Debug: Sending POST request to {} with data: {}", API_URL, data.dump());

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