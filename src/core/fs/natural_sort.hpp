/// @file natural_sort.hpp
/// @brief Natural sorting algorithm for filenames
///
/// Natural sort orders strings with embedded numbers in a human-friendly way:
/// "file1", "file2", "file10" instead of "file1", "file10", "file2"

#pragma once

#include <string>
#include <string_view>

namespace nive::fs {

/// @brief Compare two strings using natural sort order
/// @param a First string
/// @param b Second string
/// @return Negative if a < b, positive if a > b, zero if equal
///
/// Case-insensitive comparison that treats embedded numbers as numeric values.
[[nodiscard]] int naturalCompare(std::wstring_view a, std::wstring_view b) noexcept;

/// @brief Comparator for natural sorting
///
/// Use with std::sort:
/// @code
/// std::sort(files.begin(), files.end(), NaturalComparator{});
/// @endcode
struct NaturalComparator {
    [[nodiscard]] bool operator()(std::wstring_view a, std::wstring_view b) const noexcept {
        return naturalCompare(a, b) < 0;
    }
};

/// @brief Comparator for natural sorting in descending order
struct NaturalComparatorDesc {
    [[nodiscard]] bool operator()(std::wstring_view a, std::wstring_view b) const noexcept {
        return naturalCompare(a, b) > 0;
    }
};

}  // namespace nive::fs
