/// @file d2d_settings_dialog.hpp
/// @brief Settings dialog using D2D UI framework

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/config/settings.hpp"
#include "ui/d2d/components/button.hpp"
#include "ui/d2d/components/checkbox.hpp"
#include "ui/d2d/components/combobox.hpp"
#include "ui/d2d/components/editbox.hpp"
#include "ui/d2d/components/groupbox.hpp"
#include "ui/d2d/components/label.hpp"
#include "ui/d2d/components/listbox.hpp"
#include "ui/d2d/components/panel.hpp"
#include "ui/d2d/components/radiobutton.hpp"
#include "ui/d2d/components/spinedit.hpp"
#include "ui/d2d/components/tabcontrol.hpp"
#include "ui/d2d/dialog/d2d_dialog.hpp"

namespace nive::ui::d2d {

/// @brief Dialog result enum
enum class SettingsDialogResult {
    Ok,
    Cancel,
};

/// @brief Settings dialog using D2D UI framework
///
/// Replaces the Win32-based SettingsDialog with a D2D-rendered version.
class D2DSettingsDialog : public D2DDialog {
public:
    D2DSettingsDialog();
    ~D2DSettingsDialog() override = default;

    /// @brief Show the settings dialog
    /// @param parent Parent window handle
    /// @param settings Current settings (will be modified if user clicks OK)
    /// @return Dialog result
    SettingsDialogResult show(HWND parent, config::Settings& settings);

protected:
    void onCreate() override;
    void onRender(ID2D1RenderTarget* rt) override;
    void onResize(float width, float height) override;
    bool onClose() override;

    // Override mouse handling for popup support
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;

private:
    void createComponents();
    void layoutComponents();

    // Tab creation
    std::unique_ptr<D2DContainerComponent> createGeneralTab();
    std::unique_ptr<D2DContainerComponent> createThumbnailsTab();
    std::unique_ptr<D2DContainerComponent> createCacheTab();
    std::unique_ptr<D2DContainerComponent> createSortingTab();
    std::unique_ptr<D2DContainerComponent> createNetworkTab();

    // Tab layout
    void layoutGeneralTab();
    void layoutThumbnailsTab();
    void layoutCacheTab();
    void layoutSortingTab();
    void layoutNetworkTab();

    // Settings management
    void populateFromSettings();
    bool validateAndSave();

    // Resource management
    void recreateAllComponentResources();
    void updateActiveTabResources();

    // Folder browser helpers
    void browseStartupPath();
    void browseCustomCachePath();
    void addNetworkShare();
    void removeNetworkShare();

    // Settings reference (valid during show())
    config::Settings* settings_ = nullptr;
    SettingsDialogResult result_ = SettingsDialogResult::Cancel;

    // Main components
    D2DTabControl* tab_control_ = nullptr;
    D2DButton* ok_button_ = nullptr;
    D2DButton* cancel_button_ = nullptr;

    // General tab components
    D2DGroupBox* startup_group_ = nullptr;
    D2DRadioButton* startup_home_ = nullptr;
    D2DRadioButton* startup_last_ = nullptr;
    D2DRadioButton* startup_custom_ = nullptr;
    D2DEditBox* startup_path_ = nullptr;
    D2DButton* startup_browse_ = nullptr;
    D2DRadioGroup startup_radio_group_;
    D2DLabel* language_label_ = nullptr;
    D2DComboBox* language_combo_ = nullptr;
    std::vector<std::string> available_locales_;

    // Thumbnails tab components
    D2DLabel* stored_size_label_ = nullptr;
    D2DSpinEdit* stored_size_spin_ = nullptr;
    D2DLabel* display_size_label_ = nullptr;
    D2DSpinEdit* display_size_spin_ = nullptr;
    D2DLabel* buffer_count_label_ = nullptr;
    D2DSpinEdit* buffer_count_spin_ = nullptr;
    D2DLabel* worker_count_label_ = nullptr;
    D2DSpinEdit* worker_count_spin_ = nullptr;

    // Cache tab components
    D2DGroupBox* cache_location_group_ = nullptr;
    D2DRadioButton* cache_appdata_ = nullptr;
    D2DRadioButton* cache_portable_ = nullptr;
    D2DRadioButton* cache_custom_ = nullptr;
    D2DEditBox* cache_path_ = nullptr;
    D2DButton* cache_browse_ = nullptr;
    D2DRadioGroup cache_radio_group_;
    D2DLabel* compression_label_ = nullptr;
    D2DSpinEdit* compression_spin_ = nullptr;
    D2DCheckBox* retention_checkbox_ = nullptr;
    D2DLabel* retention_label_ = nullptr;
    D2DSpinEdit* retention_spin_ = nullptr;

    // Sorting tab components
    D2DLabel* sort_method_label_ = nullptr;
    D2DComboBox* sort_method_combo_ = nullptr;
    D2DLabel* sort_order_label_ = nullptr;
    D2DComboBox* sort_order_combo_ = nullptr;

    // Network tab components
    D2DLabel* network_label_ = nullptr;
    D2DListBox* network_list_ = nullptr;
    D2DButton* network_add_ = nullptr;
    D2DButton* network_remove_ = nullptr;

    // Active popup (for dropdown rendering)
    D2DComboBox* active_popup_ = nullptr;

    // Resource epoch tracking for device-lost recovery
    uint32_t last_resource_epoch_ = 0;
};

/// @brief Show settings dialog (convenience function)
/// @param parent Parent window handle
/// @param settings Settings to modify
/// @return true if OK was clicked
bool showD2DSettingsDialog(HWND parent, config::Settings& settings);

}  // namespace nive::ui::d2d
