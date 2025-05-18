#ifndef CMDGPT_H
#define CMDGPT_H

#include <string>
#include <map>
#include <memory>
#include "spdlog/spdlog.h"

// The current version
#define CMDGPT_VERSION "v0.1"

// Preprocessor defines for constants
#define DEFAULT_MODEL "gpt-4"
#define DEFAULT_SYSTEM_PROMPT "You are a helpful assistant!"
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

extern std::shared_ptr<spdlog::logger> gLogger;

int get_gpt_chat_response(const std::string& prompt,
                          std::string& response,
                          const std::string& api_key = "",
                          const std::string& system_prompt = "",
                          const std::string& model = DEFAULT_MODEL);

#endif // CMDGPT_H
