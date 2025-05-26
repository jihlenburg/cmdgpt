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
        REQUIRE(std::string(cmdgpt::VERSION) == "v0.4.0");
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

    SECTION("Throws ValidationException when prompt is empty")
    {
        REQUIRE_THROWS_AS(cmdgpt::get_gpt_chat_response("", "api_key", "system prompt"),
                          cmdgpt::ValidationException);
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

TEST_CASE_METHOD(LoggerFixture, "Security Validation", "[security]")
{
    SECTION("API key validation rejects invalid characters")
    {
        REQUIRE_THROWS_AS(cmdgpt::validate_api_key("invalid\x01key"), cmdgpt::ValidationException);
        REQUIRE_THROWS_AS(cmdgpt::validate_api_key("invalid\x7Fkey"), cmdgpt::ValidationException);
    }

    SECTION("API key validation rejects empty keys")
    {
        REQUIRE_THROWS_AS(cmdgpt::validate_api_key(""), cmdgpt::ValidationException);
    }

    SECTION("API key validation rejects overly long keys")
    {
        std::string long_key(300, 'a'); // Longer than MAX_API_KEY_LENGTH
        REQUIRE_THROWS_AS(cmdgpt::validate_api_key(long_key), cmdgpt::ValidationException);
    }

    SECTION("API key validation accepts valid keys")
    {
        REQUIRE_NOTHROW(cmdgpt::validate_api_key("sk-valid123API456key789"));
    }

    SECTION("Prompt validation rejects empty prompts")
    {
        REQUIRE_THROWS_AS(cmdgpt::validate_prompt(""), cmdgpt::ValidationException);
    }

    SECTION("Prompt validation rejects overly long prompts")
    {
        std::string long_prompt(2000000, 'a'); // Longer than MAX_PROMPT_LENGTH
        REQUIRE_THROWS_AS(cmdgpt::validate_prompt(long_prompt), cmdgpt::ValidationException);
    }

    SECTION("API key redaction works correctly")
    {
        REQUIRE(cmdgpt::redact_api_key("") == "[EMPTY]");
        REQUIRE(cmdgpt::redact_api_key("short") == "*****");
        REQUIRE(cmdgpt::redact_api_key("sk-1234567890abcdef") == "sk-1***********cdef");
    }
}

TEST_CASE_METHOD(LoggerFixture, "Configuration Management", "[config]")
{
    SECTION("Config loads defaults correctly")
    {
        cmdgpt::Config config;
        REQUIRE(config.system_prompt() == cmdgpt::DEFAULT_SYSTEM_PROMPT);
        REQUIRE(config.model() == cmdgpt::DEFAULT_MODEL);
        REQUIRE(config.log_level() == cmdgpt::DEFAULT_LOG_LEVEL);
    }

    SECTION("Config validates API key on set")
    {
        cmdgpt::Config config;
        REQUIRE_THROWS_AS(config.set_api_key("invalid\x01key"), cmdgpt::ValidationException);
    }

    SECTION("Config validates system prompt length")
    {
        cmdgpt::Config config;
        std::string long_prompt(2000000, 'a');
        REQUIRE_THROWS_AS(config.set_system_prompt(long_prompt), cmdgpt::ValidationException);
    }

    SECTION("Config validates model name")
    {
        cmdgpt::Config config;
        std::string long_model(200, 'a');
        REQUIRE_THROWS_AS(config.set_model(long_model), cmdgpt::ValidationException);
    }

    SECTION("Config validates log file path")
    {
        cmdgpt::Config config;
        std::string long_path(5000, 'a');
        REQUIRE_THROWS_AS(config.set_log_file(long_path), cmdgpt::ValidationException);
    }
}

TEST_CASE_METHOD(LoggerFixture, "Output Format Parsing", "[format]")
{
    SECTION("Parse various format strings")
    {
        REQUIRE(cmdgpt::parse_output_format("plain") == cmdgpt::OutputFormat::PLAIN);
        REQUIRE(cmdgpt::parse_output_format("PLAIN") == cmdgpt::OutputFormat::PLAIN);
        REQUIRE(cmdgpt::parse_output_format("json") == cmdgpt::OutputFormat::JSON);
        REQUIRE(cmdgpt::parse_output_format("JSON") == cmdgpt::OutputFormat::JSON);
        REQUIRE(cmdgpt::parse_output_format("markdown") == cmdgpt::OutputFormat::MARKDOWN);
        REQUIRE(cmdgpt::parse_output_format("md") == cmdgpt::OutputFormat::MARKDOWN);
        REQUIRE(cmdgpt::parse_output_format("code") == cmdgpt::OutputFormat::CODE);
        REQUIRE(cmdgpt::parse_output_format("unknown") == cmdgpt::OutputFormat::PLAIN);
    }
}

TEST_CASE_METHOD(LoggerFixture, "Conversation Management", "[conversation]")
{
    SECTION("Add and retrieve messages")
    {
        cmdgpt::Conversation conv;
        REQUIRE(conv.get_messages().empty());

        conv.add_message("system", "You are helpful");
        conv.add_message("user", "Hello");
        conv.add_message("assistant", "Hi there!");

        REQUIRE(conv.get_messages().size() == 3);
        REQUIRE(conv.get_messages()[0].role == "system");
        REQUIRE(conv.get_messages()[1].role == "user");
        REQUIRE(conv.get_messages()[2].role == "assistant");
    }

    SECTION("Clear conversation")
    {
        cmdgpt::Conversation conv;
        conv.add_message("user", "Hello");
        REQUIRE(conv.get_messages().size() == 1);

        conv.clear();
        REQUIRE(conv.get_messages().empty());
    }

    SECTION("Token estimation")
    {
        cmdgpt::Conversation conv;
        conv.add_message("user", "This is a test message");

        // Rough estimate: ~7-8 tokens for this message
        size_t tokens = conv.estimate_tokens();
        REQUIRE(tokens > 0);
        REQUIRE(tokens < 20);
    }

    SECTION("JSON serialization")
    {
        cmdgpt::Conversation conv;
        conv.add_message("user", "Hello");

        std::string json = conv.to_json();
        REQUIRE(json.find("messages") != std::string::npos);
        REQUIRE(json.find("user") != std::string::npos);
        REQUIRE(json.find("Hello") != std::string::npos);
    }
}

TEST_CASE_METHOD(LoggerFixture, "Output Formatting", "[format]")
{
    std::string test_content = "This is a test response";

    SECTION("Plain format")
    {
        std::string output = cmdgpt::format_output(test_content, cmdgpt::OutputFormat::PLAIN);
        REQUIRE(output == test_content);
    }

    SECTION("JSON format")
    {
        std::string output = cmdgpt::format_output(test_content, cmdgpt::OutputFormat::JSON);
        REQUIRE(output.find("response") != std::string::npos);
        REQUIRE(output.find(test_content) != std::string::npos);
        REQUIRE(output.find("timestamp") != std::string::npos);
        REQUIRE(output.find("version") != std::string::npos);
    }

    SECTION("Markdown format")
    {
        std::string output = cmdgpt::format_output(test_content, cmdgpt::OutputFormat::MARKDOWN);
        REQUIRE(output.find("## Response") != std::string::npos);
        REQUIRE(output.find(test_content) != std::string::npos);
        REQUIRE(output.find("Generated by cmdgpt") != std::string::npos);
    }

    SECTION("Code format with code block")
    {
        std::string code_content = "Here is code:\n```python\nprint('hello')\n```\nDone.";
        std::string output = cmdgpt::format_output(code_content, cmdgpt::OutputFormat::CODE);
        REQUIRE(output == "print('hello')\n");
    }

    SECTION("Code format without code block")
    {
        std::string output = cmdgpt::format_output(test_content, cmdgpt::OutputFormat::CODE);
        REQUIRE(output == test_content);
    }
}
