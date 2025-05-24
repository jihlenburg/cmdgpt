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

#include <string>
#include <map>
#include <memory>
#include "spdlog/spdlog.h"

// Version information
#define CMDGPT_VERSION "v0.2"

// Model and prompt defaults
#define DEFAULT_MODEL "gpt-4"
#define DEFAULT_SYSTEM_PROMPT "You are a helpful assistant!"
#define DEFAULT_LOG_LEVEL spdlog::level::warn

// HTTP headers
#define AUTHORIZATION_HEADER "Authorization"
#define CONTENT_TYPE_HEADER "Content-Type"
#define APPLICATION_JSON "application/json"

// JSON keys for OpenAI API
#define SYSTEM_ROLE "system"
#define USER_ROLE "user"
#define MODEL_KEY "model"
#define MESSAGES_KEY "messages"
#define ROLE_KEY "role"
#define CONTENT_KEY "content"
#define CHOICES_KEY "choices"
#define FINISH_REASON_KEY "finish_reason"

// API configuration
#define URL "/v1/chat/completions"
#define SERVER_URL "https://api.openai.com"

// HTTP status codes
#define EMPTY_RESPONSE_CODE -1
#define HTTP_OK 200
#define HTTP_BAD_REQUEST 400
#define HTTP_UNAUTHORIZED 401
#define HTTP_FORBIDDEN 403
#define HTTP_NOT_FOUND 404
#define HTTP_INTERNAL_SERVER_ERROR 500

// Global logger instance
extern std::shared_ptr<spdlog::logger> gLogger;

/**
 * @brief Prints the help message to the console
 */
void print_help();

/**
 * @brief Sends a chat completion request to the OpenAI API
 * 
 * @param prompt The user's input prompt
 * @param response Output parameter for the API response
 * @param api_key OpenAI API key (optional if set via environment)
 * @param system_prompt System prompt to set context (optional)
 * @param model The model to use (default: gpt-4)
 * @return HTTP status code or EMPTY_RESPONSE_CODE on failure
 */
// cppcheck-suppress syntaxError
int get_gpt_chat_response(const std::string& prompt,
                          std::string& response,
                          const std::string& api_key = "",
                          const std::string& system_prompt = "",
                          const std::string& model = DEFAULT_MODEL);

#endif // CMDGPT_H