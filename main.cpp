#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
#include "spdlog/spdlog.h"
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/file_sinks.h"
#include "cmdgpt.h"

int main(int argc, char* argv[]) {
    std::string api_key;
    std::string system_prompt;
    std::string gpt_model;
    std::string log_file;
    spdlog::level::level_enum log_level;
    std::string prompt;
    std::string response;
    int status_code;

    // Parse environment variables
    api_key = getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
    system_prompt = getenv("OPENAI_SYS_PROMPT") ? getenv("OPENAI_SYS_PROMPT") : DEFAULT_SYSTEM_PROMPT;
    gpt_model = getenv("OPENAI_GPT_MODEL") ? getenv("OPENAI_GPT_MODEL") : DEFAULT_MODEL;
    log_file = getenv("CMDGPT_LOG_FILE") ? getenv("CMDGPT_LOG_FILE") : "logfile.txt"; // Default log file
    std::string env_log_level = getenv("CMDGPT_LOG_LEVEL") ? getenv("CMDGPT_LOG_LEVEL") : "WARN"; // Default log level
    static const std::map<std::string, spdlog::level::level_enum> log_levels = {
        {"TRACE", spdlog::level::trace},
        {"DEBUG", spdlog::level::debug},
        {"INFO", spdlog::level::info},
        {"WARN", spdlog::level::warn},
        {"ERROR", spdlog::level::err},
        {"CRITICAL", spdlog::level::critical},
    };
    log_level = log_levels.count(env_log_level) ? log_levels.at(env_log_level) : DEFAULT_LOG_LEVEL;

    // Parsing command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help();
            return EXIT_SUCCESS;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "cmdgpt version: " << CMDGPT_VERSION << std::endl;
            return EXIT_SUCCESS;
        } else if (arg == "-k" || arg == "--api_key") {
            api_key = argv[++i];
        } else if (arg == "-s" || arg == "--sys_prompt") {
            system_prompt = argv[++i];
        } else if (arg == "-l" || arg == "--log_file") {
            log_file = argv[++i];
        } else if (arg == "-m" || arg == "--gpt_model") {
            gpt_model = argv[++i];
        } else if (arg == "-L" || arg == "--log_level") {
            std::string log_level_str = argv[++i];
            if (log_levels.count(log_level_str)) {
                log_level = log_levels.at(log_level_str);
            }
        } else {
            prompt = arg;
        }
    }

    // Set up logging
    auto console_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::simple_file_sink_mt>(log_file, true);
    gLogger = std::make_shared<spdlog::logger>("multi_sink", spdlog::sinks_init_list{console_sink, file_sink});
    gLogger->set_level(log_level);

    // Make the API request and handle the response
    if (prompt.empty()) {
        std::getline(std::cin, prompt);
    }
    status_code = get_gpt_chat_response(prompt, response, api_key, system_prompt, gpt_model);
    if (status_code == EMPTY_RESPONSE_CODE) {
        gLogger->critical("Error: Did not receive a response from the server.");
        return 1;
    }
    std::cout << response << std::endl;
    return 0;
}

