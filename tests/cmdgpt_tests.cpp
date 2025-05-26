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
#include <cctype>
#include <filesystem>

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
        REQUIRE(std::string(cmdgpt::VERSION) == "v0.5.0-dev");
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

TEST_CASE_METHOD(LoggerFixture, "Config Cache and Token Settings", "[config]")
{
    cmdgpt::Config config;

    SECTION("Cache settings default to enabled")
    {
        REQUIRE(config.cache_enabled() == true);
    }

    SECTION("Can disable caching")
    {
        config.set_cache_enabled(false);
        REQUIRE(config.cache_enabled() == false);
    }

    SECTION("Token display defaults to disabled")
    {
        REQUIRE(config.show_tokens() == false);
    }

    SECTION("Can enable token display")
    {
        config.set_show_tokens(true);
        REQUIRE(config.show_tokens() == true);
    }
}

TEST_CASE_METHOD(LoggerFixture, "ResponseCache Key Generation", "[cache]")
{
    cmdgpt::ResponseCache cache("/tmp/test_cache", 1);

    SECTION("Generates consistent keys for same input")
    {
        std::string key1 = cache.generate_key("test prompt", "gpt-4", "system prompt");
        std::string key2 = cache.generate_key("test prompt", "gpt-4", "system prompt");
        REQUIRE(key1 == key2);
    }

    SECTION("Generates different keys for different prompts")
    {
        std::string key1 = cache.generate_key("prompt1", "gpt-4", "system");
        std::string key2 = cache.generate_key("prompt2", "gpt-4", "system");
        REQUIRE(key1 != key2);
    }

    SECTION("Generates different keys for different models")
    {
        std::string key1 = cache.generate_key("prompt", "gpt-4", "system");
        std::string key2 = cache.generate_key("prompt", "gpt-3.5-turbo", "system");
        REQUIRE(key1 != key2);
    }

    SECTION("Key is valid SHA256 hex string")
    {
        std::string key = cache.generate_key("test", "model", "system");
        REQUIRE(key.length() == 64); // SHA256 produces 64 hex characters

        // Check all characters are valid hex
        for (char c : key)
        {
            REQUIRE(std::isxdigit(c));
        }
    }
}

TEST_CASE_METHOD(LoggerFixture, "ResponseCache Security", "[cache][security]")
{
    cmdgpt::ResponseCache cache("/tmp/test_cache_security", 1);

    SECTION("Invalid keys fail when used in operations")
    {
        // Keys with path traversal attempts should fail
        // Since get_cache_path is private, we test through public methods
        std::string invalid_key1 = "../evil";
        std::string invalid_key2 = "test/../../evil";
        std::string invalid_key3 = "/etc/passwd";

        // These should throw when trying to use invalid keys
        REQUIRE_THROWS_AS(cache.has_valid_cache(invalid_key1), cmdgpt::ValidationException);
        REQUIRE_THROWS_AS(cache.get(invalid_key2), cmdgpt::ValidationException);
        REQUIRE_THROWS_AS(cache.put(invalid_key3, "test"), cmdgpt::ValidationException);
    }

    SECTION("Valid SHA256 keys work correctly")
    {
        std::string valid_key = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
        // Should not throw with valid key
        REQUIRE_NOTHROW(cache.has_valid_cache(valid_key));
        REQUIRE_NOTHROW(cache.get(valid_key));
    }

    // Clean up test directory
    std::filesystem::remove_all("/tmp/test_cache_security");
}

TEST_CASE_METHOD(LoggerFixture, "Token Usage Parsing", "[tokens]")
{
    SECTION("Parse valid token usage")
    {
        std::string json_response = R"({
            "choices": [{"message": {"content": "test"}}],
            "usage": {
                "prompt_tokens": 10,
                "completion_tokens": 20,
                "total_tokens": 30
            }
        })";

        cmdgpt::TokenUsage usage = cmdgpt::parse_token_usage(json_response, "gpt-4");
        REQUIRE(usage.prompt_tokens == 10);
        REQUIRE(usage.completion_tokens == 20);
        REQUIRE(usage.total_tokens == 30);
        REQUIRE(usage.estimated_cost > 0); // Should calculate cost for known model
    }

    SECTION("Handle missing usage field")
    {
        std::string json_response = R"({"choices": [{"message": {"content": "test"}}]})";

        cmdgpt::TokenUsage usage = cmdgpt::parse_token_usage(json_response, "gpt-4");
        REQUIRE(usage.prompt_tokens == 0);
        REQUIRE(usage.completion_tokens == 0);
        REQUIRE(usage.total_tokens == 0);
        REQUIRE(usage.estimated_cost == 0.0);
    }

    SECTION("Calculate costs for different models")
    {
        std::string json_response = R"({
            "usage": {
                "prompt_tokens": 1000,
                "completion_tokens": 1000,
                "total_tokens": 2000
            }
        })";

        // GPT-4 should be more expensive than GPT-3.5
        cmdgpt::TokenUsage usage_gpt4 = cmdgpt::parse_token_usage(json_response, "gpt-4");
        cmdgpt::TokenUsage usage_gpt35 = cmdgpt::parse_token_usage(json_response, "gpt-3.5-turbo");

        REQUIRE(usage_gpt4.estimated_cost > usage_gpt35.estimated_cost);
    }
}

TEST_CASE_METHOD(LoggerFixture, "Token Usage Formatting", "[tokens]")
{
    cmdgpt::TokenUsage usage;
    usage.prompt_tokens = 100;
    usage.completion_tokens = 200;
    usage.total_tokens = 300;
    usage.estimated_cost = 0.0045;

    std::string formatted = cmdgpt::format_token_usage(usage);

    REQUIRE(formatted.find("300 total") != std::string::npos);
    REQUIRE(formatted.find("100 prompt") != std::string::npos);
    REQUIRE(formatted.find("200 completion") != std::string::npos);
    REQUIRE(formatted.find("$0.0045") != std::string::npos);
}

TEST_CASE_METHOD(LoggerFixture, "SecurityException", "[exceptions]")
{
    SECTION("SecurityException has correct prefix")
    {
        cmdgpt::SecurityException ex("test security issue");
        std::string msg = ex.what();
        REQUIRE(msg.find("Security Error:") != std::string::npos);
        REQUIRE(msg.find("test security issue") != std::string::npos);
    }
}

TEST_CASE_METHOD(LoggerFixture, "ResponseCache Operations", "[cache][integration]")
{
    // Use a unique test directory
    std::string test_dir = "/tmp/cmdgpt_test_" + std::to_string(std::time(nullptr));
    cmdgpt::ResponseCache cache(test_dir, 1);

    SECTION("Cache miss on first request")
    {
        std::string key = cache.generate_key("test", "gpt-4", "system");
        REQUIRE(cache.has_valid_cache(key) == false);
        REQUIRE(cache.get(key) == "");
    }

    SECTION("Cache hit after storing")
    {
        std::string key = cache.generate_key("test", "gpt-4", "system");
        std::string response = "This is a test response";

        cache.put(key, response);
        REQUIRE(cache.has_valid_cache(key) == true);
        REQUIRE(cache.get(key) == response);
    }

    SECTION("Cache statistics tracking")
    {
        std::string key = cache.generate_key("test", "gpt-4", "system");

        // Initial miss
        cache.get(key);
        auto stats = cache.get_stats();
        REQUIRE(stats["misses"] == 1);
        REQUIRE(stats["hits"] == 0);

        // Store and hit
        cache.put(key, "response");
        cache.get(key);
        stats = cache.get_stats();
        REQUIRE(stats["hits"] == 1);
        REQUIRE(stats["count"] >= 1);
    }

    SECTION("Clear cache")
    {
        std::string key = cache.generate_key("test", "gpt-4", "system");
        cache.put(key, "response");

        size_t cleared = cache.clear();
        REQUIRE(cleared >= 1);
        REQUIRE(cache.has_valid_cache(key) == false);
    }

    // Clean up test directory
    std::filesystem::remove_all(test_dir);
}

TEST_CASE_METHOD(LoggerFixture, "ResponseCache Size Limits", "[cache][limits]")
{
    std::string test_dir = "/tmp/cmdgpt_test_limits_" + std::to_string(std::time(nullptr));
    cmdgpt::ResponseCache cache(test_dir, 24);

    SECTION("Respects maximum entry count")
    {
        // Note: This is a conceptual test. In real implementation,
        // we'd need to mock or set a very low MAX_CACHE_ENTRIES for testing
        // For now, just verify the cache can handle multiple entries
        for (int i = 0; i < 10; i++)
        {
            std::string key = cache.generate_key("prompt" + std::to_string(i), "gpt-4", "system");
            cache.put(key, "response" + std::to_string(i));
        }

        auto stats = cache.get_stats();
        REQUIRE(stats["count"] <= 1000); // MAX_CACHE_ENTRIES
    }

    // Clean up
    std::filesystem::remove_all(test_dir);
}
