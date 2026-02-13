/// @file d2d_delete_confirm_dialog.hpp
/// @brief D2D-rendered dialog for confirming file deletion

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <filesystem>
#include <vector>

#include "core/util/com_ptr.hpp"
#include "ui/d2d/components/button.hpp"
#include "ui/d2d/components/checkbox.hpp"
#include "ui/d2d/components/label.hpp"
#include "ui/d2d/components/listbox.hpp"
#include "ui/d2d/dialog/d2d_dialog.hpp"

namespace nive::ui {

/// @brief Result of the delete confirmation dialog
enum class DeleteConfirmResult {
    Delete,  // User confirmed permanent deletion
    Trash,   // User chose to move to trash
    Cancel,  // User cancelled
};

/// @brief Options for the delete confirmation dialog
struct DeleteConfirmOptions {
    bool show_trash_option = true;  // Show "Move to Trash" option
    bool default_to_trash = true;   // Default selection is Trash
};

}  // namespace nive::ui

namespace nive::ui::d2d {

/// @brief D2D-rendered dialog for confirming file deletion
///
/// Shows a warning message and file list (when multiple files),
/// with optional "Move to Trash" checkbox.
class D2DDeleteConfirmDialog : public D2DDialog {
public:
    D2DDeleteConfirmDialog();
    ~D2DDeleteConfirmDialog() override = default;

    /// @brief Show the delete confirmation dialog
    /// @param parent Parent window handle
    /// @param files Files to be deleted
    /// @param options Dialog options
    /// @return User's choice
    DeleteConfirmResult show(HWND parent, const std::vector<std::filesystem::path>& files,
                             const DeleteConfirmOptions& options = {});

protected:
    void onCreate() override;
    void onResize(float width, float height) override;

private:
    void createComponents();
    void layoutComponents();
    void resizeToFitContent();
    [[nodiscard]] std::wstring formatMessage() const;

    const std::vector<std::filesystem::path>* files_ = nullptr;
    DeleteConfirmOptions options_;
    DeleteConfirmResult result_ = DeleteConfirmResult::Cancel;

    // D2D components (owned by component tree)
    D2DLabel* icon_label_ = nullptr;
    D2DLabel* message_label_ = nullptr;
    D2DListBox* file_list_ = nullptr;
    D2DCheckBox* trash_check_ = nullptr;
    D2DButton* delete_button_ = nullptr;
    D2DButton* cancel_button_ = nullptr;

    // Custom brush for warning icon
    ComPtr<ID2D1SolidColorBrush> warning_brush_;
    ComPtr<IDWriteTextFormat> icon_text_format_;

    // Resource epoch tracking for device-lost recovery
    uint32_t last_resource_epoch_ = 0;
};

/// @brief Show delete confirmation dialog (convenience function)
/// @param parent Parent window handle
/// @param files Files to delete
/// @param options Dialog options
/// @return User's choice
DeleteConfirmResult showD2DDeleteConfirmDialog(HWND parent,
                                               const std::vector<std::filesystem::path>& files,
                                               const DeleteConfirmOptions& options = {});

}  // namespace nive::ui::d2d
