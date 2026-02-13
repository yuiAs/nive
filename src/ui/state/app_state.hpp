/// @file app_state.hpp
/// @brief Application state management

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/archive/virtual_path.hpp"
#include "core/fs/file_metadata.hpp"

namespace nive::ui {

/// @brief View mode for file display
enum class ViewMode {
    Thumbnails,  // Grid of thumbnails
    List,        // Detailed list
    Icons        // Large icons
};

/// @brief Selection state
struct Selection {
    std::vector<size_t> indices;    // Selected item indices
    std::optional<size_t> focused;  // Focused item (may not be selected)

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }

    [[nodiscard]] size_t count() const noexcept { return indices.size(); }

    [[nodiscard]] bool contains(size_t index) const noexcept {
        return std::find(indices.begin(), indices.end(), index) != indices.end();
    }

    void clear() {
        indices.clear();
        focused.reset();
    }

    void select(size_t index) {
        if (!contains(index)) {
            indices.push_back(index);
        }
        focused = index;
    }

    void deselect(size_t index) {
        auto it = std::find(indices.begin(), indices.end(), index);
        if (it != indices.end()) {
            indices.erase(it);
        }
    }

    void toggle(size_t index) {
        if (contains(index)) {
            deselect(index);
        } else {
            select(index);
        }
    }

    void selectSingle(size_t index) {
        indices.clear();
        indices.push_back(index);
        focused = index;
    }
};

/// @brief Navigation history entry
struct HistoryEntry {
    std::filesystem::path path;
    std::optional<std::wstring> scroll_to_file;
};

/// @brief Application state
///
/// Thread-safe state management for the application.
class AppState {
public:
    /// @brief State change notification types
    enum class ChangeType {
        CurrentPath,
        Selection,
        ViewMode,
        DirectoryContents,
        ViewerImage
    };

    using ChangeCallback = std::function<void(ChangeType)>;

    AppState();
    ~AppState();

    // Non-copyable
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;

    /// @brief Register change callback
    /// @return Callback ID for unregistration
    size_t onChange(ChangeCallback callback);

    /// @brief Unregister change callback
    void removeCallback(size_t id);

    // ===== Current Path =====

    /// @brief Get current directory path
    [[nodiscard]] std::filesystem::path currentPath() const;

    /// @brief Set current directory path
    void setCurrentPath(const std::filesystem::path& path);

    /// @brief Navigate to parent directory
    /// @return true if navigated
    bool navigateUp();

    /// @brief Navigate back in history
    /// @return true if navigated
    bool navigateBack();

    /// @brief Navigate forward in history
    /// @return true if navigated
    bool navigateForward();

    /// @brief Check if back navigation is available
    [[nodiscard]] bool canNavigateBack() const;

    /// @brief Check if forward navigation is available
    [[nodiscard]] bool canNavigateForward() const;

    // ===== Directory Contents =====

    /// @brief Get current directory file list
    [[nodiscard]] std::vector<fs::FileMetadata> files() const;

    /// @brief Set directory file list
    void setFiles(std::vector<fs::FileMetadata> files);

    /// @brief Get file at index
    [[nodiscard]] std::optional<fs::FileMetadata> fileAt(size_t index) const;

    /// @brief Find file index by name
    [[nodiscard]] std::optional<size_t> findFile(const std::wstring& name) const;

    // ===== Selection =====

    /// @brief Get current selection
    [[nodiscard]] Selection selection() const;

    /// @brief Set selection
    void setSelection(Selection sel);

    /// @brief Clear selection
    void clearSelection();

    /// @brief Select single item
    void selectSingle(size_t index);

    /// @brief Toggle item selection
    void toggleSelection(size_t index);

    /// @brief Select all items
    void selectAll();

    /// @brief Get selected files
    [[nodiscard]] std::vector<fs::FileMetadata> selectedFiles() const;

    // ===== View Mode =====

    /// @brief Get current view mode
    [[nodiscard]] ViewMode viewMode() const;

    /// @brief Set view mode
    void setViewMode(ViewMode mode);

    // ===== Image Viewer =====

    /// @brief Get currently viewed image path
    [[nodiscard]] std::optional<archive::VirtualPath> viewerImage() const;

    /// @brief Set image to view
    void setViewerImage(const archive::VirtualPath& path);

    /// @brief Clear viewer image
    void clearViewerImage();

    /// @brief Check if viewer is open
    [[nodiscard]] bool isViewerOpen() const;

    /// @brief Navigate to next image in current directory
    /// @return true if navigated
    bool viewerNext();

    /// @brief Navigate to previous image in current directory
    /// @return true if navigated
    bool viewerPrevious();

private:
    void notify(ChangeType type);

    mutable std::mutex mutex_;

    // Navigation
    std::filesystem::path current_path_;
    std::vector<HistoryEntry> history_;
    size_t history_index_ = 0;

    // Directory contents
    std::vector<fs::FileMetadata> files_;

    // Selection
    Selection selection_;

    // View settings
    ViewMode view_mode_ = ViewMode::Thumbnails;

    // Viewer state
    std::optional<archive::VirtualPath> viewer_image_;

    // Callbacks
    std::vector<std::pair<size_t, ChangeCallback>> callbacks_;
    size_t next_callback_id_ = 1;
};

}  // namespace nive::ui
