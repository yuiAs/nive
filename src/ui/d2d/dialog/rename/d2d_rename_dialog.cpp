/// @file d2d_rename_dialog.cpp
/// @brief D2D rename dialog implementation

#include "d2d_rename_dialog.hpp"

#include "core/fs/file_operations.hpp"
#include "core/i18n/i18n.hpp"

namespace nive::ui::d2d {

namespace {

// Layout constants (in DIPs)
constexpr float kPadding = 15.0f;
constexpr float kSmallPadding = 8.0f;
constexpr float kLabelHeight = 20.0f;
constexpr float kEditHeight = 28.0f;
constexpr float kButtonWidth = 90.0f;
constexpr float kButtonHeight = 28.0f;
constexpr float kButtonSpacing = 10.0f;
constexpr float kContentWidth = 320.0f;

}  // namespace

D2DRenameDialog::D2DRenameDialog() {
    setTitle(i18n::tr("dialog.rename.title"));
    float total_height = kPadding + kLabelHeight + kSmallPadding + kEditHeight + kPadding +
                         kButtonHeight + kPadding;
    setInitialSize(Size{kContentWidth + kPadding * 2, total_height});
    setResizable(false);
}

std::optional<std::wstring> D2DRenameDialog::show(HWND parent,
                                                   const std::wstring& current_name) {
    current_name_ = current_name;
    result_ = std::nullopt;

    showModal(parent);

    return result_;
}

void D2DRenameDialog::onCreate() {
    createComponents();
    layoutComponents();
    resizeToFitContent();
}

void D2DRenameDialog::onResize(float width, float height) {
    D2DDialog::onResize(width, height);
    layoutComponents();
}

void D2DRenameDialog::createComponents() {
    auto& res = deviceResources();

    // Label
    auto lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.rename.title"));
    lbl->createResources(res);
    label_ = lbl.get();
    addChild(std::move(lbl));

    // Edit box with current filename
    auto edit = std::make_unique<D2DEditBox>(current_name_);
    edit->setMaxLength(255);
    edit->createResources(res);

    // Select stem (filename without extension)
    auto dot_pos = current_name_.rfind(L'.');
    if (dot_pos != std::wstring::npos && dot_pos > 0) {
        edit->setSelection(0, dot_pos);
    } else {
        edit->selectAll();
    }

    edit_ = edit.get();
    addChild(std::move(edit));

    // OK button
    auto ok_btn = std::make_unique<D2DButton>(i18n::tr("dialog.rename.ok"));
    ok_btn->setVariant(ButtonVariant::Primary);
    ok_btn->createResources(res);
    ok_btn->onClick([this]() {
        auto new_name = edit_->text();
        if (new_name.empty() || !fs::isValidFilename(new_name)) {
            // Flash the edit box by re-focusing; do not close
            edit_->setFocused(true);
            return;
        }
        result_ = new_name;
        endDialog(IDOK);
    });
    ok_button_ = ok_btn.get();
    addChild(std::move(ok_btn));

    // Cancel button
    auto cancel_btn = std::make_unique<D2DButton>(i18n::tr("dialog.rename.cancel"));
    cancel_btn->createResources(res);
    cancel_btn->onClick([this]() { endDialog(IDCANCEL); });
    cancel_button_ = cancel_btn.get();
    addChild(std::move(cancel_btn));

    setDefaultButton(ok_button_);

    // Focus the edit box
    edit_->setFocused(true);
}

void D2DRenameDialog::layoutComponents() {
    auto size = deviceResources().getSize();
    float content_width = size.width - kPadding * 2;
    float y = kPadding;

    // Label
    label_->arrange(Rect{kPadding, y, content_width, kLabelHeight});
    y += kLabelHeight + kSmallPadding;

    // Edit box
    edit_->arrange(Rect{kPadding, y, content_width, kEditHeight});
    y += kEditHeight + kPadding;

    // Buttons at bottom right
    float btn_area_width = kButtonWidth * 2 + kButtonSpacing;
    float btn_x = kPadding + content_width - btn_area_width;
    ok_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
    btn_x += kButtonWidth + kButtonSpacing;
    cancel_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
}

void D2DRenameDialog::resizeToFitContent() {
    float needed_width = kContentWidth + kPadding * 2;
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

}  // namespace nive::ui::d2d
