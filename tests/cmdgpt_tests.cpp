#include <catch2/catch.hpp>
#include "cmdgpt.h"

TEST_CASE("Constants are defined correctly") {
    REQUIRE(std::string(DEFAULT_MODEL) == "gpt-4");
    REQUIRE(HTTP_OK == 200);
}

TEST_CASE("get_gpt_chat_response throws when API key missing") {
    std::string response;
    REQUIRE_THROWS_AS(get_gpt_chat_response("hi", response, "", ""), std::invalid_argument);
}

