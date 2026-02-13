/// @file d2d_test_dialog.hpp
/// @brief Debug-only test dialog for D2D UI components

#pragma once

#ifdef NIVE_DEBUG_D2D_TEST

    #include <memory>

    #include "ui/d2d/components/button.hpp"
    #include "ui/d2d/components/checkbox.hpp"
    #include "ui/d2d/components/editbox.hpp"
    #include "ui/d2d/components/groupbox.hpp"
    #include "ui/d2d/components/label.hpp"
    #include "ui/d2d/components/panel.hpp"
    #include "ui/d2d/components/radiobutton.hpp"
    #include "ui/d2d/components/spinedit.hpp"
    #include "ui/d2d/components/tabcontrol.hpp"
    #include "ui/d2d/dialog/d2d_dialog.hpp"

namespace nive::ui::d2d {

/// @brief Test dialog for D2D UI component development
///
/// Only available in Debug builds. Provides a sandbox to test
/// D2D-rendered components interactively.
class D2DTestDialog : public D2DDialog {
public:
    D2DTestDialog();
    ~D2DTestDialog() override = default;

protected:
    void onCreate() override;
    void onRender(ID2D1RenderTarget* rt) override;
    void onResize(float width, float height) override;

private:
    void createComponents();
    void layoutComponents();

    // Create tab content
    std::unique_ptr<D2DContainerComponent> createBasicTab();
    std::unique_ptr<D2DContainerComponent> createFormTab();
    std::unique_ptr<D2DContainerComponent> createInputTab();

    void updateCounterLabel();
    void updateStatusLabel();

    void layoutBasicTabContent();
    void layoutFormTabContent();
    void layoutInputTabContent();

    // Main components
    D2DTabControl* tab_control_ = nullptr;
    D2DButton* close_button_ = nullptr;
    D2DLabel* status_label_ = nullptr;

    // Basic tab components
    D2DLabel* title_label_ = nullptr;
    D2DLabel* info_label_ = nullptr;
    D2DLabel* counter_label_ = nullptr;
    D2DButton* increment_button_ = nullptr;
    D2DButton* decrement_button_ = nullptr;
    D2DButton* reset_button_ = nullptr;

    // Form tab components
    D2DCheckBox* checkbox1_ = nullptr;
    D2DCheckBox* checkbox2_ = nullptr;
    D2DCheckBox* checkbox3_ = nullptr;
    D2DRadioButton* radio1_ = nullptr;
    D2DRadioButton* radio2_ = nullptr;
    D2DRadioButton* radio3_ = nullptr;
    D2DGroupBox* checkbox_group_ = nullptr;
    D2DGroupBox* radio_group_ = nullptr;
    D2DRadioGroup radio_group_manager_;

    // Input tab components
    D2DLabel* edit_label_ = nullptr;
    D2DEditBox* edit_box_ = nullptr;
    D2DLabel* spin_label_ = nullptr;
    D2DSpinEdit* spin_edit_ = nullptr;
    D2DLabel* result_label_ = nullptr;

    int counter_ = 0;

    ComPtr<ID2D1SolidColorBrush> border_brush_;
};

/// @brief Show the D2D test dialog
/// @param parent Parent window handle
/// @return Dialog result
INT_PTR showD2DTestDialog(HWND parent);

}  // namespace nive::ui::d2d

#endif  // NIVE_DEBUG_D2D_TEST
