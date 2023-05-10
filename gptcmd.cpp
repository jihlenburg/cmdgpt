#include <iostream>     // for std::cout, std::cerr
#include <string>       // for std::string
#include <stdexcept>    // for std::invalid_argument
#include <cstdlib>      // for std::getenv
#include "httplib.h"    // for httplib::Client
#include "json.hpp"     // for json

using json = nlohmann::json;

/**
 * @brief Send a message to the GPT Chat API and return the HTTP response status code.
 * @param prompt The text prompt to send to the API.
 * @param response A reference to a string where the API response will be stored.
 * @param api_key The API key for the OpenAI GPT API. Default is an empty string.
 * @param system_prompt The system prompt for the OpenAI GPT API. Default is an empty string.
 * @return The HTTP response status code, or -1 if no response was received.
 * @throws std::invalid_argument If no API key was provided.
 */
int get_gpt_chat_response(const std::string& prompt, std::string& response, const std::string& api_key = "", const std::string& system_prompt = "") {
    // If no API key or system prompt is provided, throw an exception
    if (api_key.empty() || system_prompt.empty()) {
        throw std::invalid_argument("API key and system prompt must be provided.");
    }

    // Headers for the API request
    httplib::Headers headers = {
        { "Authorization", "Bearer " + api_key },
        { "Content-Type", "application/json" }
    };

    // Data to be sent in the API request
    json data = {
        {"model", "gpt-4"},
        {"messages", {
            {{"role", "system"}, {"content", system_prompt}},
            {{"role", "user"}, {"content", prompt}}
        }}
    };

    // Use chat endpoint
    std::string url = "/v1/chat/completions";

    // Send the post request to the API
    httplib::Client cli("api.openai.com");
    auto res = cli.Post(url.c_str(), headers, data.dump(), "application/json");
    
    // If response is received, process it
    if (res) {
        json res_json = json::parse(res->body);
        std::string finish_reason = res_json["choices"][0]["finish_reason"].get<std::string>();
        std::cerr << "Finish reason: " << finish_reason << std::endl;
        response = res_json["choices"][0]["message"]["content"].get<std::string>();
    }
    
    // Return the status code (or -1 if no response was received)
    return res ? res->status : -1;
}

/**
 * @brief Function to print help message.
 */
void print_help() {
    std::cout << "Usage: ./program [options] prompt\n"
              << "Options:\n"
              << "-h, --help        Show this help message\n"
              << "-k, --api_key     API key for OpenAI GPT API\n"
              << "-s, --sys_prompt  System prompt for OpenAI GPT API\n";
}

/**
 * @brief Entry point of the application.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return Exit status. 0 indicates success.
 */
int main(int argc, char* argv[]) {
    // Variables to hold command-line arguments
    std::string prompt;
    std::string api_key;
    std::string system_prompt; // Don't initialize here

    // Handle command-line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            // If the help argument was passed, print the help message and exit
            print_help();
            return 0;
        } else if (arg == "-k" || arg == "--api_key") {
            // If this is the API key argument, use the next argument as the API key
            api_key = argv[++i];
        } else if (arg == "-s" || arg == "--sys_prompt") {
            // If this is the system prompt argument, use the next argument as the system prompt
            system_prompt = argv[++i];
        } else {
            // If this is a positional argument, use it as the prompt
            prompt = arg;
        }
    }

    // Try to get the API key from the environment if it's not provided
    if (api_key.empty()) {
        char* env_api_key = std::getenv("OPENAI_API_KEY");
        if (env_api_key != nullptr) {
            api_key = env_api_key;
        }
    }

    // Try to get the system prompt from the environment if it's not provided
    if (system_prompt.empty()) {
        char* env_sys_prompt = std::getenv("OPENAI_SYS_PROMPT");
        if (env_sys_prompt != nullptr) {
            system_prompt = env_sys_prompt;
        }
        else {
            system_prompt = "You are a helpful assistant.";
        }
    }

    // Try to get a response from the GPT Chat API
    try {
        std::string response;
        int status = get_gpt_chat_response(prompt, response, api_key, system_prompt);
        
        // Print the response
        std::cout << "Response: " << response << std::endl;

        // Print the HTTP status
        std::cerr << "HTTP request status: " << status << std::endl;

        // Handle HTTP response status codes
        if (status == 200) {
            return 0; // 0: Success
        } else if (status == 400 || status == 404) {
            return 64; // 64: Command line usage error
        } else if (status == 401 || status == 403) {
            return 78; // 78: Configuration error
        } else if (status == 503 || status == 504) {
            return 75; // 75: Temporary failure
        } else {
            return 1; // 1: Other errors
        }
    } catch (const std::invalid_argument& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 22; // 22: Invalid argument
    }
}

