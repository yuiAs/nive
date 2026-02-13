/// @file d2d_delete_confirm_dialog.cpp
/// @brief D2D delete confirmation dialog implementation

#include "d2d_delete_confirm_dialog.hpp"

#include <format>

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

namespace {

// Layout constants (in DIPs)
constexpr float kPadding = 15.0f;
constexpr float kSmallPadding = 8.0f;
constexpr float kIconSize = 32.0f;
constexpr float kItemHeight = 22.0f;
constexpr float kListHeight = 110.0f;
constexpr float kRowSpacing = 6.0f;
constexpr float kButtonWidth = 90.0f;
constexpr float kButtonHeight = 28.0f;
constexpr float kButtonSpacing = 10.0f;
constexpr float kContentWidth = 320.0f;

}  // namespace

D2DDeleteConfirmDialog::D2DDeleteConfirmDialog() {
    setTitle(L"Confirm Delete");
    setInitialSize(Size{kContentWidth + kPadding * 2, 200.0f});
    setResizable(false);
}

DeleteConfirmResult
D2DDeleteConfirmDialog::show(HWND parent, const std::vector<std::filesystem::path>& files,
                             const DeleteConfirmOptions& options) {
    files_ = &files;
    options_ = options;
    result_ = DeleteConfirmResult::Cancel;

    showModal(parent);

    return result_;
}

void D2DDeleteConfirmDialog::onCreate() {
    createComponents();
    layoutComponents();
    resizeToFitContent();
}

void D2DDeleteConfirmDialog::onResize(float width, float height) {
    D2DDialog::onResize(width, height);
    layoutComponents();
}

void D2DDeleteConfirmDialog::createComponents() {
    auto& res = deviceResources();

    // Warning icon brush (amber)
    warning_brush_ = res.createSolidBrush(Color::fromRgb(0xE6A817));

    // Icon text format (large)
    icon_text_format_ = res.createTextFormat(L"Segoe UI", 22.0f, DWRITE_FONT_WEIGHT_NORMAL);
    if (icon_text_format_) {
        icon_text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        icon_text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Warning icon label
    auto icon = std::make_unique<D2DLabel>(L"\u26A0");
    icon->setFontSize(22.0f);
    icon->setTextColor(Color::fromRgb(0xE6A817));
    icon->setTextAlignment(TextAlignment::Center);
    icon->createResources(res);
    icon_label_ = icon.get();
    addChild(std::move(icon));

    // Message label
    auto message = std::make_unique<D2DLabel>(formatMessage());
    message->setWordWrap(true);
    message->createResources(res);
    message_label_ = message.get();
    addChild(std::move(message));

    // File list (for multiple files)
    auto list = std::make_unique<D2DListBox>();
    list->createResources(res);
    if (files_ && files_->size() > 1) {
        for (const auto& file : *files_) {
            list->addItem(file.filename().wstring());
        }
    }
    file_list_ = list.get();
    addChild(std::move(list));

    // Trash checkbox
    auto trash = std::make_unique<D2DCheckBox>(L"Move to Trash instead of permanent delete");
    trash->createResources(res);
    if (options_.default_to_trash) {
        trash->setChecked(true);
    }
    trash_check_ = trash.get();
    addChild(std::move(trash));

    // Delete button
    auto del_btn = std::make_unique<D2DButton>(L"Delete");
    del_btn->setVariant(ButtonVariant::Primary);
    del_btn->createResources(res);
    del_btn->onClick([this]() {
        if (options_.show_trash_option && trash_check_->isChecked()) {
            result_ = DeleteConfirmResult::Trash;
        } else {
            result_ = DeleteConfirmResult::Delete;
        }
        endDialog(IDOK);
    });
    delete_button_ = del_btn.get();
    addChild(std::move(del_btn));

    // Cancel button
    auto cancel_btn = std::make_unique<D2DButton>(L"Cancel");
    cancel_btn->createResources(res);
    cancel_btn->onClick([this]() {
        result_ = DeleteConfirmResult::Cancel;
        endDialog(IDCANCEL);
    });
    cancel_button_ = cancel_btn.get();
    addChild(std::move(cancel_btn));
}

void D2DDeleteConfirmDialog::layoutComponents() {
    auto size = deviceResources().getSize();
    float content_width = size.width - kPadding * 2;
    float y = kPadding;

    // Warning icon (left) + Message (right of icon)
    float icon_x = kPadding;
    float msg_x = kPadding + kIconSize + kSmallPadding;
    float msg_width = content_width - kIconSize - kSmallPadding;

    icon_label_->arrange(Rect{icon_x, y, kIconSize, kIconSize});
    message_label_->arrange(Rect{msg_x, y, msg_width, kIconSize});
    y += kIconSize + kSmallPadding;

    // File list (only visible for multiple files)
    if (files_ && files_->size() > 1) {
        file_list_->setVisible(true);
        file_list_->arrange(Rect{kPadding, y, content_width, kListHeight});
        y += kListHeight + kSmallPadding;
    } else {
        file_list_->setVisible(false);
    }

    // Trash checkbox
    if (options_.show_trash_option) {
        trash_check_->setVisible(true);
        trash_check_->arrange(Rect{kPadding, y, content_width, kItemHeight});
        y += kItemHeight + kPadding;
    } else {
        trash_check_->setVisible(false);
        y += kSmallPadding;
    }

    // Buttons at bottom right
    float btn_area_width = kButtonWidth * 2 + kButtonSpacing;
    float btn_x = kPadding + content_width - btn_area_width;
    delete_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
    btn_x += kButtonWidth + kButtonSpacing;
    cancel_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
}

void D2DDeleteConfirmDialog::resizeToFitContent() {
    float needed_width = kContentWidth + kPadding * 2;
    float needed_height = delete_button_->bounds().bottom() + kPadding;

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

std::wstring D2DDeleteConfirmDialog::formatMessage() const {
    if (!files_ || files_->empty()) {
        return L"Are you sure you want to delete?";
    }
    if (files_->size() == 1) {
        return std::format(L"Are you sure you want to delete '{}'?",
                           (*files_)[0].filename().wstring());
    }
    return std::format(L"Are you sure you want to delete these {} files?", files_->size());
}

DeleteConfirmResult showD2DDeleteConfirmDialog(HWND parent,
                                               const std::vector<std::filesystem::path>& files,
                                               const DeleteConfirmOptions& options) {
    D2DDeleteConfirmDialog dialog;
    return dialog.show(parent, files, options);
}

}  // namespace nive::ui::d2d
