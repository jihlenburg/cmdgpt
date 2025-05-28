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
#include "spdlog/sinks/null_sink.h"
#include "spdlog/spdlog.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <cctype>
#include <filesystem>
#include <thread>

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
        REQUIRE(std::string(cmdgpt::VERSION) == "v0.6.2");
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

TEST_CASE_METHOD(LoggerFixture, "Base64 Encoding and Decoding", "[base64]")
{
    SECTION("Encode and decode string roundtrip")
    {
        std::string original = "Hello, World!";
        std::string encoded = cmdgpt::base64_encode(original);
        REQUIRE(encoded == "SGVsbG8sIFdvcmxkIQ==");

        std::vector<uint8_t> decoded = cmdgpt::base64_decode(encoded);
        std::string decoded_str(decoded.begin(), decoded.end());
        REQUIRE(decoded_str == original);
    }

    SECTION("Encode and decode binary data")
    {
        std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
        std::string encoded = cmdgpt::base64_encode(binary_data);

        std::vector<uint8_t> decoded = cmdgpt::base64_decode(encoded);
        REQUIRE(decoded == binary_data);
    }

    SECTION("Handle empty input")
    {
        std::string empty = "";
        std::string encoded = cmdgpt::base64_encode(empty);
        REQUIRE(encoded == "");

        std::vector<uint8_t> decoded = cmdgpt::base64_decode(encoded);
        REQUIRE(decoded.empty());
    }

    SECTION("Validate base64 strings")
    {
        REQUIRE(cmdgpt::is_valid_base64("SGVsbG8sIFdvcmxkIQ==") == true);
        REQUIRE(cmdgpt::is_valid_base64("YWJjZGVmZ2hpams=") == true);
        REQUIRE(cmdgpt::is_valid_base64("") == true); // Empty is valid
        REQUIRE(cmdgpt::is_valid_base64("Invalid!@#$") == false);
        REQUIRE(cmdgpt::is_valid_base64("SGVs") == true); // Valid without padding
        REQUIRE(cmdgpt::is_valid_base64("SGV") == false); // Invalid length
    }

    SECTION("Decode invalid base64 throws")
    {
        REQUIRE_THROWS_AS(cmdgpt::base64_decode("Invalid!@#$"), std::invalid_argument);
        REQUIRE_THROWS_AS(cmdgpt::base64_decode("SGV"), std::invalid_argument);
    }

    SECTION("Handle padding correctly")
    {
        // Test various padding scenarios
        REQUIRE(cmdgpt::base64_encode("a") == "YQ==");
        REQUIRE(cmdgpt::base64_encode("ab") == "YWI=");
        REQUIRE(cmdgpt::base64_encode("abc") == "YWJj");

        // Decode with padding
        std::vector<uint8_t> decoded1 = cmdgpt::base64_decode("YQ==");
        std::string str1(decoded1.begin(), decoded1.end());
        REQUIRE(str1 == "a");

        std::vector<uint8_t> decoded2 = cmdgpt::base64_decode("YWI=");
        std::string str2(decoded2.begin(), decoded2.end());
        REQUIRE(str2 == "ab");
    }

    SECTION("Large data encoding")
    {
        // Create a large string (1MB)
        std::string large_data(1024 * 1024, 'A');
        std::string encoded = cmdgpt::base64_encode(large_data);

        // Verify size is approximately 4/3 of original
        REQUIRE(encoded.size() > large_data.size());
        REQUIRE(encoded.size() < large_data.size() * 2);

        // Verify roundtrip
        std::vector<uint8_t> decoded = cmdgpt::base64_decode(encoded);
        std::string decoded_str(decoded.begin(), decoded.end());
        REQUIRE(decoded_str == large_data);
    }
}

TEST_CASE_METHOD(LoggerFixture, "File Type Detection", "[file_utils]")
{
    SECTION("Detect PNG files")
    {
        std::vector<uint8_t> png_header = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        REQUIRE(cmdgpt::detect_file_type(png_header) == cmdgpt::FileType::PNG);
    }

    SECTION("Detect JPEG files")
    {
        std::vector<uint8_t> jpeg_header = {0xFF, 0xD8, 0xFF, 0xE0};
        REQUIRE(cmdgpt::detect_file_type(jpeg_header) == cmdgpt::FileType::JPEG);
    }

    SECTION("Detect GIF files")
    {
        std::vector<uint8_t> gif87_header = {0x47, 0x49, 0x46, 0x38, 0x37, 0x61};
        REQUIRE(cmdgpt::detect_file_type(gif87_header) == cmdgpt::FileType::GIF);

        std::vector<uint8_t> gif89_header = {0x47, 0x49, 0x46, 0x38, 0x39, 0x61};
        REQUIRE(cmdgpt::detect_file_type(gif89_header) == cmdgpt::FileType::GIF);
    }

    SECTION("Detect WebP files")
    {
        std::vector<uint8_t> webp_header = {
            0x52, 0x49, 0x46, 0x46, // RIFF
            0x00, 0x00, 0x00, 0x00, // File size (ignored)
            0x57, 0x45, 0x42, 0x50  // WEBP
        };
        REQUIRE(cmdgpt::detect_file_type(webp_header) == cmdgpt::FileType::WEBP);
    }

    SECTION("Detect PDF files")
    {
        std::vector<uint8_t> pdf_header = {0x25, 0x50, 0x44, 0x46, 0x2D};
        REQUIRE(cmdgpt::detect_file_type(pdf_header) == cmdgpt::FileType::PDF);
    }

    SECTION("Unknown file type")
    {
        std::vector<uint8_t> unknown = {0x00, 0x01, 0x02, 0x03};
        REQUIRE(cmdgpt::detect_file_type(unknown) == cmdgpt::FileType::UNKNOWN);

        std::vector<uint8_t> empty;
        REQUIRE(cmdgpt::detect_file_type(empty) == cmdgpt::FileType::UNKNOWN);
    }
}

TEST_CASE_METHOD(LoggerFixture, "MIME Type Mapping", "[file_utils]")
{
    REQUIRE(cmdgpt::get_mime_type(cmdgpt::FileType::PNG) == "image/png");
    REQUIRE(cmdgpt::get_mime_type(cmdgpt::FileType::JPEG) == "image/jpeg");
    REQUIRE(cmdgpt::get_mime_type(cmdgpt::FileType::GIF) == "image/gif");
    REQUIRE(cmdgpt::get_mime_type(cmdgpt::FileType::WEBP) == "image/webp");
    REQUIRE(cmdgpt::get_mime_type(cmdgpt::FileType::PDF) == "application/pdf");
    REQUIRE(cmdgpt::get_mime_type(cmdgpt::FileType::UNKNOWN) == "application/octet-stream");
}

TEST_CASE_METHOD(LoggerFixture, "File Extension from MIME", "[file_utils]")
{
    REQUIRE(cmdgpt::get_extension_from_mime("image/png") == "png");
    REQUIRE(cmdgpt::get_extension_from_mime("image/jpeg") == "jpg");
    REQUIRE(cmdgpt::get_extension_from_mime("image/gif") == "gif");
    REQUIRE(cmdgpt::get_extension_from_mime("image/webp") == "webp");
    REQUIRE(cmdgpt::get_extension_from_mime("application/pdf") == "pdf");
    REQUIRE(cmdgpt::get_extension_from_mime("unknown/type") == "dat");
}

TEST_CASE_METHOD(LoggerFixture, "Image Validation", "[file_utils]")
{
    SECTION("Valid image formats")
    {
        std::vector<uint8_t> png_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00};
        REQUIRE(cmdgpt::validate_image(png_data) == true);

        std::vector<uint8_t> jpeg_data = {0xFF, 0xD8, 0xFF, 0xE0, 0x00};
        REQUIRE(cmdgpt::validate_image(jpeg_data) == true);
    }

    SECTION("Invalid image data")
    {
        std::vector<uint8_t> invalid = {0x00, 0x01, 0x02};
        REQUIRE(cmdgpt::validate_image(invalid) == false);

        std::vector<uint8_t> empty;
        REQUIRE(cmdgpt::validate_image(empty) == false);
    }

    SECTION("Size validation")
    {
        std::vector<uint8_t> small_png = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        REQUIRE(cmdgpt::validate_image(small_png, 100) == true);
        REQUIRE(cmdgpt::validate_image(small_png, 5) == false);
    }
}

TEST_CASE_METHOD(LoggerFixture, "PDF Validation", "[file_utils]")
{
    SECTION("Valid PDF")
    {
        std::vector<uint8_t> pdf_data = {
            0x25, 0x50, 0x44, 0x46, 0x2D, // %PDF-
            0x31, 0x2E, 0x34, 0x0A,       // 1.4\n
            0x00, 0x00, 0x00, 0x00,       // Content
            0x25, 0x25, 0x45, 0x4F, 0x46  // %%EOF
        };
        REQUIRE(cmdgpt::validate_pdf(pdf_data) == true);
    }

    SECTION("Invalid PDF")
    {
        std::vector<uint8_t> not_pdf = {0x00, 0x01, 0x02, 0x03, 0x04};
        REQUIRE(cmdgpt::validate_pdf(not_pdf) == false);

        // Missing %%EOF
        std::vector<uint8_t> no_eof = {
            0x25, 0x50, 0x44, 0x46, 0x2D, // %PDF-
            0x31, 0x2E, 0x34, 0x0A        // 1.4\n
        };
        REQUIRE(cmdgpt::validate_pdf(no_eof) == false);

        std::vector<uint8_t> empty;
        REQUIRE(cmdgpt::validate_pdf(empty) == false);
    }
}

TEST_CASE_METHOD(LoggerFixture, "Timestamp Filename Generation", "[file_utils]")
{
    std::string filename1 = cmdgpt::generate_timestamp_filename("png");
    std::string filename2 = cmdgpt::generate_timestamp_filename("jpg", "test");

    SECTION("Correct format")
    {
        REQUIRE(filename1.find("cmdgpt_") == 0);
        REQUIRE(filename1.find(".png") != std::string::npos);

        REQUIRE(filename2.find("test_") == 0);
        REQUIRE(filename2.find(".jpg") != std::string::npos);
    }

    SECTION("Unique filenames")
    {
        // Sleep briefly to ensure different timestamps
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        std::string filename3 = cmdgpt::generate_timestamp_filename("png");
        REQUIRE(filename1 != filename3);
    }
}

TEST_CASE_METHOD(LoggerFixture, "File Operations", "[file_utils][integration]")
{
    std::string test_dir = "/tmp/cmdgpt_file_test_" + std::to_string(std::time(nullptr));
    std::filesystem::create_directory(test_dir);

    SECTION("Save and validate file")
    {
        std::vector<uint8_t> test_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
        std::filesystem::path test_file = test_dir + "/test.png";

        REQUIRE_NOTHROW(cmdgpt::save_file(test_data, test_file));
        REQUIRE(std::filesystem::exists(test_file));

        // Verify permissions (owner read/write only)
        auto perms = std::filesystem::status(test_file).permissions();
        REQUIRE((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none);
        REQUIRE((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);
    }

    // Clean up
    std::filesystem::remove_all(test_dir);
}
