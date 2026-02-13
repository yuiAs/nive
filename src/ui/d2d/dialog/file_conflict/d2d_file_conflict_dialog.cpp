/// @file d2d_file_conflict_dialog.cpp
/// @brief D2D file conflict dialog implementation

#include "d2d_file_conflict_dialog.hpp"

#include <chrono>
#include <format>

#include "core/config/settings.hpp"
#include "core/fs/file_operations.hpp"
#include "core/image/image_scaler.hpp"
#include "core/image/wic_decoder.hpp"
#include "ui/app.hpp"
#include "ui/d2d/core/bitmap_utils.hpp"
#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

namespace {

// Layout constants (in DIPs)
constexpr float kPadding = 15.0f;
constexpr float kSmallPadding = 8.0f;
constexpr float kThumbnailSize = 256.0f;
constexpr float kThumbnailGap = 30.0f;
constexpr float kItemHeight = 22.0f;
constexpr float kRowSpacing = 6.0f;
constexpr float kButtonWidth = 90.0f;
constexpr float kButtonHeight = 28.0f;
constexpr float kButtonSpacing = 10.0f;
constexpr float kSeparatorHeight = 1.0f;
constexpr float kInfoLabelHeight = 48.0f;

// Map config::ConflictResolution to a default radio button index (0-5)
int getDefaultRadioIndex(config::ConflictResolution res) {
    switch (res) {
    case config::ConflictResolution::NewerDate:
        return 0;
    case config::ConflictResolution::Overwrite:
        return 1;
    case config::ConflictResolution::Skip:
        return 2;
    case config::ConflictResolution::Rename:
        return 4;  // Auto number
    case config::ConflictResolution::LargerSize:
        return 5;
    case config::ConflictResolution::Ask:
    default:
        return 2;  // Default to Skip
    }
}

// Load a decoded image for a path (use cache or load from file)
std::unique_ptr<image::DecodedImage> loadThumbnailImage(const std::filesystem::path& path) {
    // Try to get from cache
    auto* cache = App::instance().cache();
    if (cache) {
        auto result = cache->getThumbnail(path);
        if (result) {
            return std::make_unique<image::DecodedImage>(std::move(*result));
        }
    }

    // Not in cache, try loading the actual file and create a thumbnail
    image::WicDecoder decoder;
    auto result = decoder.decode(path);
    if (result) {
        auto thumb = image::generateThumbnail(*result, 384);
        if (thumb) {
            return std::make_unique<image::DecodedImage>(std::move(*thumb));
        }
    }

    return nullptr;
}

}  // namespace

D2DFileConflictDialog::D2DFileConflictDialog() {
    setTitle(L"File Conflict");
    setInitialSize(Size{kPadding * 2 + kThumbnailSize * 2 + kThumbnailGap, 480.0f});
    setResizable(false);
}

std::optional<fs::ConflictResolution>
D2DFileConflictDialog::show(HWND parent, const fs::FileConflictInfo& conflict) {
    conflict_ = &conflict;
    resolution_ = fs::ConflictResolution{};
    apply_to_all_ = false;
    cancelled_ = true;

    showModal(parent);

    if (cancelled_) {
        return std::nullopt;
    }

    return resolution_;
}

void D2DFileConflictDialog::onCreate() {
    createComponents();
    populateFromConflict();
    layoutComponents();
    loadThumbnails();
    resizeToFitContent();
}

void D2DFileConflictDialog::onResize(float width, float height) {
    D2DDialog::onResize(width, height);
    layoutComponents();
}

void D2DFileConflictDialog::resizeToFitContent() {
    // Calculate needed client area from actual content
    float needed_width = kPadding * 2 + kThumbnailSize * 2 + kThumbnailGap;
    float needed_height = ok_button_->bounds().bottom() + kPadding;

    // Derive DIP-to-pixel ratio from current client rect
    RECT client_rc;
    GetClientRect(hwnd(), &client_rc);
    auto current_size = deviceResources().getSize();
    if (current_size.width <= 0.0f || current_size.height <= 0.0f)
        return;

    float ppd_x = static_cast<float>(client_rc.right) / current_size.width;
    float ppd_y = static_cast<float>(client_rc.bottom) / current_size.height;
    int new_client_w = static_cast<int>(needed_width * ppd_x + 0.5f);
    int new_client_h = static_cast<int>(needed_height * ppd_y + 0.5f);

    // Convert client size to window size (accounting for title bar and borders)
    DWORD style = static_cast<DWORD>(GetWindowLongW(hwnd(), GWL_STYLE));
    DWORD ex_style = static_cast<DWORD>(GetWindowLongW(hwnd(), GWL_EXSTYLE));
    RECT rc = {0, 0, new_client_w, new_client_h};
    AdjustWindowRectEx(&rc, style, FALSE, ex_style);

    int win_w = rc.right - rc.left;
    int win_h = rc.bottom - rc.top;

    // Re-center on parent after resize
    RECT win_rc;
    GetWindowRect(hwnd(), &win_rc);
    HWND parent = GetParent(hwnd());
    int x = win_rc.left;
    int y = win_rc.top;
    if (parent) {
        RECT parent_rc;
        GetWindowRect(parent, &parent_rc);
        x = parent_rc.left + (parent_rc.right - parent_rc.left - win_w) / 2;
        y = parent_rc.top + (parent_rc.bottom - parent_rc.top - win_h) / 2;
    }

    SetWindowPos(hwnd(), nullptr, x, y, win_w, win_h, SWP_NOZORDER | SWP_NOACTIVATE);
}

void D2DFileConflictDialog::createComponents() {
    auto& res = deviceResources();

    // Create custom brushes for rendering
    separator_brush_ = res.createSolidBrush(Color::fromRgb(0xD0D0D0));
    placeholder_bg_brush_ = res.createSolidBrush(Color::fromRgb(0xF0F0F0));
    placeholder_text_brush_ = res.createSolidBrush(Color::fromRgb(0x999999));
    arrow_brush_ = res.createSolidBrush(Color::fromRgb(0x666666));

    // Arrow text format
    arrow_text_format_ = res.createTextFormat(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_BOLD);
    if (arrow_text_format_) {
        arrow_text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        arrow_text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Header label
    auto header = std::make_unique<D2DLabel>(L"A file with the same name already exists.");
    header->createResources(res);
    header_label_ = header.get();
    addChild(std::move(header));

    // Source info label
    auto source_info = std::make_unique<D2DLabel>();
    source_info->setWordWrap(true);
    source_info->setFontSize(11.0f);
    source_info->createResources(res);
    source_info_label_ = source_info.get();
    addChild(std::move(source_info));

    // Destination info label
    auto dest_info = std::make_unique<D2DLabel>();
    dest_info->setWordWrap(true);
    dest_info->setFontSize(11.0f);
    dest_info->createResources(res);
    dest_info_label_ = dest_info.get();
    addChild(std::move(dest_info));

    // Identical label (may be hidden)
    auto identical = std::make_unique<D2DLabel>(L"These files appear to be identical.");
    identical->setTextAlignment(TextAlignment::Center);
    identical->setTextColor(Color::fromRgb(0x888888));
    identical->createResources(res);
    identical_label_ = identical.get();
    addChild(std::move(identical));

    // Radio buttons - Left column
    auto newer = std::make_unique<D2DRadioButton>(L"Keep newer date");
    newer->setGroup(&action_radio_group_);
    newer->createResources(res);
    newer->onChange([this](bool) { custom_name_edit_->setEnabled(false); });
    action_newer_ = newer.get();
    addChild(std::move(newer));

    auto overwrite = std::make_unique<D2DRadioButton>(L"Overwrite");
    overwrite->setGroup(&action_radio_group_);
    overwrite->createResources(res);
    overwrite->onChange([this](bool) { custom_name_edit_->setEnabled(false); });
    action_overwrite_ = overwrite.get();
    addChild(std::move(overwrite));

    auto skip = std::make_unique<D2DRadioButton>(L"Skip");
    skip->setGroup(&action_radio_group_);
    skip->createResources(res);
    skip->onChange([this](bool) { custom_name_edit_->setEnabled(false); });
    action_skip_ = skip.get();
    addChild(std::move(skip));

    // Radio buttons - Right column
    auto rename = std::make_unique<D2DRadioButton>(L"Rename:");
    rename->setGroup(&action_radio_group_);
    rename->createResources(res);
    rename->onChange([this](bool selected) {
        custom_name_edit_->setEnabled(selected);
        if (selected) {
            custom_name_edit_->selectAll();
        }
    });
    action_rename_ = rename.get();
    addChild(std::move(rename));

    auto autonumber = std::make_unique<D2DRadioButton>(L"Auto number");
    autonumber->setGroup(&action_radio_group_);
    autonumber->createResources(res);
    autonumber->onChange([this](bool) { custom_name_edit_->setEnabled(false); });
    action_autonumber_ = autonumber.get();
    addChild(std::move(autonumber));

    auto larger = std::make_unique<D2DRadioButton>(L"Keep larger size");
    larger->setGroup(&action_radio_group_);
    larger->createResources(res);
    larger->onChange([this](bool) { custom_name_edit_->setEnabled(false); });
    action_larger_ = larger.get();
    addChild(std::move(larger));

    // Custom name edit box
    auto edit = std::make_unique<D2DEditBox>();
    edit->setEnabled(false);
    edit->createResources(res);
    custom_name_edit_ = edit.get();
    addChild(std::move(edit));

    // Checkboxes
    auto trash = std::make_unique<D2DCheckBox>(L"Move replaced file to Trash");
    trash->createResources(res);
    move_to_trash_check_ = trash.get();
    addChild(std::move(trash));

    auto skip_id = std::make_unique<D2DCheckBox>(L"Skip identical files");
    skip_id->createResources(res);
    skip_identical_check_ = skip_id.get();
    addChild(std::move(skip_id));

    // Buttons
    auto ok = std::make_unique<D2DButton>(L"OK");
    ok->setVariant(ButtonVariant::Primary);
    ok->createResources(res);
    ok->onClick([this]() {
        if (validateAndSave()) {
            saveOptionsToSettings();
            cancelled_ = false;
            endDialog(IDOK);
        }
    });
    ok_button_ = ok.get();
    addChild(std::move(ok));

    auto apply_all = std::make_unique<D2DButton>(L"Apply to All");
    apply_all->createResources(res);
    apply_all->onClick([this]() {
        if (validateAndSave()) {
            saveOptionsToSettings();
            apply_to_all_ = true;
            resolution_.apply_to_all = true;
            cancelled_ = false;
            endDialog(IDOK);
        }
    });
    apply_all_button_ = apply_all.get();
    addChild(std::move(apply_all));

    auto cancel = std::make_unique<D2DButton>(L"Cancel");
    cancel->createResources(res);
    cancel->onClick([this]() {
        cancelled_ = true;
        endDialog(IDCANCEL);
    });
    cancel_button_ = cancel.get();
    addChild(std::move(cancel));
}

void D2DFileConflictDialog::layoutComponents() {
    auto size = deviceResources().getSize();
    float content_width = size.width - kPadding * 2;
    float y = kPadding;

    // Header label
    header_label_->arrange(Rect{kPadding, y, content_width, kItemHeight});
    y += kItemHeight + kSmallPadding;

    // Thumbnail areas (reserved for custom rendering in onRender)
    float thumb_x1 = kPadding;
    float thumb_x2 = kPadding + kThumbnailSize + kThumbnailGap;
    source_thumb_area_ = Rect{thumb_x1, y, kThumbnailSize, kThumbnailSize};
    dest_thumb_area_ = Rect{thumb_x2, y, kThumbnailSize, kThumbnailSize};
    y += kThumbnailSize + kRowSpacing;

    // Source/dest info labels
    source_info_label_->arrange(Rect{thumb_x1, y, kThumbnailSize, kInfoLabelHeight});
    dest_info_label_->arrange(Rect{thumb_x2, y, kThumbnailSize, kInfoLabelHeight});
    y += kInfoLabelHeight + kRowSpacing;

    // Identical label (visible only when files are identical)
    if (conflict_ && conflict_->files_identical) {
        identical_label_->arrange(Rect{kPadding, y, content_width, kItemHeight});
        identical_label_->setVisible(true);
        y += kItemHeight + kRowSpacing;
    } else {
        identical_label_->setVisible(false);
    }

    // Separator 1
    y += kSeparatorHeight + kSmallPadding;

    // Radio buttons - two columns
    float col1_x = kPadding;
    float col2_x = kPadding + content_width / 2.0f + kSmallPadding;
    float radio_width = content_width / 2.0f - kSmallPadding;
    float left_y = y;
    float right_y = y;

    // Left column: newer, overwrite, skip
    action_newer_->arrange(Rect{col1_x, left_y, radio_width, kItemHeight});
    left_y += kItemHeight + kRowSpacing;

    action_overwrite_->arrange(Rect{col1_x, left_y, radio_width, kItemHeight});
    left_y += kItemHeight + kRowSpacing;

    action_skip_->arrange(Rect{col1_x, left_y, radio_width, kItemHeight});
    left_y += kItemHeight;

    // Right column: rename + edit, autonumber, larger
    float rename_radio_width = 80.0f;
    float edit_x = col2_x + rename_radio_width + 4.0f;
    float edit_width = radio_width - rename_radio_width - 4.0f;

    action_rename_->arrange(Rect{col2_x, right_y, rename_radio_width, kItemHeight});
    custom_name_edit_->arrange(Rect{edit_x, right_y, edit_width, kItemHeight});
    right_y += kItemHeight + kRowSpacing;

    action_autonumber_->arrange(Rect{col2_x, right_y, radio_width, kItemHeight});
    right_y += kItemHeight + kRowSpacing;

    action_larger_->arrange(Rect{col2_x, right_y, radio_width, kItemHeight});
    right_y += kItemHeight;

    y = std::max(left_y, right_y) + kSmallPadding;

    // Separator 2
    y += kSeparatorHeight + kSmallPadding;

    // Checkboxes - two columns
    move_to_trash_check_->arrange(Rect{col1_x, y, radio_width, kItemHeight});
    skip_identical_check_->arrange(Rect{col2_x, y, radio_width, kItemHeight});
    y += kItemHeight + kPadding;

    // Buttons at bottom
    float btn_x = kPadding;
    ok_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
    btn_x += kButtonWidth + kButtonSpacing;
    apply_all_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
    btn_x += kButtonWidth + kButtonSpacing;
    cancel_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
}

void D2DFileConflictDialog::populateFromConflict() {
    if (!conflict_)
        return;

    // Set file info labels
    auto source_text =
        std::format(L"{}\n{}\n{}", conflict_->source_path.filename().wstring(),
                    formatFileTime(conflict_->source_time), formatFileSize(conflict_->source_size));
    source_info_label_->setText(source_text);

    auto dest_text =
        std::format(L"{}\n{}\n{}", conflict_->dest_path.filename().wstring(),
                    formatFileTime(conflict_->dest_time), formatFileSize(conflict_->dest_size));
    dest_info_label_->setText(dest_text);

    // Set custom name edit to source filename by default
    custom_name_edit_->setText(conflict_->source_path.filename().wstring());

    // Load default options from settings
    auto& settings = App::instance().settings();
    int default_index = getDefaultRadioIndex(settings.conflict_resolution);

    D2DRadioButton* radios[] = {action_newer_,   action_overwrite_, action_skip_,
                                action_rename_,  action_autonumber_, action_larger_};
    if (default_index >= 0 && default_index < 6) {
        radios[default_index]->setSelected(true);
    }

    // Set checkbox defaults
    if (settings.use_recycle_bin) {
        move_to_trash_check_->setChecked(true);
    }

    // If files are identical, enable skip identical by default
    if (conflict_->files_identical) {
        skip_identical_check_->setChecked(true);
    }
}

void D2DFileConflictDialog::loadThumbnails() {
    source_image_ = loadThumbnailImage(conflict_->source_path);
    dest_image_ = loadThumbnailImage(conflict_->dest_path);

    auto* rt = deviceResources().renderTarget();
    if (!rt)
        return;

    if (source_image_) {
        source_bitmap_ = createBitmapFromDecodedImage(rt, *source_image_);
    }
    if (dest_image_) {
        dest_bitmap_ = createBitmapFromDecodedImage(rt, *dest_image_);
    }
}

void D2DFileConflictDialog::onRender(ID2D1RenderTarget* rt) {
    rt->Clear(D2D1::ColorF(0xF5F5F5));

    // Device-lost recovery
    uint32_t epoch = deviceResources().resourceEpoch();
    if (epoch != last_resource_epoch_) {
        auto& res = deviceResources();

        // Recreate custom brushes
        separator_brush_ = res.createSolidBrush(Color::fromRgb(0xD0D0D0));
        placeholder_bg_brush_ = res.createSolidBrush(Color::fromRgb(0xF0F0F0));
        placeholder_text_brush_ = res.createSolidBrush(Color::fromRgb(0x999999));
        arrow_brush_ = res.createSolidBrush(Color::fromRgb(0x666666));

        arrow_text_format_ = res.createTextFormat(L"Segoe UI", 18.0f, DWRITE_FONT_WEIGHT_BOLD);
        if (arrow_text_format_) {
            arrow_text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            arrow_text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        // Recreate D2D bitmaps from source images
        if (source_image_) {
            source_bitmap_ = createBitmapFromDecodedImage(rt, *source_image_);
        }
        if (dest_image_) {
            dest_bitmap_ = createBitmapFromDecodedImage(rt, *dest_image_);
        }

        // Recreate all component resources
        if (header_label_) header_label_->createResources(res);
        if (source_info_label_) source_info_label_->createResources(res);
        if (dest_info_label_) dest_info_label_->createResources(res);
        if (identical_label_) identical_label_->createResources(res);
        if (action_newer_) action_newer_->createResources(res);
        if (action_overwrite_) action_overwrite_->createResources(res);
        if (action_skip_) action_skip_->createResources(res);
        if (action_rename_) action_rename_->createResources(res);
        if (action_autonumber_) action_autonumber_->createResources(res);
        if (action_larger_) action_larger_->createResources(res);
        if (custom_name_edit_) custom_name_edit_->createResources(res);
        if (move_to_trash_check_) move_to_trash_check_->createResources(res);
        if (skip_identical_check_) skip_identical_check_->createResources(res);
        if (ok_button_) ok_button_->createResources(res);
        if (apply_all_button_) apply_all_button_->createResources(res);
        if (cancel_button_) cancel_button_->createResources(res);

        last_resource_epoch_ = epoch;
    }

    // Render all child components
    D2DDialog::onRender(rt);

    // Render thumbnails on top of reserved areas
    renderThumbnail(rt, source_thumb_area_, source_bitmap_.Get());
    renderThumbnail(rt, dest_thumb_area_, dest_bitmap_.Get());

    // Draw border around thumbnail areas
    rt->DrawRectangle(source_thumb_area_.toD2D(), separator_brush_.Get(), 1.0f);
    rt->DrawRectangle(dest_thumb_area_.toD2D(), separator_brush_.Get(), 1.0f);

    // Render arrow between thumbnails
    renderArrow(rt, source_thumb_area_, dest_thumb_area_);

    // Render separator lines
    float content_width = deviceResources().getSize().width - kPadding * 2;

    // Separator 1: after info labels / identical label
    float sep1_y = source_thumb_area_.bottom() + kRowSpacing + kInfoLabelHeight + kRowSpacing;
    if (conflict_ && conflict_->files_identical) {
        sep1_y += kItemHeight + kRowSpacing;
    }
    renderSeparator(rt, kPadding, sep1_y, content_width);

    // Separator 2: after radio buttons
    float radio_bottom = std::max(action_skip_->bounds().bottom(), action_larger_->bounds().bottom());
    float sep2_y = radio_bottom + kSmallPadding;
    renderSeparator(rt, kPadding, sep2_y, content_width);
}

void D2DFileConflictDialog::renderThumbnail(ID2D1RenderTarget* rt, const Rect& area,
                                             ID2D1Bitmap* bitmap) const {
    if (bitmap) {
        auto bmp_size = bitmap->GetSize();

        // Calculate scaled dimensions maintaining aspect ratio
        float scale_x = area.width / bmp_size.width;
        float scale_y = area.height / bmp_size.height;
        float scale = std::min(scale_x, scale_y);

        float scaled_w = bmp_size.width * scale;
        float scaled_h = bmp_size.height * scale;

        // Center in area
        float x = area.x + (area.width - scaled_w) / 2.0f;
        float y = area.y + (area.height - scaled_h) / 2.0f;

        D2D1_RECT_F dest_rect = D2D1::RectF(x, y, x + scaled_w, y + scaled_h);
        rt->DrawBitmap(bitmap, dest_rect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    } else {
        // Draw placeholder
        rt->FillRectangle(area.toD2D(), placeholder_bg_brush_.Get());

        if (placeholder_text_brush_ && arrow_text_format_) {
            rt->DrawText(L"No preview", 10, arrow_text_format_.Get(), area.toD2D(),
                         placeholder_text_brush_.Get());
        }
    }
}

void D2DFileConflictDialog::renderArrow(ID2D1RenderTarget* rt, const Rect& source_area,
                                         const Rect& dest_area) const {
    if (!arrow_brush_ || !arrow_text_format_)
        return;

    // Arrow is drawn in the gap between the two thumbnail areas
    float arrow_x = source_area.right();
    float arrow_width = dest_area.x - source_area.right();
    float arrow_y = source_area.y + source_area.height / 2.0f - 12.0f;
    float arrow_height = 24.0f;

    D2D1_RECT_F arrow_rect =
        D2D1::RectF(arrow_x, arrow_y, arrow_x + arrow_width, arrow_y + arrow_height);

    rt->DrawText(L"\u25B6", 1, arrow_text_format_.Get(), arrow_rect, arrow_brush_.Get());
}

void D2DFileConflictDialog::renderSeparator(ID2D1RenderTarget* rt, float x, float y,
                                             float width) const {
    if (!separator_brush_)
        return;

    rt->DrawLine(D2D1::Point2F(x, y), D2D1::Point2F(x + width, y), separator_brush_.Get(), 1.0f);
}

bool D2DFileConflictDialog::validateAndSave() {
    // Determine selected action
    if (action_newer_->isSelected()) {
        resolution_.action = fs::ConflictAction::KeepNewer;
    } else if (action_overwrite_->isSelected()) {
        resolution_.action = fs::ConflictAction::Overwrite;
    } else if (action_skip_->isSelected()) {
        resolution_.action = fs::ConflictAction::Skip;
    } else if (action_rename_->isSelected()) {
        resolution_.action = fs::ConflictAction::Rename;
        resolution_.custom_name = custom_name_edit_->text();

        if (resolution_.custom_name.empty()) {
            MessageBoxW(hwnd(), L"Please enter a file name.", L"Validation Error",
                        MB_ICONWARNING | MB_OK);
            return false;
        }

        if (!fs::isValidFilename(resolution_.custom_name)) {
            MessageBoxW(hwnd(), L"The file name contains invalid characters.", L"Validation Error",
                        MB_ICONWARNING | MB_OK);
            return false;
        }
    } else if (action_autonumber_->isSelected()) {
        resolution_.action = fs::ConflictAction::AutoNumber;
    } else if (action_larger_->isSelected()) {
        resolution_.action = fs::ConflictAction::KeepLarger;
    }

    // Read checkbox states
    resolution_.move_replaced_to_trash = move_to_trash_check_->isChecked();
    resolution_.skip_identical = skip_identical_check_->isChecked();

    return true;
}

void D2DFileConflictDialog::saveOptionsToSettings() {
    auto& settings = App::instance().settings();

    // Map selected radio to config resolution
    if (action_newer_->isSelected()) {
        settings.conflict_resolution = config::ConflictResolution::NewerDate;
    } else if (action_overwrite_->isSelected()) {
        settings.conflict_resolution = config::ConflictResolution::Overwrite;
    } else if (action_skip_->isSelected()) {
        settings.conflict_resolution = config::ConflictResolution::Skip;
    } else if (action_rename_->isSelected() || action_autonumber_->isSelected()) {
        settings.conflict_resolution = config::ConflictResolution::Rename;
    } else if (action_larger_->isSelected()) {
        settings.conflict_resolution = config::ConflictResolution::LargerSize;
    }

    settings.use_recycle_bin = move_to_trash_check_->isChecked();
}

std::wstring D2DFileConflictDialog::formatFileSize(uint64_t size) {
    if (size < 1024) {
        return std::format(L"{} B", size);
    } else if (size < 1024 * 1024) {
        return std::format(L"{:.1f} KB", size / 1024.0);
    } else if (size < 1024 * 1024 * 1024) {
        return std::format(L"{:.1f} MB", size / (1024.0 * 1024.0));
    } else {
        return std::format(L"{:.1f} GB", size / (1024.0 * 1024.0 * 1024.0));
    }
}

std::wstring D2DFileConflictDialog::formatFileTime(const std::filesystem::file_time_type& time) {
    auto sctp = std::chrono::clock_cast<std::chrono::system_clock>(time);
    auto tt = std::chrono::system_clock::to_time_t(sctp);

    std::tm tm;
    localtime_s(&tm, &tt);

    return std::format(L"{:04d}/{:02d}/{:02d} {:02d}:{:02d}:{:02d}", tm.tm_year + 1900,
                       tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
}

std::optional<fs::ConflictResolution>
showD2DFileConflictDialog(HWND parent, const fs::FileConflictInfo& conflict) {
    D2DFileConflictDialog dialog;
    return dialog.show(parent, conflict);
}

}  // namespace nive::ui::d2d
