/// @file thumbnail_data.cpp
/// @brief Thumbnail cache data implementation

#include "thumbnail_data.hpp"

#include <format>

#include "../util/hash.hpp"
#include "../util/string_utils.hpp"

namespace nive::cache {

std::string generateCacheKey(const std::filesystem::path& path,
                             std::chrono::system_clock::time_point mtime) {
    // Create a unique key from path + modification time
    auto mtime_epoch =
        std::chrono::duration_cast<std::chrono::milliseconds>(mtime.time_since_epoch()).count();

    std::string key_source = std::format("{}|{}", pathToUtf8(path), mtime_epoch);

    auto hash_result = sha256Hex(key_source);

    if (hash_result) {
        return *hash_result;
    }

    // Fallback: use path hash only
    return sha256Hex(pathToUtf8(path)).value_or("");
}

std::string generateCacheKey(const std::filesystem::path& path) {
    std::error_code ec;
    auto mtime = std::filesystem::last_write_time(path, ec);

    if (ec) {
        // If we can't get mtime, just use path
        return sha256Hex(pathToUtf8(path)).value_or("");
    }

    // Convert file_time to system_clock
    auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(mtime);

    return generateCacheKey(path, sys_time);
}

}  // namespace nive::cache
