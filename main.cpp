/**
 * @file main.cpp
 * @brief Main entry point for cmdgpt command-line tool
 * @author Joern Ihlenburg
 * @date 2023-2024
 *
 * This file contains the main function and command-line argument parsing
 * for the cmdgpt tool. It handles initialization, configuration loading,
 * and routing to appropriate modes (interactive or single-shot).
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
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
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
    // Initialize configuration
    cmdgpt::Config config;

    // Load from environment first
    config.load_from_environment();

    // Try to load from config file
    cmdgpt::ConfigFile config_file;
    if (cmdgpt::ConfigFile::exists())
    {
        try
        {
            if (config_file.load(cmdgpt::ConfigFile::get_default_path()))
            {
                config_file.apply_to(config);
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "Warning: Failed to load config file: " << e.what() << std::endl;
        }
    }

    std::string prompt;
    bool interactive_mode = false;
    cmdgpt::OutputFormat output_format = cmdgpt::OutputFormat::PLAIN;

    static const std::map<std::string, spdlog::level::level_enum> log_levels = {
        {"TRACE", spdlog::level::trace}, {"DEBUG", spdlog::level::debug},
        {"INFO", spdlog::level::info},   {"WARN", spdlog::level::warn},
        {"ERROR", spdlog::level::err},   {"CRITICAL", spdlog::level::critical},
    };

    // Parsing command-line arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            cmdgpt::print_help();
            return EXIT_SUCCESS;
        }
        else if (arg == "-v" || arg == "--version")
        {
            std::cout << "cmdgpt version: " << cmdgpt::VERSION << std::endl;
            return EXIT_SUCCESS;
        }
        else if (arg == "-i" || arg == "--interactive")
        {
            interactive_mode = true;
        }
        else if (arg == "--stream")
        {
            streaming_mode = true;
        }
        else if (arg == "-f" || arg == "--format")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Format argument requires a value" << std::endl;
                return EX_USAGE;
            }
            output_format = cmdgpt::parse_output_format(argv[i]);
        }
        else if (arg == "-k" || arg == "--api_key")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: API key argument requires a value" << std::endl;
                return EX_USAGE;
            }
            try
            {
                config.set_api_key(argv[i]);
            }
            catch (const cmdgpt::ValidationException& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return EX_USAGE;
            }
        }
        else if (arg == "-s" || arg == "--sys_prompt")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: System prompt argument requires a value" << std::endl;
                return EX_USAGE;
            }
            try
            {
                config.set_system_prompt(argv[i]);
            }
            catch (const cmdgpt::ValidationException& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return EX_USAGE;
            }
        }
        else if (arg == "-l" || arg == "--log_file")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Log file argument requires a value" << std::endl;
                return EX_USAGE;
            }
            try
            {
                config.set_log_file(argv[i]);
            }
            catch (const cmdgpt::ValidationException& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return EX_USAGE;
            }
        }
        else if (arg == "-m" || arg == "--gpt_model")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Model argument requires a value" << std::endl;
                return EX_USAGE;
            }
            try
            {
                config.set_model(argv[i]);
            }
            catch (const cmdgpt::ValidationException& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return EX_USAGE;
            }
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
                config.set_log_level(log_levels.at(log_level_str));
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
            cmdgpt::print_help();
            return EX_USAGE;
        }
        else
        {
            prompt = arg;
            // Basic validation for prompt length
            if (prompt.length() > 2000000) // 2MB limit
            {
                std::cerr << "Error: Prompt too long" << std::endl;
                return EX_USAGE;
            }
        }
    }

    // Set up logging
    try
    {
        auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file(), true);
        gLogger = std::make_shared<spdlog::logger>(
            "multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
        gLogger->set_level(config.log_level());
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        std::cerr << "Log initialization failed: " << ex.what() << std::endl;
        return EX_CONFIG;
    }

    // Run interactive mode if requested
    if (interactive_mode)
    {
        // Check for API key
        if (config.api_key().empty())
        {
            std::cerr
                << "Error: No API key provided. "
                << "Set OPENAI_API_KEY environment variable, use -k option, or add to ~/.cmdgptrc"
                << std::endl;
            return EX_CONFIG;
        }

        try
        {
            cmdgpt::run_interactive_mode(config);
            return EXIT_SUCCESS;
        }
        catch (const std::exception& e)
        {
            gLogger->critical("Interactive mode error: {}", e.what());
            return EXIT_FAILURE;
        }
    }

    // Check for API key for non-interactive mode
    if (config.api_key().empty())
    {
        std::cerr << "Error: No API key provided. "
                  << "Set OPENAI_API_KEY environment variable, use -k option, or add to ~/.cmdgptrc"
                  << std::endl;
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
        std::string response = cmdgpt::get_gpt_chat_response(prompt, config);

        // Format output based on requested format
        std::string formatted_output = cmdgpt::format_output(response, output_format);
        std::cout << formatted_output << std::endl;

        return EXIT_SUCCESS;
    }
    catch (const cmdgpt::ConfigurationException& e)
    {
        gLogger->critical("Configuration Error: {}", e.what());
        return EX_CONFIG;
    }
    catch (const cmdgpt::ValidationException& e)
    {
        gLogger->critical("Validation Error: {}", e.what());
        return EX_DATAERR;
    }
    catch (const cmdgpt::ApiException& e)
    {
        gLogger->critical("API Error: {}", e.what());
        return EX_TEMPFAIL;
    }
    catch (const cmdgpt::NetworkException& e)
    {
        gLogger->critical("Network Error: {}", e.what());
        return EX_TEMPFAIL;
    }
    catch (const std::exception& e)
    {
        gLogger->critical("Unexpected Error: {}", e.what());
        return EXIT_FAILURE;
    }
}
