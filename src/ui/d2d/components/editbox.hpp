/// @file editbox.hpp
/// @brief D2D single-line text edit box component

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <functional>
#include <string>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

/// @brief Input mode for text filtering
enum class InputMode {
    Text,     // Any text input (default)
    Integer,  // Integer numbers (digits, optional leading minus)
    Decimal,  // Decimal numbers (digits, minus, single decimal point)
};

/// @brief Single-line text input component
///
/// Supports text input, caret, selection, and clipboard operations.
class D2DEditBox : public D2DUIComponent {
public:
    using Style = StyleTraits<D2DEditBox>;
    using ChangeCallback = std::function<void(const std::wstring& text)>;

    D2DEditBox();
    explicit D2DEditBox(const std::wstring& text);
    ~D2DEditBox() override = default;

    // Properties

    [[nodiscard]] const std::wstring& text() const noexcept { return text_; }
    void setText(const std::wstring& text);

    [[nodiscard]] const std::wstring& placeholder() const noexcept { return placeholder_; }
    void setPlaceholder(const std::wstring& placeholder);

    [[nodiscard]] bool isReadOnly() const noexcept { return read_only_; }
    void setReadOnly(bool read_only);

    [[nodiscard]] size_t maxLength() const noexcept { return max_length_; }
    void setMaxLength(size_t max_length);

    [[nodiscard]] InputMode inputMode() const noexcept { return input_mode_; }
    void setInputMode(InputMode mode) noexcept { input_mode_ = mode; }

    // Selection

    [[nodiscard]] size_t caretPosition() const noexcept { return caret_pos_; }
    void setCaretPosition(size_t pos);

    [[nodiscard]] bool hasSelection() const noexcept { return selection_start_ != selection_end_; }
    [[nodiscard]] std::wstring selectedText() const;
    void selectAll();
    void setSelection(size_t start, size_t end);
    void clearSelection();

    /// @brief Set text change callback
    void onChange(ChangeCallback callback) { on_change_ = std::move(callback); }

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void render(ID2D1RenderTarget* rt) override;

    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onChar(const KeyEvent& event) override;
    void onFocusChanged(const FocusEvent& event) override;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    void insertText(const std::wstring& text);
    void deleteSelection();
    void deleteCharacter(bool forward);
    void moveCaret(int delta, bool extend_selection);
    void moveCaretToWord(bool forward, bool extend_selection);
    void moveCaretToEnd(bool start, bool extend_selection);

    void copyToClipboard();
    void cutToClipboard();
    void pasteFromClipboard();

    [[nodiscard]] size_t hitTestPosition(float x) const;
    [[nodiscard]] float getCaretX() const;
    void updateTextLayout();
    void ensureCaretVisible();
    [[nodiscard]] bool isValidInputChar(wchar_t ch) const;

    [[nodiscard]] Rect getContentRect() const noexcept;

    std::wstring text_;
    std::wstring placeholder_;
    size_t caret_pos_ = 0;
    size_t selection_start_ = 0;
    size_t selection_end_ = 0;
    size_t max_length_ = 0;  // 0 = no limit
    bool read_only_ = false;
    bool dragging_ = false;
    InputMode input_mode_ = InputMode::Text;

    float scroll_offset_ = 0.0f;  // Horizontal scroll for long text

    ChangeCallback on_change_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;
    ComPtr<IDWriteTextLayout> placeholder_layout_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> background_brush_;
    ComPtr<ID2D1SolidColorBrush> border_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1SolidColorBrush> placeholder_brush_;
    ComPtr<ID2D1SolidColorBrush> selection_brush_;
    ComPtr<ID2D1SolidColorBrush> caret_brush_;
};

}  // namespace nive::ui::d2d
