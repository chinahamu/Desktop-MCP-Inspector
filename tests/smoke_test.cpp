#include <catch2/catch_test_macros.hpp>

#include <string_view>

TEST_CASE("project smoke test validates test runner", "[smoke]")
{
    constexpr std::string_view projectName = "Desktop MCP Inspector";

    REQUIRE_FALSE(projectName.empty());
    REQUIRE(projectName.find("MCP") != std::string_view::npos);
}
