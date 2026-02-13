/// @file app_state.cpp
/// @brief Application state implementation

#include "app_state.hpp"

#include <algorithm>

namespace nive::ui {

AppState::AppState() = default;
AppState::~AppState() = default;

size_t AppState::onChange(ChangeCallback callback) {
    std::lock_guard lock(mutex_);
    size_t id = next_callback_id_++;
    callbacks_.emplace_back(id, std::move(callback));
    return id;
}

void AppState::removeCallback(size_t id) {
    std::lock_guard lock(mutex_);
    auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
                           [id](const auto& pair) { return pair.first == id; });
    if (it != callbacks_.end()) {
        callbacks_.erase(it);
    }
}

void AppState::notify(ChangeType type) {
    // Copy callbacks to avoid holding lock during invocation
    std::vector<ChangeCallback> callbacks_copy;
    {
        std::lock_guard lock(mutex_);
        callbacks_copy.reserve(callbacks_.size());
        for (const auto& [_, cb] : callbacks_) {
            callbacks_copy.push_back(cb);
        }
    }

    for (const auto& cb : callbacks_copy) {
        cb(type);
    }
}

// ===== Current Path =====

std::filesystem::path AppState::currentPath() const {
    std::lock_guard lock(mutex_);
    return current_path_;
}

void AppState::setCurrentPath(const std::filesystem::path& path) {
    {
        std::lock_guard lock(mutex_);

        // Don't navigate to same path
        if (current_path_ == path) {
            return;
        }

        // Add to history
        if (!current_path_.empty()) {
            // Remove forward history
            if (history_index_ < history_.size()) {
                history_.resize(history_index_);
            }
            history_.push_back({current_path_, std::nullopt});
            history_index_ = history_.size();
        }

        current_path_ = path;
        selection_.clear();
    }

    notify(ChangeType::CurrentPath);
    notify(ChangeType::Selection);
}

bool AppState::navigateUp() {
    std::filesystem::path parent;
    {
        std::lock_guard lock(mutex_);
        if (current_path_.empty() || !current_path_.has_parent_path()) {
            return false;
        }
        parent = current_path_.parent_path();
        if (parent == current_path_) {
            return false;
        }
    }

    setCurrentPath(parent);
    return true;
}

bool AppState::navigateBack() {
    std::filesystem::path target;
    {
        std::lock_guard lock(mutex_);
        if (history_index_ == 0 || history_.empty()) {
            return false;
        }

        // Save current path to forward history
        if (history_index_ == history_.size() && !current_path_.empty()) {
            history_.push_back({current_path_, std::nullopt});
        }

        --history_index_;
        target = history_[history_index_].path;
        current_path_ = target;
        selection_.clear();
    }

    notify(ChangeType::CurrentPath);
    notify(ChangeType::Selection);
    return true;
}

bool AppState::navigateForward() {
    std::filesystem::path target;
    {
        std::lock_guard lock(mutex_);
        if (history_index_ >= history_.size() - 1) {
            return false;
        }

        ++history_index_;
        target = history_[history_index_].path;
        current_path_ = target;
        selection_.clear();
    }

    notify(ChangeType::CurrentPath);
    notify(ChangeType::Selection);
    return true;
}

bool AppState::canNavigateBack() const {
    std::lock_guard lock(mutex_);
    return history_index_ > 0;
}

bool AppState::canNavigateForward() const {
    std::lock_guard lock(mutex_);
    return history_index_ < history_.size() - 1;
}

// ===== Directory Contents =====

std::vector<fs::FileMetadata> AppState::files() const {
    std::lock_guard lock(mutex_);
    return files_;
}

void AppState::setFiles(std::vector<fs::FileMetadata> files) {
    {
        std::lock_guard lock(mutex_);
        files_ = std::move(files);
        selection_.clear();
    }
    notify(ChangeType::DirectoryContents);
    notify(ChangeType::Selection);
}

std::optional<fs::FileMetadata> AppState::fileAt(size_t index) const {
    std::lock_guard lock(mutex_);
    if (index >= files_.size()) {
        return std::nullopt;
    }
    return files_[index];
}

std::optional<size_t> AppState::findFile(const std::wstring& name) const {
    std::lock_guard lock(mutex_);
    for (size_t i = 0; i < files_.size(); ++i) {
        if (files_[i].name == name) {
            return i;
        }
    }
    return std::nullopt;
}

// ===== Selection =====

Selection AppState::selection() const {
    std::lock_guard lock(mutex_);
    return selection_;
}

void AppState::setSelection(Selection sel) {
    {
        std::lock_guard lock(mutex_);
        selection_ = std::move(sel);
    }
    notify(ChangeType::Selection);
}

void AppState::clearSelection() {
    {
        std::lock_guard lock(mutex_);
        selection_.clear();
    }
    notify(ChangeType::Selection);
}

void AppState::selectSingle(size_t index) {
    {
        std::lock_guard lock(mutex_);
        selection_.selectSingle(index);
    }
    notify(ChangeType::Selection);
}

void AppState::toggleSelection(size_t index) {
    {
        std::lock_guard lock(mutex_);
        selection_.toggle(index);
    }
    notify(ChangeType::Selection);
}

void AppState::selectAll() {
    {
        std::lock_guard lock(mutex_);
        selection_.indices.clear();
        selection_.indices.reserve(files_.size());
        for (size_t i = 0; i < files_.size(); ++i) {
            selection_.indices.push_back(i);
        }
    }
    notify(ChangeType::Selection);
}

std::vector<fs::FileMetadata> AppState::selectedFiles() const {
    std::lock_guard lock(mutex_);
    std::vector<fs::FileMetadata> result;
    result.reserve(selection_.indices.size());
    for (size_t index : selection_.indices) {
        if (index < files_.size()) {
            result.push_back(files_[index]);
        }
    }
    return result;
}

// ===== View Mode =====

ViewMode AppState::viewMode() const {
    std::lock_guard lock(mutex_);
    return view_mode_;
}

void AppState::setViewMode(ViewMode mode) {
    {
        std::lock_guard lock(mutex_);
        if (view_mode_ == mode) {
            return;
        }
        view_mode_ = mode;
    }
    notify(ChangeType::ViewMode);
}

// ===== Image Viewer =====

std::optional<archive::VirtualPath> AppState::viewerImage() const {
    std::lock_guard lock(mutex_);
    return viewer_image_;
}

void AppState::setViewerImage(const archive::VirtualPath& path) {
    {
        std::lock_guard lock(mutex_);
        viewer_image_ = path;
    }
    notify(ChangeType::ViewerImage);
}

void AppState::clearViewerImage() {
    {
        std::lock_guard lock(mutex_);
        viewer_image_.reset();
    }
    notify(ChangeType::ViewerImage);
}

bool AppState::isViewerOpen() const {
    std::lock_guard lock(mutex_);
    return viewer_image_.has_value();
}

bool AppState::viewerNext() {
    archive::VirtualPath next_image;
    {
        std::lock_guard lock(mutex_);
        if (!viewer_image_) {
            return false;
        }

        // Find current image in file list
        auto current_name = viewer_image_->filename();
        std::optional<size_t> current_index;
        for (size_t i = 0; i < files_.size(); ++i) {
            if (files_[i].name == current_name) {
                current_index = i;
                break;
            }
        }

        if (!current_index) {
            return false;
        }

        // Find next image
        for (size_t i = *current_index + 1; i < files_.size(); ++i) {
            if (files_[i].is_image()) {
                next_image = files_[i].virtual_path.value_or(
                    archive::VirtualPath(files_[i].path));
                break;
            }
        }

        if (next_image.empty()) {
            return false;
        }

        viewer_image_ = next_image;
    }

    notify(ChangeType::ViewerImage);
    return true;
}

bool AppState::viewerPrevious() {
    archive::VirtualPath prev_image;
    {
        std::lock_guard lock(mutex_);
        if (!viewer_image_) {
            return false;
        }

        auto current_name = viewer_image_->filename();
        std::optional<size_t> current_index;
        for (size_t i = 0; i < files_.size(); ++i) {
            if (files_[i].name == current_name) {
                current_index = i;
                break;
            }
        }

        if (!current_index || *current_index == 0) {
            return false;
        }

        // Find previous image
        for (size_t i = *current_index; i > 0; --i) {
            if (files_[i - 1].is_image()) {
                prev_image = files_[i - 1].virtual_path.value_or(
                    archive::VirtualPath(files_[i - 1].path));
                break;
            }
        }

        if (prev_image.empty()) {
            return false;
        }

        viewer_image_ = prev_image;
    }

    notify(ChangeType::ViewerImage);
    return true;
}

}  // namespace nive::ui
