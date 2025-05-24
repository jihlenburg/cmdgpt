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
    SECTION("Version and model defaults")
    {
        REQUIRE(std::string(cmdgpt::VERSION) == "v0.2");
        REQUIRE(std::string(cmdgpt::DEFAULT_MODEL) == "gpt-4");
        REQUIRE(std::string(cmdgpt::DEFAULT_SYSTEM_PROMPT) == "You are a helpful assistant!");
        REQUIRE(cmdgpt::DEFAULT_LOG_LEVEL == spdlog::level::warn);
    }

    SECTION("HTTP status codes")
    {
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::OK) == 200);
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::BAD_REQUEST) == 400);
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::UNAUTHORIZED) == 401);
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::FORBIDDEN) == 403);
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::NOT_FOUND) == 404);
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::INTERNAL_SERVER_ERROR) == 500);
        REQUIRE(static_cast<int>(cmdgpt::HttpStatus::EMPTY_RESPONSE) == -1);
    }

    SECTION("API configuration")
    {
        REQUIRE(std::string(cmdgpt::SERVER_URL) == "https://api.openai.com");
        REQUIRE(std::string(cmdgpt::API_URL) == "/v1/chat/completions");
    }

    SECTION("JSON keys")
    {
        REQUIRE(std::string(cmdgpt::MODEL_KEY) == "model");
        REQUIRE(std::string(cmdgpt::MESSAGES_KEY) == "messages");
        REQUIRE(std::string(cmdgpt::ROLE_KEY) == "role");
        REQUIRE(std::string(cmdgpt::CONTENT_KEY) == "content");
        REQUIRE(std::string(cmdgpt::CHOICES_KEY) == "choices");
        REQUIRE(std::string(cmdgpt::FINISH_REASON_KEY) == "finish_reason");
    }
}

TEST_CASE_METHOD(LoggerFixture, "API key validation")
{
    SECTION("Throws ConfigurationException when API key is empty and no environment variable")
    {
        // Ensure OPENAI_API_KEY is not set for this test
        unsetenv("OPENAI_API_KEY");
        REQUIRE_THROWS_AS(cmdgpt::get_gpt_chat_response("test prompt", "", "system prompt"),
                          cmdgpt::ConfigurationException);
    }

    SECTION("Throws ConfigurationException when prompt is empty")
    {
        REQUIRE_THROWS_AS(cmdgpt::get_gpt_chat_response("", "api_key", "system prompt"),
                          cmdgpt::ConfigurationException);
    }

    SECTION("Uses default system prompt when empty")
    {
        // This test would require mocking the HTTP client or using a test API
        // For now, we just ensure it doesn't throw a ConfigurationException
        // (it will throw ApiException when server responds with 401 for invalid API key)
        REQUIRE_THROWS_AS(cmdgpt::get_gpt_chat_response("test prompt", "fake_api_key", ""),
                          cmdgpt::ApiException);
    }

    SECTION("Exception messages are descriptive")
    {
        unsetenv("OPENAI_API_KEY");
        REQUIRE_THROWS_WITH(cmdgpt::get_gpt_chat_response("test prompt", "", ""),
                            Catch::Matchers::ContainsSubstring("API key must be provided"));

        REQUIRE_THROWS_WITH(cmdgpt::get_gpt_chat_response("", "api_key", "system prompt"),
                            Catch::Matchers::ContainsSubstring("Prompt cannot be empty"));
    }
}
