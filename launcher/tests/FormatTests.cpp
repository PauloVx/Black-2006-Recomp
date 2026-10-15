#include "core/Format.h"

#include <doctest/doctest.h>

#include <limits>

using namespace black::core;

TEST_CASE("Sizes read the way the launcher shows them") {
    CHECK(formatBytes(0) == "0 B");
    CHECK(formatBytes(1023) == "1023 B");
    CHECK(formatBytes(12 * 1024) == "12 KB");
    CHECK(formatBytes(512ULL * 1024 * 1024) == "512 MB");
    CHECK(formatBytes(7301444403ULL) == "6.8 GB");
    CHECK(formatBytes(3ULL * 1024 * 1024 * 1024 * 1024) == "3.0 TB");
}

TEST_CASE("Speeds and time left") {
    CHECK(formatRate(85.0 * 1024 * 1024) == "85 MB/S");
    CHECK(formatRate(0) == "-");
    CHECK(formatRate(std::numeric_limits<double>::quiet_NaN()) == "-");
    CHECK(formatTimeLeft(20) == "LESS THAN A MINUTE LEFT");
    CHECK(formatTimeLeft(125) == "ABOUT 2 MIN LEFT");
    CHECK(formatTimeLeft(3900) == "ABOUT 1 H 05 MIN LEFT");
    CHECK(formatTimeLeft(-1).empty());
}
