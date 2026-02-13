/// @file virtual_path.cpp
/// @brief Virtual path implementation

#include "virtual_path.hpp"

#include <algorithm>

namespace nive::archive {

VirtualPath::VirtualPath(const std::filesystem::path& path)
    : archive_path_(path), internal_path_() {
}

VirtualPath::VirtualPath(const std::filesystem::path& archive_path,
                         const std::wstring& internal_path)
    : archive_path_(archive_path), internal_path_(internal_path) {
    // Normalize internal path separators to forward slash
    std::replace(internal_path_.begin(), internal_path_.end(), L'\\', L'/');

    // Remove leading slash
    while (!internal_path_.empty() && internal_path_.front() == L'/') {
        internal_path_.erase(0, 1);
    }
}

VirtualPath VirtualPath::parse(const std::wstring& path_string) {
    auto sep_pos = path_string.find(kVirtualPathSeparator);

    if (sep_pos == std::wstring::npos) {
        // Regular filesystem path
        return VirtualPath(std::filesystem::path(path_string));
    }

    // Virtual path: archive|internal
    std::wstring archive_part = path_string.substr(0, sep_pos);
    std::wstring internal_part = path_string.substr(sep_pos + 1);

    return VirtualPath(std::filesystem::path(archive_part), internal_part);
}

std::wstring VirtualPath::filename() const {
    if (is_in_archive()) {
        // Get last component of internal path
        auto pos = internal_path_.find_last_of(L"/\\");
        if (pos == std::wstring::npos) {
            return internal_path_;
        }
        return internal_path_.substr(pos + 1);
    }
    return archive_path_.filename().wstring();
}

std::wstring VirtualPath::extension() const {
    std::wstring name = filename();
    auto pos = name.find_last_of(L'.');
    if (pos == std::wstring::npos || pos == 0) {
        return L"";
    }
    return name.substr(pos);
}

VirtualPath VirtualPath::parent_path() const {
    if (is_in_archive()) {
        auto pos = internal_path_.find_last_of(L"/\\");
        if (pos == std::wstring::npos) {
            // No more internal path - return archive root
            return VirtualPath(archive_path_, L"");
        }
        return VirtualPath(archive_path_, internal_path_.substr(0, pos));
    }

    // Regular filesystem path
    auto parent = archive_path_.parent_path();
    if (parent.empty() || parent == archive_path_) {
        return VirtualPath();
    }
    return VirtualPath(parent);
}

std::wstring VirtualPath::to_string() const {
    if (is_in_archive()) {
        return archive_path_.wstring() + kVirtualPathSeparator + internal_path_;
    }
    return archive_path_.wstring();
}

bool VirtualPath::operator==(const VirtualPath& other) const noexcept {
    return archive_path_ == other.archive_path_ && internal_path_ == other.internal_path_;
}

bool VirtualPath::operator<(const VirtualPath& other) const noexcept {
    if (archive_path_ != other.archive_path_) {
        return archive_path_ < other.archive_path_;
    }
    return internal_path_ < other.internal_path_;
}

VirtualPath operator/(const VirtualPath& parent, const std::wstring& child) {
    if (parent.is_in_archive() || parent.internal_path().empty()) {
        // Append to internal path
        std::wstring new_internal;
        if (!parent.internal_path().empty()) {
            new_internal = parent.internal_path() + L"/" + child;
        } else {
            new_internal = child;
        }
        return VirtualPath(parent.archive_path(), new_internal);
    }

    // Regular filesystem path
    return VirtualPath(parent.archive_path() / child);
}

}  // namespace nive::archive
