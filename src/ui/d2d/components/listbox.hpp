/// @file listbox.hpp
/// @brief D2D list box component

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

// Forward declaration for style traits
class D2DListBox;

/// @brief Style traits for D2DListBox
template <>
struct StyleTraits<D2DListBox> {
    // Normal state
    static constexpr Color background() { return Color::fromRgb(0xFFFFFF); }
    static constexpr Color border() { return CommonStyle::border(); }
    static constexpr Color textColor() { return CommonStyle::foreground(); }

    // Focused state
    static constexpr Color focusedBorder() { return CommonStyle::borderFocused(); }

    // Item states
    static constexpr Color itemHoverBackground() { return CommonStyle::hoverBackground(); }
    static constexpr Color selectedItemBackground() { return CommonStyle::accent(); }
    static constexpr Color selectedItemTextColor() { return Color::white(); }

    // Disabled state
    static constexpr Color disabledBackground() { return CommonStyle::disabledBackground(); }
    static constexpr Color disabledBorder() { return Color::fromRgb(0xD0D0D0); }
    static constexpr Color disabledTextColor() { return CommonStyle::disabled(); }

    // Scrollbar
    static constexpr Color scrollbarTrack() { return Color::fromRgb(0xF0F0F0); }
    static constexpr Color scrollbarThumb() { return Color::fromRgb(0xC0C0C0); }
    static constexpr Color scrollbarThumbHover() { return Color::fromRgb(0xA0A0A0); }

    // Typography
    static constexpr const wchar_t* fontFamily() { return CommonStyle::fontFamily(); }
    static constexpr float fontSize() { return CommonStyle::fontSize(); }

    // Layout
    static constexpr float borderWidth() { return 1.0f; }
    static constexpr float borderRadius() { return CommonStyle::borderRadius(); }
    static constexpr Thickness padding() { return Thickness{4.0f, 4.0f, 4.0f, 4.0f}; }
    static constexpr float itemHeight() { return 22.0f; }
    static constexpr float itemPadding() { return 6.0f; }
    static constexpr float scrollbarWidth() { return 12.0f; }
};

/// @brief List box component for displaying and selecting items
///
/// Supports single selection, keyboard navigation, and scrolling.
class D2DListBox : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DListBox>;
    using SelectionCallback = std::function<void(int index)>;
    using DoubleClickCallback = std::function<void(int index)>;

    D2DListBox();
    ~D2DListBox() override = default;

    // Item management

    /// @brief Add an item to the list
    void addItem(const std::wstring& text);

    /// @brief Add multiple items
    void addItems(const std::vector<std::wstring>& items);

    /// @brief Insert item at index
    void insertItem(size_t index, const std::wstring& text);

    /// @brief Remove item at index
    void removeItem(size_t index);

    /// @brief Remove all items
    void clearItems();

    /// @brief Get item count
    [[nodiscard]] size_t itemCount() const noexcept { return items_.size(); }

    /// @brief Get item text at index
    [[nodiscard]] const std::wstring& itemAt(size_t index) const;

    // Selection

    /// @brief Get selected index (-1 if none)
    [[nodiscard]] int selectedIndex() const noexcept { return selected_index_; }

    /// @brief Set selected index
    void setSelectedIndex(int index);

    /// @brief Get selected item text (empty if none)
    [[nodiscard]] std::wstring selectedText() const;

    /// @brief Set selection callback
    void onSelectionChanged(SelectionCallback callback) {
        on_selection_changed_ = std::move(callback);
    }

    /// @brief Set double-click callback
    void onDoubleClick(DoubleClickCallback callback) { on_double_click_ = std::move(callback); }

    // Scrolling

    /// @brief Get scroll offset
    [[nodiscard]] float scrollOffset() const noexcept { return scroll_offset_; }

    /// @brief Set scroll offset
    void setScrollOffset(float offset);

    /// @brief Ensure selected item is visible
    void ensureSelectedVisible();

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void render(ID2D1RenderTarget* rt) override;

    bool onMouseEnter(const MouseEvent& event) override;
    bool onMouseLeave(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    void onFocusChanged(const FocusEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    [[nodiscard]] Rect contentRect() const;
    [[nodiscard]] Rect scrollbarRect() const;
    [[nodiscard]] Rect scrollThumbRect() const;
    [[nodiscard]] float totalContentHeight() const;
    [[nodiscard]] float maxScrollOffset() const;
    [[nodiscard]] int hitTestItem(const Point& point) const;
    [[nodiscard]] bool isScrollbarNeeded() const;
    void clampScrollOffset();
    void renderScrollbar(ID2D1RenderTarget* rt);

    std::vector<std::wstring> items_;
    int selected_index_ = -1;
    int hovered_index_ = -1;
    float scroll_offset_ = 0.0f;
    bool scrollbar_hovered_ = false;
    bool scrollbar_dragging_ = false;
    float drag_start_offset_ = 0.0f;
    float drag_start_y_ = 0.0f;

    SelectionCallback on_selection_changed_;
    DoubleClickCallback on_double_click_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> background_brush_;
    ComPtr<ID2D1SolidColorBrush> border_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1SolidColorBrush> item_hover_brush_;
    ComPtr<ID2D1SolidColorBrush> selected_item_brush_;
    ComPtr<ID2D1SolidColorBrush> selected_item_text_brush_;
    ComPtr<ID2D1SolidColorBrush> scrollbar_track_brush_;
    ComPtr<ID2D1SolidColorBrush> scrollbar_thumb_brush_;

    static const std::wstring kEmptyString;
};

}  // namespace nive::ui::d2d
