/// @file d2d_about_dialog.cpp
/// @brief D2D About dialog implementation

#include "d2d_about_dialog.hpp"

#include <shellapi.h>

#include <format>

#include "core/i18n/i18n.hpp"
#include "ui/d2d/styles/default_style.hpp"
#include "version.h"

namespace nive::ui::d2d {

namespace {

// Layout constants (in DIPs)
constexpr float kPadding = 20.0f;
constexpr float kSmallPadding = 6.0f;
constexpr float kAppNameHeight = 30.0f;
constexpr float kVersionHeight = 20.0f;
constexpr float kUrlHeight = 20.0f;
constexpr float kButtonWidth = 90.0f;
constexpr float kButtonHeight = 28.0f;
constexpr float kContentWidth = 300.0f;
constexpr float kAppNameFontSize = 20.0f;

constexpr wchar_t kSiteUrl[] = L"https://github.com/yuiAs/nive";

/// @brief Clickable label that opens a URL in the default browser
class D2DLinkLabel : public D2DLabel {
public:
    explicit D2DLinkLabel(const std::wstring& url) : D2DLabel(url), url_(url) {
        setTextColor(CommonStyle::accent());
        setTextAlignment(TextAlignment::Leading);
    }

    bool onMouseDown(const MouseEvent& event) override {
        ShellExecuteW(nullptr, L"open", url_.c_str(), nullptr, nullptr, SW_SHOW);
        return true;
    }

    HCURSOR cursor() const override { return LoadCursorW(nullptr, IDC_HAND); }

private:
    std::wstring url_;
};

}  // namespace

D2DAboutDialog::D2DAboutDialog() {
    setTitle(i18n::tr("dialog.about.title"));
    float total_height =
        kPadding + kAppNameHeight + kSmallPadding + kVersionHeight + kSmallPadding + kUrlHeight +
        kPadding + kButtonHeight + kPadding;
    setInitialSize(Size{kContentWidth + kPadding * 2, total_height});
    setResizable(false);
}

void D2DAboutDialog::show(HWND parent) {
    showModal(parent);
}

void D2DAboutDialog::onCreate() {
    createComponents();
    layoutComponents();
}

void D2DAboutDialog::onResize(float width, float height) {
    D2DDialog::onResize(width, height);
    layoutComponents();
}

void D2DAboutDialog::createComponents() {
    auto& res = deviceResources();

    // Application name
    auto name_lbl = std::make_unique<D2DLabel>(L"nive");
    name_lbl->setFontSize(kAppNameFontSize);
    name_lbl->setTextAlignment(TextAlignment::Leading);
    name_lbl->setFocusable(false);
    name_lbl->createResources(res);
    app_name_label_ = name_lbl.get();
    addChild(std::move(name_lbl));

    // Version string: "x.y.z (hash)"
    auto version_text =
        std::format(L"{} ({})", L"" NIVE_VERSION_STRING, L"" NIVE_GIT_HASH_SHORT);
    auto ver_lbl = std::make_unique<D2DLabel>(version_text);
    ver_lbl->setTextAlignment(TextAlignment::Leading);
    ver_lbl->setTextColor(Color::fromRgb(0x666666));
    ver_lbl->setFocusable(false);
    ver_lbl->createResources(res);
    version_label_ = ver_lbl.get();
    addChild(std::move(ver_lbl));

    // Site URL (clickable link)
    auto link = std::make_unique<D2DLinkLabel>(kSiteUrl);
    link->setFocusable(false);
    link->createResources(res);
    url_label_ = link.get();
    addChild(std::move(link));

    // OK button
    auto ok_btn = std::make_unique<D2DButton>(i18n::tr("dialog.about.ok"));
    ok_btn->setVariant(ButtonVariant::Primary);
    ok_btn->createResources(res);
    ok_btn->onClick([this]() { endDialog(IDOK); });
    ok_button_ = ok_btn.get();
    addChild(std::move(ok_btn));

    setDefaultButton(ok_button_);
}

void D2DAboutDialog::layoutComponents() {
    auto size = deviceResources().getSize();
    float content_width = size.width - kPadding * 2;
    float y = kPadding;

    // App name (centered)
    app_name_label_->arrange(Rect{kPadding, y, content_width, kAppNameHeight});
    y += kAppNameHeight + kSmallPadding;

    // Version
    version_label_->arrange(Rect{kPadding, y, content_width, kVersionHeight});
    y += kVersionHeight + kSmallPadding;

    // URL
    url_label_->arrange(Rect{kPadding, y, content_width, kUrlHeight});
    y += kUrlHeight + kPadding;

    // OK button (right-aligned)
    float btn_x = kPadding + content_width - kButtonWidth;
    ok_button_->arrange(Rect{btn_x, y, kButtonWidth, kButtonHeight});
}

void showD2DAboutDialog(HWND parent) {
    D2DAboutDialog dialog;
    dialog.show(parent);
}

}  // namespace nive::ui::d2d
