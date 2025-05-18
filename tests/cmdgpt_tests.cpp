#include "catch2/catch.hpp"
#include "cmdgpt.cpp"

TEST_CASE("get_gpt_chat_response parses response", "[get_gpt_chat_response]") {
    std::string output;
    auto stub = [](const std::string&, const httplib::Headers&, const std::string&, const char*) {
        auto r = std::make_shared<httplib::Response>();
        r->status = HTTP_OK;
        r->body = R"({"choices":[{"finish_reason":"stop","message":{"content":"Hi"}}]})";
        return r;
    };
    int status = get_gpt_chat_response("hello", output, "key", "sys", DEFAULT_MODEL, stub);
    REQUIRE(status == HTTP_OK);
    REQUIRE(output == "Hi");
}

TEST_CASE("get_gpt_chat_response throws without key", "[get_gpt_chat_response]") {
    std::string output;
    auto stub = [](const std::string&, const httplib::Headers&, const std::string&, const char*) {
        auto r = std::make_shared<httplib::Response>();
        r->status = HTTP_OK;
        r->body = "{}";
        return r;
    };
    REQUIRE_THROWS_AS(get_gpt_chat_response("hello", output, "", "sys", DEFAULT_MODEL, stub), std::invalid_argument);
}
