#include <iostream>
#include <string>
#include <stdexcept>
#include <cstdlib>
#include <fstream>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/ansicolor_sink.h"
#include "spdlog/sinks/file_sinks.h"

using json = nlohmann::json;

// Preprocessor defines for constants
#define DEFAULT_MODEL "gpt-4"
#define DEFAULT_SYSTEM_PROMPT "You are a helpfull assitant!"
#define DEFAULT_LOG_LEVEL spdlog::level::warn
#define AUTHORIZATION_HEADER "Authorization"
#define CONTENT_TYPE_HEADER "Content-Type"
#define APPLICATION_JSON "application/json"
#define SYSTEM_ROLE "system"
#define USER_ROLE "user"
#define MODEL_KEY "model"
#define MESSAGES_KEY "messages"
#define ROLE_KEY "role"
#define CONTENT_KEY "content"
#define CHOICES_KEY "choices"
#define FINISH_REASON_KEY "finish_reason"
#define URL "/v1/chat/completions"
#define SERVER_URL "https://api.openai.com"
#define EMPTY_RESPONSE_CODE -1
#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_INTERNAL_SERVER_ERROR 500

// Map of string log levels to spdlog::level::level_enum values
const std::map<std::string, spdlog::level::level_enum> log_levels = {
    {"TRACE", spdlog::level::trace},
    {"DEBUG", spdlog::level::debug},
    {"INFO", spdlog::level::info},
    {"WARN", spdlog::level::warn},
    {"ERROR", spdlog::level::err},
    {"CRITICAL", spdlog::level::critical},
};

// Global logger variable accessible by all functions
std::shared_ptr<spdlog::logger> gLogger;

/**
 * @brief Prints the help message to the console.
 */
void print_help() {
    std::cout << "Usage: ./program [options] [prompt]\n"
              << "Options:\n"
              << "  -h, --help              Show this help message and exit\n"
              << "  -k, --api_key KEY       Set the OpenAI API key to KEY\n"
              << "  -s, --sys_prompt PROMPT Set the system prompt to PROMPT\n"
              << "  -l, --log_file FILE     Set the log file to FILE\n"
              << "  -m, --gpt_model MODEL   Set the GPT model to MODEL\n"
              << "  -L, --log_level LEVEL   Set the log level to LEVEL\n"
              << "                          (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)\n"
              << "prompt:\n"
              << "  The text prompt to send to the OpenAI GPT API. If not provided, the program will read from stdin.\n";
}

/**
 * @brief Sends a message to the GPT Chat API and returns the HTTP response status code.
 * @param prompt The text prompt to send to the API.
 * @param response A reference to a string where the API response will be stored.
 * @param api_key The API key for the OpenAI GPT API. Default is an empty string.
 * @param system_prompt The system prompt for the OpenAI GPT API. Default is an empty string.
 * @param model The GPT model to use. Default is DEFAULT_MODEL.
 * @return The HTTP response status code, or EMPTY_RESPONSE_CODE if no response was received.
 * @throws std::invalid_argument If no API key was provided.
 */
int get_gpt_chat_response(const std::string& prompt, std::string& response, const std::string& api_key = "", const std::string& system_prompt = "", const std::string& model = DEFAULT_MODEL) {
    // Declare the required variables at the beginning of the function
    json res_json;
    std::string finish_reason;

    // API key and system prompt must be provided
    if (api_key.empty() || system_prompt.empty()) {
        throw std::invalid_argument("API key and system prompt must be provided.");
    }

    // Setup headers for the POST request
    httplib::Headers headers = {
        { AUTHORIZATION_HEADER, "Bearer " + api_key },
        { CONTENT_TYPE_HEADER, APPLICATION_JSON }
    };

    // Prepare the JSON data for the POST request
    json data = {
        {MODEL_KEY, model},
        {MESSAGES_KEY, {
            {{ROLE_KEY, SYSTEM_ROLE}, {CONTENT_KEY, system_prompt}},
            {{ROLE_KEY, USER_ROLE}, {CONTENT_KEY, prompt}}
        }}
    };

    // Initialize the HTTP client
    httplib::Client cli(SERVER_URL);

    // Log the data being sent
    gLogger->debug("Debug: Sending POST request to {} with data: {}", URL, data.dump());

    // Send the POST request
    auto res = cli.Post(URL, headers, data.dump(), APPLICATION_JSON);

    // If response is received from the server
    if (res) {
        gLogger->debug("Debug: Received HTTP response with status {} and body: {}", res->status, res->body);
        // Handle the HTTP response status code
        switch (res->status) {
            case HTTP_OK:
                // Everything is fine
                break;
            case HTTP_BAD_REQUEST:
                gLogger->error("Error: Bad request.");
                return res->status;
            case HTTP_UNAUTHORIZED:
                gLogger->error("Error: Unauthorized. Check your API key.");
                return res->status;
            case HTTP_FORBIDDEN:
                gLogger->error("Error: Forbidden. You do not have the necessary permissions.");
                return res->status;
            case HTTP_NOT_FOUND:
                gLogger->error("Error: Not Found. The requested URL was not found on the server.");
                return res->status;
            case HTTP_INTERNAL_SERVER_ERROR:
                gLogger->error("Error: Internal Server Error. The server encountered an unexpected condition.");
                return res->status;
            default:
                gLogger->error("Error: Received unexpected HTTP status code: {}", res->status);
                return res->status;
        }
    } else {
        gLogger->debug("Debug: No response received from the server.");
        return EMPTY_RESPONSE_CODE;
    }

    // If response body is not empty
    if (!res->body.empty()) {
        // Parse the JSON response
        res_json = json::parse(res->body);

        // If 'choices' array is empty
        if (res_json[CHOICES_KEY].empty()) {
            gLogger->error("Error: '{}' array is empty.", CHOICES_KEY);
            return EMPTY_RESPONSE_CODE;
        }
        // If 'finish_reason' field is missing
        if (!res_json[CHOICES_KEY][0].contains(FINISH_REASON_KEY)) {
            gLogger->error("Error: '{}' field is missing.", FINISH_REASON_KEY);
            return EMPTY_RESPONSE_CODE;
        }
        // If 'content' field is missing
        if (!res_json[CHOICES_KEY][0]["message"].contains(CONTENT_KEY)) {
            gLogger->error("Error: '{}' field is missing.", CONTENT_KEY);
            return EMPTY_RESPONSE_CODE;
        }

        // Extract 'finish_reason' and 'content'
        finish_reason = res_json[CHOICES_KEY][0][FINISH_REASON_KEY].get<std::string>();
        gLogger->debug("Finish reason: {}", finish_reason);
        response = res_json[CHOICES_KEY][0]["message"][CONTENT_KEY].get<std::string>();
    }

    return res->status;
}

/**
 * @brief The main function of the application.
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments.
 * @return The exit code of the application.
 */
int main(int argc, char* argv[]) {
    std::string api_key;
    std::string system_prompt;
    std::string gpt_model;
    std::string log_file;
    spdlog::level::level_enum log_level;
    std::string arg;
    std::string prompt;
    std::string response;
    int status_code;

    // Parse command-line arguments and environment variables
    api_key = getenv("OPENAI_API_KEY") ? getenv("OPENAI_API_KEY") : "";
    system_prompt = getenv("OPENAI_SYSTEM_PROMPT") ? getenv("OPENAI_SYSTEM_PROMPT") : DEFAULT_SYSTEM_PROMPT;
    gpt_model = getenv("OPENAI_GPT_MODEL") ? getenv("OPENAI_GPT_MODEL") : DEFAULT_MODEL;
    log_file = getenv("CMDGPT_LOG_FILE") ? getenv("CMDGPT_LOG_FILE") : "logfile.txt"; // Default log file
    std::string env_log_level = getenv("CMDGPT_LOG_LEVEL") ? getenv("CMDGPT_LOG_LEVEL") : "WARN"; // Default log level
    log_level = log_levels.count(env_log_level) ? log_levels.at(env_log_level) : DEFAULT_LOG_LEVEL;
    // Check for command-line options
    // Parsing command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help();
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
        // If no prompt was provided in the command line, read it from stdin
        std::getline(std::cin, prompt);
    }
    status_code = get_gpt_chat_response(prompt, response, api_key, system_prompt, gpt_model);
    if (status_code == EMPTY_RESPONSE_CODE) {
        gLogger->critical("Error: Did not receive a response from the server.");
        return 1;
    }
    // output the response to stdout
    std::cout << response << std::endl;
    // that's all folks...
    return 0;
}
