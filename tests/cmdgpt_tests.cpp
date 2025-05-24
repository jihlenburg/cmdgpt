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
#include "spdlog/sinks/null_sink.h"
#include "spdlog/spdlog.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

// Test fixture to initialize logger
struct LoggerFixture
{
    LoggerFixture()
    {
        // Create a null sink logger for tests to avoid file I/O
        auto null_sink = std::make_shared<spdlog::sinks::null_sink_mt>();
        gLogger = std::make_shared<spdlog::logger>("test_logger", null_sink);
        gLogger->set_level(spdlog::level::off);
    }
};

TEST_CASE_METHOD(LoggerFixture, "Constants are defined correctly")
{
    SECTION("Model defaults")
    {
        REQUIRE(std::string(DEFAULT_MODEL) == "gpt-4");
        REQUIRE(std::string(DEFAULT_SYSTEM_PROMPT) == "You are a helpful assistant!");
    }

    SECTION("HTTP status codes")
    {
        REQUIRE(HTTP_OK == 200);
        REQUIRE(HTTP_BAD_REQUEST == 400);
        REQUIRE(HTTP_UNAUTHORIZED == 401);
        REQUIRE(HTTP_FORBIDDEN == 403);
        REQUIRE(HTTP_NOT_FOUND == 404);
        REQUIRE(HTTP_INTERNAL_SERVER_ERROR == 500);
        REQUIRE(EMPTY_RESPONSE_CODE == -1);
    }

    SECTION("API configuration")
    {
        REQUIRE(std::string(SERVER_URL) == "https://api.openai.com");
        REQUIRE(std::string(URL) == "/v1/chat/completions");
    }

    SECTION("JSON keys")
    {
        REQUIRE(std::string(MODEL_KEY) == "model");
        REQUIRE(std::string(MESSAGES_KEY) == "messages");
        REQUIRE(std::string(ROLE_KEY) == "role");
        REQUIRE(std::string(CONTENT_KEY) == "content");
        REQUIRE(std::string(CHOICES_KEY) == "choices");
        REQUIRE(std::string(FINISH_REASON_KEY) == "finish_reason");
    }
}

TEST_CASE_METHOD(LoggerFixture, "API key validation")
{
    std::string response;

    SECTION("Throws when API key is empty")
    {
        REQUIRE_THROWS_AS(get_gpt_chat_response("test prompt", response, "", "system prompt"),
                          std::invalid_argument);
    }

    SECTION("Throws when system prompt is empty")
    {
        REQUIRE_THROWS_AS(get_gpt_chat_response("test prompt", response, "api_key", ""),
                          std::invalid_argument);
    }

    SECTION("Throws when both are empty")
    {
        REQUIRE_THROWS_AS(get_gpt_chat_response("test prompt", response, "", ""),
                          std::invalid_argument);
    }

    SECTION("Exception message is descriptive")
    {
        REQUIRE_THROWS_WITH(
            get_gpt_chat_response("test prompt", response, "", ""),
            Catch::Matchers::ContainsSubstring("API key and system prompt must be provided."));
    }
}
