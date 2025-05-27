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

#include "base64.h"
#include "cmdgpt.h"
#include "file_utils.h"
#include "spdlog/async.h"
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sysexits.h>
#include <unistd.h>

// Define EX_DATAERR if not available (it's not standard on all systems)
#ifndef EX_DATAERR
#define EX_DATAERR 65
#endif

/**
 * @brief Main entry point for the cmdgpt command-line tool
 *
 * Parses command-line arguments and environment variables, processes user input,
 * and interacts with the OpenAI API to generate responses.
 *
 * @param argc Number of command-line arguments
 * @param argv Array of command-line argument strings
 * @return Exit code:
 *         - 0 (EX_OK): Success
 *         - 64 (EX_USAGE): Command line usage error
 *         - 75 (EX_TEMPFAIL): Temporary failure (e.g., network error)
 *         - 78 (EX_CONFIG): Configuration error
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
    bool streaming_mode = false;
    cmdgpt::OutputFormat output_format = cmdgpt::OutputFormat::PLAIN;

    // Image-related variables
    std::vector<std::string> image_paths;
    bool generate_image_mode = false;
    std::string image_size = "1024x1024";
    std::string image_quality = "standard";
    std::string image_style = "vivid";
    bool save_images = false;

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
            config.set_streaming_mode(true);
        }
        else if (arg == "--no-cache")
        {
            config.set_cache_enabled(false);
        }
        else if (arg == "--clear-cache")
        {
            auto& cache = cmdgpt::get_response_cache();
            size_t cleared = cache.clear();
            std::cout << "Cleared " << cleared << " cache entries." << std::endl;
            return EXIT_SUCCESS;
        }
        else if (arg == "--show-tokens")
        {
            config.set_show_tokens(true);
        }
        else if (arg == "--endpoint")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Endpoint argument requires a value" << std::endl;
                return EX_USAGE;
            }
            try
            {
                config.set_endpoint(argv[i]);
            }
            catch (const cmdgpt::ValidationException& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
                return EX_USAGE;
            }
        }
        else if (arg == "--cache-stats")
        {
            const auto& cache = cmdgpt::get_response_cache();
            const auto stats = cache.get_stats();
            std::cout << "Cache Statistics:\n";
            std::cout << "  Entries: " << stats.at("count") << "\n";
            std::cout << "  Size: " << (stats.at("size_bytes") / 1024) << " KB\n";
            std::cout << "  Hits: " << stats.at("hits") << "\n";
            std::cout << "  Misses: " << stats.at("misses") << "\n";
            return EXIT_SUCCESS;
        }
        else if (arg == "--history")
        {
            const auto& history = cmdgpt::get_response_history();
            auto recent = history.get_recent(10);

            if (recent.empty())
            {
                std::cout << "No history entries found.\n";
            }
            else
            {
                std::cout << "Recent History (last " << recent.size() << " entries):\n\n";
                for (const auto& entry : recent)
                {
                    std::cout << "Date: " << entry.timestamp << "\n";
                    std::cout << "Model: " << entry.model;
                    if (entry.from_cache)
                        std::cout << " (cached)";
                    std::cout << "\n";
                    std::cout << "Prompt: " << entry.prompt.substr(0, 80);
                    if (entry.prompt.length() > 80)
                        std::cout << "...";
                    std::cout << "\n";
                    std::cout << "Tokens: " << entry.token_usage.total_tokens;
                    if (entry.token_usage.estimated_cost > 0)
                    {
                        std::cout << " (~$" << std::fixed << std::setprecision(4)
                                  << entry.token_usage.estimated_cost << ")";
                    }
                    std::cout << "\n\n";
                }
            }
            return EXIT_SUCCESS;
        }
        else if (arg == "--clear-history")
        {
            auto& history = cmdgpt::get_response_history();
            size_t cleared = history.clear();
            std::cout << "Cleared " << cleared << " history entries." << std::endl;
            return EXIT_SUCCESS;
        }
        else if (arg == "--search-history")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Search query required" << std::endl;
                return EX_USAGE;
            }
            const auto& history = cmdgpt::get_response_history();
            auto results = history.search(argv[i]);

            if (results.empty())
            {
                std::cout << "No matching history entries found.\n";
            }
            else
            {
                std::cout << "Found " << results.size() << " matching entries:\n\n";
                for (const auto& entry : results)
                {
                    std::cout << "Date: " << entry.timestamp << "\n";
                    std::cout << "Prompt: " << entry.prompt.substr(0, 80);
                    if (entry.prompt.length() > 80)
                        std::cout << "...";
                    std::cout << "\n\n";
                }
            }
            return EXIT_SUCCESS;
        }
        else if (arg == "--list-templates")
        {
            const auto& manager = cmdgpt::get_template_manager();
            auto templates = manager.list_templates();

            std::cout << "Available Templates:\n\n";
            for (const auto& templ : templates)
            {
                std::cout << templ.name << " - " << templ.description << "\n";
                if (!templ.variables.empty())
                {
                    std::cout << "  Variables: ";
                    for (size_t j = 0; j < templ.variables.size(); ++j)
                    {
                        std::cout << templ.variables[j];
                        if (j < templ.variables.size() - 1)
                            std::cout << ", ";
                    }
                    std::cout << "\n";
                }
                std::cout << "\n";
            }
            return EXIT_SUCCESS;
        }
        else if (arg == "--template")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Template name required" << std::endl;
                return EX_USAGE;
            }
            std::string template_name = argv[i];

            const auto& manager = cmdgpt::get_template_manager();
            auto templ_opt = manager.get_template(template_name);

            if (!templ_opt)
            {
                std::cerr << "Error: Template '" << template_name << "' not found" << std::endl;
                std::cerr << "Use --list-templates to see available templates" << std::endl;
                return EX_USAGE;
            }

            const auto& templ = *templ_opt;
            std::map<std::string, std::string> variables;

            // Collect variable values
            for (const auto& var : templ.variables)
            {
                if (++i >= argc)
                {
                    std::cerr << "Error: Value required for variable '" << var << "'" << std::endl;
                    return EX_USAGE;
                }
                variables[var] = argv[i];
            }

            // Apply template
            try
            {
                prompt = manager.apply_template(template_name, variables);
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error applying template: " << e.what() << std::endl;
                return EX_USAGE;
            }
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
        else if (arg == "-I" || arg == "--image")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Image path argument requires a value" << std::endl;
                return EX_USAGE;
            }
            image_paths.push_back(argv[i]);
        }
        else if (arg == "--images")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Images argument requires comma-separated paths" << std::endl;
                return EX_USAGE;
            }
            // Split comma-separated paths
            std::string paths_str = argv[i];
            size_t start = 0;
            size_t end = paths_str.find(',');
            while (end != std::string::npos)
            {
                image_paths.push_back(paths_str.substr(start, end - start));
                start = end + 1;
                end = paths_str.find(',', start);
            }
            image_paths.push_back(paths_str.substr(start));
        }
        else if (arg == "--generate-image")
        {
            generate_image_mode = true;
        }
        else if (arg == "--image-size")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Image size argument requires a value" << std::endl;
                return EX_USAGE;
            }
            image_size = argv[i];
        }
        else if (arg == "--image-quality")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Image quality argument requires a value" << std::endl;
                return EX_USAGE;
            }
            image_quality = argv[i];
        }
        else if (arg == "--image-style")
        {
            if (++i >= argc)
            {
                std::cerr << "Error: Image style argument requires a value" << std::endl;
                return EX_USAGE;
            }
            image_style = argv[i];
        }
        else if (arg == "--save-images")
        {
            save_images = true;
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
        // Initialize async logger with a thread pool
        // Queue size of 8192 and 1 backend thread for async logging
        spdlog::init_thread_pool(8192, 1);

        auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
        auto file_sink =
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.log_file(), true);

        // Create async logger to prevent I/O blocking on TRACE level
        gLogger = std::make_shared<spdlog::async_logger>(
            "multi_sink", spdlog::sinks_init_list{console_sink, file_sink}, spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        gLogger->set_level(config.log_level());

        // Only flush on error or higher to improve performance
        gLogger->flush_on(spdlog::level::err);

        // Register as default logger
        spdlog::register_logger(gLogger);
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

    /**
     * Input handling supports three modes:
     * 1. Command-line prompt only: ./cmdgpt "prompt"
     * 2. Stdin only: echo "prompt" | ./cmdgpt
     * 3. Combined mode: command | ./cmdgpt "instruction"
     *    In this mode, stdin provides context and the command-line argument
     *    provides the instruction to apply to that context.
     */

    // Check if there's input from stdin (pipe or file)
    std::string stdin_input;
    if (!isatty(fileno(stdin)))
    {
        // Reading from pipe or file - read all input
        std::string line;
        while (std::getline(std::cin, line))
        {
            if (!stdin_input.empty())
                stdin_input += "\n";
            stdin_input += line;
        }
    }

    // Combine stdin input with command-line prompt if both exist
    if (!stdin_input.empty())
    {
        if (!prompt.empty())
        {
            // Both stdin and command-line prompt exist
            // Format: stdin content + newlines + command prompt
            // This enables powerful usage like: git log | cmdgpt "summarize"
            prompt = stdin_input + "\n\n" + prompt;
        }
        else
        {
            // Only stdin input exists
            prompt = stdin_input;
        }
    }
    else if (prompt.empty())
    {
        // No input from either source
        std::cerr << "Error: No prompt provided" << std::endl;
        std::cerr << "Usage: " << argv[0] << " [options] \"prompt\"" << std::endl;
        std::cerr << "   or: echo \"prompt\" | " << argv[0] << " [options]" << std::endl;
        std::cerr << "   or: command | " << argv[0] << " \"instruction\"" << std::endl;
        return EX_USAGE;
    }

    // Handle image generation mode
    if (generate_image_mode)
    {
        try
        {
            gLogger->info("Generating image with prompt: {}", prompt);
            std::string base64_image;
            cmdgpt::TokenUsage token_usage;

            if (config.show_tokens())
            {
                auto api_response = cmdgpt::generate_image_full(prompt, config, image_size,
                                                                image_quality, image_style);
                base64_image = api_response.content;
                token_usage = api_response.token_usage;
            }
            else
            {
                base64_image =
                    cmdgpt::generate_image(prompt, config, image_size, image_quality, image_style);
            }

            if (save_images)
            {
                // Decode and save the image
                std::vector<uint8_t> image_data = cmdgpt::base64_decode(base64_image);
                std::string filename = cmdgpt::generate_timestamp_filename("png", "dalle");
                cmdgpt::save_file(image_data, filename);
                std::cout << "Image saved to: " << filename << std::endl;
            }
            else
            {
                // Output base64 data
                std::cout << base64_image << std::endl;
            }

            // Display token usage (cost) if requested
            if (config.show_tokens() && token_usage.estimated_cost > 0)
            {
                std::cout << "\nImage generation cost: $" << std::fixed << std::setprecision(3)
                          << token_usage.estimated_cost << std::endl;
            }

            return EXIT_SUCCESS;
        }
        catch (const std::exception& e)
        {
            gLogger->critical("Image generation error: {}", e.what());
            return EXIT_FAILURE;
        }
    }

    // Handle vision API mode if images are provided
    if (!image_paths.empty())
    {
        try
        {
            // Load all images
            std::vector<cmdgpt::ImageData> images;
            for (const auto& path : image_paths)
            {
                gLogger->info("Loading image: {}", path);
                images.push_back(cmdgpt::read_image_file(path));
            }

            // Set model to vision model if not already set
            if (config.model().find("vision") == std::string::npos && config.model() != "gpt-4o" &&
                config.model() != "gpt-4o-mini")
            {
                config.set_model("gpt-4o-mini");
                gLogger->info("Automatically selected vision model: gpt-4o-mini");
            }

            // Make vision API request
            std::string response;
            cmdgpt::TokenUsage token_usage;

            if (config.show_tokens())
            {
                auto api_response =
                    cmdgpt::get_gpt_chat_response_with_images_full(prompt, images, config);
                response = api_response.content;
                token_usage = api_response.token_usage;
            }
            else
            {
                response = cmdgpt::get_gpt_chat_response_with_images(prompt, images, config);
            }

            // Format and output response
            std::string formatted_output = cmdgpt::format_output(response, output_format);
            std::cout << formatted_output << std::endl;

            // Display token usage if requested
            if (config.show_tokens() && token_usage.total_tokens > 0)
            {
                std::cout << "\n" << cmdgpt::format_token_usage(token_usage) << std::endl;
            }

            // Extract and save any images in the response if requested
            if (save_images)
            {
                auto saved_files = cmdgpt::extract_and_save_images(response, "vision_response");
                if (!saved_files.empty())
                {
                    std::cout << "\nSaved " << saved_files.size() << " image(s) from response:\n";
                    for (const auto& file : saved_files)
                    {
                        std::cout << "  - " << file << "\n";
                    }
                }
            }

            return EXIT_SUCCESS;
        }
        catch (const std::exception& e)
        {
            gLogger->critical("Vision API error: {}", e.what());
            return EXIT_FAILURE;
        }
    }

    // Make the API request and handle the response
    try
    {
        if (streaming_mode)
        {
            // Streaming mode - output chunks as they arrive
            std::string full_response;

            cmdgpt::get_gpt_chat_response_stream(
                prompt, config,
                [&full_response, output_format](const std::string& chunk)
                {
                    // In streaming mode, we output plain text directly
                    // Other formats need the complete response
                    if (output_format == cmdgpt::OutputFormat::PLAIN)
                    {
                        std::cout << chunk << std::flush;
                    }
                    full_response += chunk;
                });

            // For non-plain formats, output the formatted result at the end
            if (output_format != cmdgpt::OutputFormat::PLAIN)
            {
                std::string formatted_output = cmdgpt::format_output(full_response, output_format);
                std::cout << formatted_output << std::endl;
            }
            else
            {
                std::cout << std::endl; // Add newline after streaming output
            }
        }
        else
        {
            // Non-streaming mode - wait for complete response with retry
            std::string response;
            cmdgpt::TokenUsage token_usage;

            if (config.show_tokens())
            {
                auto api_response = cmdgpt::get_gpt_chat_response_full(prompt, config);
                response = api_response.content;
                token_usage = api_response.token_usage;
            }
            else
            {
                response = cmdgpt::get_gpt_chat_response_with_retry(prompt, config);
            }

            // Format output based on requested format
            std::string formatted_output = cmdgpt::format_output(response, output_format);
            std::cout << formatted_output << std::endl;

            // Display token usage if requested
            if (config.show_tokens() && token_usage.total_tokens > 0)
            {
                std::cout << "\n" << cmdgpt::format_token_usage(token_usage) << std::endl;
            }

            // Extract and save any images in the response if requested
            if (save_images)
            {
                auto saved_files = cmdgpt::extract_and_save_images(response, "response");
                if (!saved_files.empty())
                {
                    std::cout << "\nSaved " << saved_files.size() << " image(s) from response:\n";
                    for (const auto& file : saved_files)
                    {
                        std::cout << "  - " << file << "\n";
                    }
                }
            }
        }

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
