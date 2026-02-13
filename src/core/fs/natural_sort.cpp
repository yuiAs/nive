/// @file natural_sort.cpp
/// @brief Natural sorting algorithm implementation

#include "natural_sort.hpp"

#include <cwctype>

namespace nive::fs {

namespace {

/// @brief Check if character is a digit
[[nodiscard]] constexpr bool is_digit(wchar_t c) noexcept {
    return c >= L'0' && c <= L'9';
}

/// @brief Convert character to lowercase
[[nodiscard]] constexpr wchar_t to_lower(wchar_t c) noexcept {
    if (c >= L'A' && c <= L'Z') {
        return c + (L'a' - L'A');
    }
    return c;
}

/// @brief Skip leading zeros and return number value and length
struct NumberInfo {
    uint64_t value;
    size_t length;
    size_t leading_zeros;
};

[[nodiscard]] NumberInfo parse_number(std::wstring_view str, size_t start) noexcept {
    NumberInfo info{0, 0, 0};

    // Count leading zeros
    size_t pos = start;
    while (pos < str.size() && str[pos] == L'0') {
        ++info.leading_zeros;
        ++pos;
    }

    // Parse actual number
    while (pos < str.size() && is_digit(str[pos])) {
        // Prevent overflow
        if (info.value > (UINT64_MAX - 9) / 10) {
            // On overflow, compare by string length
            info.value = UINT64_MAX;
        } else {
            info.value = info.value * 10 + (str[pos] - L'0');
        }
        ++pos;
    }

    info.length = pos - start;
    return info;
}

}  // namespace

int naturalCompare(std::wstring_view a, std::wstring_view b) noexcept {
    size_t i = 0;
    size_t j = 0;

    while (i < a.size() && j < b.size()) {
        wchar_t ca = a[i];
        wchar_t cb = b[j];

        // Both are digits - compare as numbers
        if (is_digit(ca) && is_digit(cb)) {
            auto num_a = parse_number(a, i);
            auto num_b = parse_number(b, j);

            // Compare by numeric value
            if (num_a.value != num_b.value) {
                return (num_a.value < num_b.value) ? -1 : 1;
            }

            // Same value - compare by number of leading zeros (fewer is "smaller")
            // This ensures "01" < "001" when values are equal
            if (num_a.leading_zeros != num_b.leading_zeros) {
                return (num_a.leading_zeros < num_b.leading_zeros) ? -1 : 1;
            }

            i += num_a.length;
            j += num_b.length;
            continue;
        }

        // One is digit, one is not
        if (is_digit(ca) != is_digit(cb)) {
            // Digits come before non-digits
            return is_digit(ca) ? -1 : 1;
        }

        // Both are non-digits - compare case-insensitively
        wchar_t lower_a = to_lower(ca);
        wchar_t lower_b = to_lower(cb);

        if (lower_a != lower_b) {
            return (lower_a < lower_b) ? -1 : 1;
        }

        // If same when lowercased, use original case as tiebreaker
        // (uppercase comes before lowercase)
        if (ca != cb) {
            // Only record first case difference, don't return yet
            // Continue comparing rest of string
        }

        ++i;
        ++j;
    }

    // One string is prefix of the other
    if (a.size() != b.size()) {
        return (a.size() < b.size()) ? -1 : 1;
    }

    // Strings are equal
    return 0;
}

}  // namespace nive::fs
