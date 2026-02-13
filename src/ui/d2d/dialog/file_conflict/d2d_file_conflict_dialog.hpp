/// @file d2d_file_conflict_dialog.hpp
/// @brief D2D-rendered dialog for resolving file conflicts during copy/move operations

#pragma once

#include <d2d1.h>

#include <optional>

#include "core/fs/file_conflict.hpp"
#include "core/image/decoded_image.hpp"
#include "core/util/com_ptr.hpp"
#include "ui/d2d/components/button.hpp"
#include "ui/d2d/components/checkbox.hpp"
#include "ui/d2d/components/editbox.hpp"
#include "ui/d2d/components/label.hpp"
#include "ui/d2d/components/radiobutton.hpp"
#include "ui/d2d/dialog/d2d_dialog.hpp"

namespace nive::ui::d2d {

/// @brief D2D-rendered dialog for resolving file conflicts
///
/// Shows source and destination file information with thumbnails,
/// and allows the user to choose how to resolve the conflict.
/// Extends D2DDialog using the D2D component framework.
class D2DFileConflictDialog : public D2DDialog {
public:
    D2DFileConflictDialog();
    ~D2DFileConflictDialog() override = default;

    /// @brief Show the conflict resolution dialog
    /// @param parent Parent window handle
    /// @param conflict Information about the conflict
    /// @return The resolution chosen by the user, or nullopt if cancelled
    std::optional<fs::ConflictResolution> show(HWND parent, const fs::FileConflictInfo& conflict);

    /// @brief Get whether user chose "Apply to all"
    [[nodiscard]] bool isApplyToAll() const noexcept { return apply_to_all_; }

protected:
    void onCreate() override;
    void onRender(ID2D1RenderTarget* rt) override;
    void onResize(float width, float height) override;

private:
    void createComponents();
    void layoutComponents();
    void resizeToFitContent();
    void populateFromConflict();
    void loadThumbnails();

    void renderThumbnail(ID2D1RenderTarget* rt, const Rect& area,
                         ID2D1Bitmap* bitmap) const;
    void renderArrow(ID2D1RenderTarget* rt, const Rect& source_area,
                     const Rect& dest_area) const;
    void renderSeparator(ID2D1RenderTarget* rt, float x, float y, float width) const;

    bool validateAndSave();
    void saveOptionsToSettings();

    [[nodiscard]] static std::wstring formatFileSize(uint64_t size);
    [[nodiscard]] static std::wstring formatFileTime(
        const std::filesystem::file_time_type& time);

    // Conflict data (valid during show())
    const fs::FileConflictInfo* conflict_ = nullptr;
    fs::ConflictResolution resolution_;
    bool apply_to_all_ = false;
    bool cancelled_ = true;

    // Thumbnail bitmaps for D2D rendering
    ComPtr<ID2D1Bitmap> source_bitmap_;
    ComPtr<ID2D1Bitmap> dest_bitmap_;

    // Source images for device-lost re-creation
    std::unique_ptr<image::DecodedImage> source_image_;
    std::unique_ptr<image::DecodedImage> dest_image_;

    // Thumbnail reserved area rects (set during layout, used during render)
    Rect source_thumb_area_;
    Rect dest_thumb_area_;

    // D2D component pointers (owned by component tree via addChild)
    D2DLabel* header_label_ = nullptr;
    D2DLabel* source_info_label_ = nullptr;
    D2DLabel* dest_info_label_ = nullptr;
    D2DLabel* identical_label_ = nullptr;

    // Action radio buttons
    D2DRadioButton* action_newer_ = nullptr;
    D2DRadioButton* action_overwrite_ = nullptr;
    D2DRadioButton* action_skip_ = nullptr;
    D2DRadioButton* action_rename_ = nullptr;
    D2DRadioButton* action_autonumber_ = nullptr;
    D2DRadioButton* action_larger_ = nullptr;
    D2DRadioGroup action_radio_group_;

    // Rename edit box
    D2DEditBox* custom_name_edit_ = nullptr;

    // Option checkboxes
    D2DCheckBox* move_to_trash_check_ = nullptr;
    D2DCheckBox* skip_identical_check_ = nullptr;

    // Buttons
    D2DButton* ok_button_ = nullptr;
    D2DButton* apply_all_button_ = nullptr;
    D2DButton* cancel_button_ = nullptr;

    // Brushes for custom rendering
    ComPtr<ID2D1SolidColorBrush> separator_brush_;
    ComPtr<ID2D1SolidColorBrush> placeholder_bg_brush_;
    ComPtr<ID2D1SolidColorBrush> placeholder_text_brush_;
    ComPtr<ID2D1SolidColorBrush> arrow_brush_;

    // DWrite text format for arrow glyph
    ComPtr<IDWriteTextFormat> arrow_text_format_;

    // Resource epoch tracking for device-lost recovery
    uint32_t last_resource_epoch_ = 0;
};

/// @brief Show file conflict dialog (convenience function)
/// @param parent Parent window handle
/// @param conflict Conflict information
/// @return Resolution or nullopt if cancelled
std::optional<fs::ConflictResolution> showD2DFileConflictDialog(
    HWND parent, const fs::FileConflictInfo& conflict);

}  // namespace nive::ui::d2d
