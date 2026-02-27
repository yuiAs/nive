/// @file d2d_rename_dialog.hpp
/// @brief D2D-rendered dialog for renaming files

#pragma once

#include <optional>
#include <string>

#include "ui/d2d/components/button.hpp"
#include "ui/d2d/components/editbox.hpp"
#include "ui/d2d/components/label.hpp"
#include "ui/d2d/dialog/d2d_dialog.hpp"

namespace nive::ui::d2d {

/// @brief D2D-rendered dialog for renaming files
///
/// Shows a label and edit box with the current filename.
/// The stem (filename without extension) is pre-selected.
class D2DRenameDialog : public D2DDialog {
public:
    D2DRenameDialog();
    ~D2DRenameDialog() override = default;

    /// @brief Show the rename dialog
    /// @param parent Parent window handle
    /// @param current_name Current filename
    /// @return New filename, or nullopt if cancelled
    std::optional<std::wstring> show(HWND parent, const std::wstring& current_name);

protected:
    void onCreate() override;
    void onResize(float width, float height) override;

private:
    void createComponents();
    void layoutComponents();
    void resizeToFitContent();

    std::wstring current_name_;
    std::optional<std::wstring> result_;

    // D2D components (owned by component tree)
    D2DLabel* label_ = nullptr;
    D2DEditBox* edit_ = nullptr;
    D2DButton* ok_button_ = nullptr;
    D2DButton* cancel_button_ = nullptr;
};

}  // namespace nive::ui::d2d
