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
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/file_sinks.h"
#include "spdlog/spdlog.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sysexits.h>

/**
 * @brief Parses command-line arguments and environment variables
 */
int main(int argc, const char* const argv[])
{
    std::string api_key;
    std::string system_prompt;
    std::string gpt_model;
    std::string log_file;
    spdlog::level::level_enum log_level;
    std::string prompt;

    // Parse environment variables
    api_key = getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
    system_prompt =
        getenv("OPENAI_SYS_PROMPT") ? getenv("OPENAI_SYS_PROMPT") : DEFAULT_SYSTEM_PROMPT;
    gpt_model = getenv("OPENAI_GPT_MODEL") ? getenv("OPENAI_GPT_MODEL") : DEFAULT_MODEL;
    log_file = getenv("CMDGPT_LOG_FILE") ? getenv("CMDGPT_LOG_FILE") : "logfile.txt";

    std::string env_log_level = getenv("CMDGPT_LOG_LEVEL") ? getenv("CMDGPT_LOG_LEVEL") : "WARN";

    static const std::map<std::string, spdlog::level::level_enum> log_levels = {
        {"TRACE", spdlog::level::trace}, {"DEBUG", spdlog::level::debug},
        {"INFO", spdlog::level::info},   {"WARN", spdlog::level::warn},
        {"ERROR", spdlog::level::err},   {"CRITICAL", spdlog::level::critical},
    };

    log_level = log_levels.count(env_log_level) ? log_levels.at(env_log_level) : DEFAULT_LOG_LEVEL;

    // Parsing command-line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            print_help();
            return EXIT_SUCCESS;
        }
        else if (arg == "-v" || arg == "--version")
        {
            std::cout << "cmdgpt version: " << CMDGPT_VERSION << std::endl;
            return EXIT_SUCCESS;
        }
        else if (arg == "-k" || arg == "--api_key")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: API key argument requires a value" << std::endl;
                return EX_USAGE;
            }
            api_key = argv[i];
        }
        else if (arg == "-s" || arg == "--sys_prompt")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: System prompt argument requires a value" << std::endl;
                return EX_USAGE;
            }
            system_prompt = argv[i];
        }
        else if (arg == "-l" || arg == "--log_file")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Log file argument requires a value" << std::endl;
                return EX_USAGE;
            }
            log_file = argv[i];
        }
        else if (arg == "-m" || arg == "--gpt_model")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Model argument requires a value" << std::endl;
                return EX_USAGE;
            }
            gpt_model = argv[i];
        }
        else if (arg == "-L" || arg == "--log_level")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Log level argument requires a value" << std::endl;
                return EX_USAGE;
            }
            std::string log_level_str = argv[i];
            if (log_levels.count(log_level_str))
            {
                log_level = log_levels.at(log_level_str);
            }
            else
            {
                std::cerr << "Error: Invalid log level: " << log_level_str << std::endl;
                return EX_USAGE;
            }
        }
        else if (arg.substr(0, 1) == "-")
        {
            std::cerr << "Error: Unknown argument: " << arg << std::endl;
            print_help();
            return EX_USAGE;
        }
        else
        {
            prompt = arg;
        }
    }

    // Set up logging
    try
    {
        auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::simple_file_sink_mt>(log_file, true);
        gLogger = std::make_shared<spdlog::logger>(
            "multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
        gLogger->set_level(log_level);
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return EX_CONFIG;
    }

    // Check for API key
    if (api_key.empty())
    {
        std::cerr << "Error: No API key provided. "
                  << "Set OPENAI_API_KEY environment variable or use -k option." << std::endl;
        return EX_CONFIG;
    }

    // Get prompt from stdin if not provided
    if (prompt.empty())
    {
        if (!std::getline(std::cin, prompt) || prompt.empty())
        {
            std::cerr << "Error: No prompt provided" << std::endl;
            return EX_USAGE;
        }
    }

    // Make the API request and handle the response
    try
    {
        std::string response;
        int status_code =
            get_gpt_chat_response(prompt, response, api_key, system_prompt, gpt_model);

        if (status_code == EMPTY_RESPONSE_CODE)
        {
            gLogger->critical("Error: Did not receive a response from the server.");
            return EX_TEMPFAIL;
        }
        else if (status_code != HTTP_OK)
        {
            gLogger->critical("Error: HTTP request failed with status code: {}", status_code);
            return EX_TEMPFAIL;
        }

        std::cout << response << std::endl;
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        gLogger->critical("Error: {}", e.what());
        return EXIT_FAILURE;
    }
}
