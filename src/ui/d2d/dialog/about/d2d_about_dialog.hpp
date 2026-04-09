/// @file d2d_about_dialog.hpp
/// @brief D2D-rendered About dialog

#pragma once

#include <string>

#include "ui/d2d/components/button.hpp"
#include "ui/d2d/components/label.hpp"
#include "ui/d2d/dialog/d2d_dialog.hpp"

namespace nive::ui::d2d {

/// @brief D2D-rendered About dialog
///
/// Displays application name, version with git hash, and repository URL.
class D2DAboutDialog : public D2DDialog {
public:
    D2DAboutDialog();
    ~D2DAboutDialog() override = default;

    /// @brief Show the About dialog
    /// @param parent Parent window handle
    void show(HWND parent);

protected:
    void onCreate() override;
    void onResize(float width, float height) override;

private:
    void createComponents();
    void layoutComponents();

    // D2D components (owned by component tree)
    D2DLabel* app_name_label_ = nullptr;
    D2DLabel* version_label_ = nullptr;
    D2DUIComponent* url_label_ = nullptr;
    D2DButton* ok_button_ = nullptr;
};

/// @brief Convenience function to show the About dialog
void showD2DAboutDialog(HWND parent);

}  // namespace nive::ui::d2d
