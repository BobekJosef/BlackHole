#pragma once
// Checks shared by the unit tests, reported through doctest at the caller's line with the values involved.
// Written so that NaN fails (a comparison with NaN is false).

#include <doctest/doctest.h>

#include <cmath>
#include <format>
#include <source_location>
#include <string_view>

namespace bh::test {

inline void expect(const bool condition, const std::string_view what,
                   const std::source_location where = std::source_location::current())
{
    if (!condition) {
        ADD_FAIL_CHECK_AT(where.file_name(), static_cast<int>(where.line()), what);
    }
}

inline void expectNear(const double actual, const double expected, const double tolerance, const std::string_view what,
                       const std::source_location where = std::source_location::current())
{
    if (!(std::abs(actual - expected) <= tolerance)) {
        ADD_FAIL_CHECK_AT(where.file_name(), static_cast<int>(where.line()),
                          std::format("{}: got {}, expected {} +- {}", what, actual, expected, tolerance));
    }
}

inline void expectRelative(const double actual, const double expected, const double tolerance,
                           const std::string_view what,
                           const std::source_location where = std::source_location::current())
{
    if (!(std::abs(actual - expected) <= tolerance * std::abs(expected))) {
        ADD_FAIL_CHECK_AT(
            where.file_name(), static_cast<int>(where.line()),
            std::format("{}: got {}, expected {} (relative tolerance {})", what, actual, expected, tolerance));
    }
}

} // namespace bh::test
