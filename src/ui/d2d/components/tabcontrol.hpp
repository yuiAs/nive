/// @file tabcontrol.hpp
/// @brief D2D tab control component

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <functional>
#include <string>
#include <vector>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

/// @brief Tab page data
struct TabPage {
    std::wstring title;
    D2DContainerComponent* content = nullptr;  // Owned by the container
};

/// @brief Style traits for tab control
struct TabControlStyle {
    static constexpr Color tabBackground() { return Color::fromRgb(0xF0F0F0); }
    static constexpr Color tabBackgroundActive() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color tabBackgroundHover() { return Color::fromRgb(0xE5E5E5); }
    static constexpr Color tabBorder() { return CommonStyle::border(); }
    static constexpr Color tabText() { return CommonStyle::foreground(); }
    static constexpr Color tabTextActive() { return CommonStyle::foreground(); }
    static constexpr Color contentBackground() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color contentBorder() { return CommonStyle::border(); }

    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }
    static constexpr float tabHeight() { return 28.0f; }
    static constexpr float tabPadding() { return 12.0f; }
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return 4.0f; }
};

/// @brief Tab control with multiple pages
///
/// Displays a row of tabs at the top with a content area below.
/// Each tab can have its own content container.
class D2DTabControl : public D2DContainerComponent {
public:
    using Style = TabControlStyle;
    using SelectCallback = std::function<void(int index)>;

    D2DTabControl();
    ~D2DTabControl() override = default;

    // Tab management

    /// @brief Add a tab with content
    /// @param title Tab title
    /// @param content Content container (takes ownership)
    /// @return Index of the new tab
    int addTab(const std::wstring& title, std::unique_ptr<D2DContainerComponent> content);

    /// @brief Remove a tab
    /// @param index Tab index to remove
    void removeTab(int index);

    /// @brief Get tab count
    [[nodiscard]] int tabCount() const noexcept { return static_cast<int>(tabs_.size()); }

    /// @brief Get tab title
    [[nodiscard]] const std::wstring& tabTitle(int index) const;

    /// @brief Set tab title
    void setTabTitle(int index, const std::wstring& title);

    // Selection

    [[nodiscard]] int selectedIndex() const noexcept { return selected_index_; }
    void setSelectedIndex(int index);

    /// @brief Set selection change callback
    void onSelect(SelectCallback callback) { on_select_ = std::move(callback); }

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void arrange(const Rect& bounds) override;
    void render(ID2D1RenderTarget* rt) override;

    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseLeave(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onChar(const KeyEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

    /// @brief Get the content area bounds
    [[nodiscard]] Rect contentArea() const noexcept {
        return content_bounds_.translated(bounds_.x, bounds_.y);
    }

private:
    struct TabInfo {
        std::wstring title;
        std::unique_ptr<D2DContainerComponent> content;
        Rect tab_bounds;  // Bounds of the tab header
        float title_width = 0.0f;
    };

    [[nodiscard]] int hitTestTab(const Point& point) const;
    void updateTabWidths();
    void layoutTabs();

    std::vector<TabInfo> tabs_;
    int selected_index_ = -1;
    int hovered_index_ = -1;
    SelectCallback on_select_;

    Rect tab_strip_bounds_;
    Rect content_bounds_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> tab_bg_brush_;
    ComPtr<ID2D1SolidColorBrush> tab_bg_active_brush_;
    ComPtr<ID2D1SolidColorBrush> tab_bg_hover_brush_;
    ComPtr<ID2D1SolidColorBrush> tab_border_brush_;
    ComPtr<ID2D1SolidColorBrush> tab_text_brush_;
    ComPtr<ID2D1SolidColorBrush> content_bg_brush_;
    ComPtr<ID2D1SolidColorBrush> content_border_brush_;
};

}  // namespace nive::ui::d2d
