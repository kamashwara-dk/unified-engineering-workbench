// Phase-0 smoke test: verifies the build/test pipeline only. Not evidence of any feature.
#include <regex>

#include "catch_amalgamated.hpp"
#include "workbench/version.hpp"

TEST_CASE("core library links and reports a semantic version", "[smoke]") {
    const std::string v = workbench::core_version();
    REQUIRE_FALSE(v.empty());
    REQUIRE(std::regex_match(v, std::regex(R"(\d+\.\d+\.\d+)")));
}
