/// @file editbox.cpp
/// @brief Implementation of D2D edit box component

#include "editbox.hpp"

#include <algorithm>

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

D2DEditBox::D2DEditBox() = default;

D2DEditBox::D2DEditBox(const std::wstring& text) : text_(text), caret_pos_(text.length()) {
}

void D2DEditBox::setText(const std::wstring& text) {
    if (text_ != text) {
        text_ = text;
        caret_pos_ = std::min(caret_pos_, text_.length());
        selection_start_ = selection_end_ = caret_pos_;
        text_layout_.Reset();
        invalidate();
    }
}

void D2DEditBox::setPlaceholder(const std::wstring& placeholder) {
    if (placeholder_ != placeholder) {
        placeholder_ = placeholder;
        placeholder_layout_.Reset();
        invalidate();
    }
}

void D2DEditBox::setReadOnly(bool read_only) {
    read_only_ = read_only;
}

void D2DEditBox::setMaxLength(size_t max_length) {
    max_length_ = max_length;
    if (max_length_ > 0 && text_.length() > max_length_) {
        text_.resize(max_length_);
        caret_pos_ = std::min(caret_pos_, text_.length());
        selection_start_ = selection_end_ = caret_pos_;
        text_layout_.Reset();
        invalidate();
    }
}

void D2DEditBox::setCaretPosition(size_t pos) {
    pos = std::min(pos, text_.length());
    if (caret_pos_ != pos) {
        caret_pos_ = pos;
        selection_start_ = selection_end_ = pos;
        invalidate();
    }
}

std::wstring D2DEditBox::selectedText() const {
    if (!hasSelection()) {
        return {};
    }
    size_t start = std::min(selection_start_, selection_end_);
    size_t end = std::max(selection_start_, selection_end_);
    return text_.substr(start, end - start);
}

void D2DEditBox::selectAll() {
    selection_start_ = 0;
    selection_end_ = text_.length();
    caret_pos_ = text_.length();
    invalidate();
}

void D2DEditBox::clearSelection() {
    selection_start_ = selection_end_ = caret_pos_;
    invalidate();
}

Rect D2DEditBox::getContentRect() const noexcept {
    Thickness pad = Style::padding();
    return Rect{bounds_.x + pad.left, bounds_.y + pad.top, bounds_.width - pad.horizontalTotal(),
                bounds_.height - pad.verticalTotal()};
}

void D2DEditBox::createResources(DeviceResources& resources) {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid()) {
        return;
    }

    // Create text format
    if (!text_format_) {
        factory.dwriteFactory()->CreateTextFormat(
            Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL, Style::fontSize(), L"", &text_format_);

        if (text_format_) {
            text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        }
    }

    // Create brushes
    Color bg_color = enabled_ ? Style::background() : Style::disabledBackground();
    Color border_color;
    if (!enabled_) {
        border_color = Style::disabledBorder();
    } else if (focused_) {
        border_color = Style::focusedBorder();
    } else if (hovered_) {
        border_color = Style::hoverBorder();
    } else {
        border_color = Style::border();
    }
    Color text_color = enabled_ ? Style::textColor() : Style::disabledTextColor();

    background_brush_ = resources.createSolidBrush(bg_color);
    border_brush_ = resources.createSolidBrush(border_color);
    text_brush_ = resources.createSolidBrush(text_color);
    placeholder_brush_ = resources.createSolidBrush(Style::placeholderColor());
    selection_brush_ = resources.createSolidBrush(Style::selectionBackground());
    caret_brush_ = resources.createSolidBrush(Style::caretColor());
}

void D2DEditBox::updateTextLayout() {
    auto& factory = D2DFactory::instance();
    if (!factory.isValid() || !text_format_) {
        return;
    }

    Rect content = getContentRect();

    if (!text_.empty()) {
        text_layout_.Reset();
        factory.dwriteFactory()->CreateTextLayout(
            text_.c_str(), static_cast<UINT32>(text_.length()), text_format_.Get(),
            10000.0f,  // Wide to prevent wrapping
            content.height, &text_layout_);
    }

    if (!placeholder_.empty()) {
        placeholder_layout_.Reset();
        factory.dwriteFactory()->CreateTextLayout(
            placeholder_.c_str(), static_cast<UINT32>(placeholder_.length()), text_format_.Get(),
            content.width, content.height, &placeholder_layout_);
    }
}

Size D2DEditBox::measure(const Size& available_size) {
    Thickness pad = Style::padding();
    float height = Style::height();
    float width = available_size.width > 0 ? available_size.width : 200.0f;

    desired_size_ = {width, height};
    return desired_size_;
}

void D2DEditBox::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }

    // Draw background
    D2D1_ROUNDED_RECT rounded_rect =
        D2D1::RoundedRect(bounds_.toD2D(), Style::borderRadius(), Style::borderRadius());

    if (background_brush_) {
        rt->FillRoundedRectangle(rounded_rect, background_brush_.Get());
    }
    if (border_brush_) {
        rt->DrawRoundedRectangle(rounded_rect, border_brush_.Get(), Style::borderWidth());
    }

    // Set clip rect for content
    Rect content = getContentRect();
    rt->PushAxisAlignedClip(content.toD2D(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    // Update layout if needed
    if (!text_layout_ && !text_.empty()) {
        updateTextLayout();
    }
    if (!placeholder_layout_ && !placeholder_.empty()) {
        updateTextLayout();
    }

    float text_x = content.x - scroll_offset_;
    float text_y = content.y;

    // Draw selection highlight
    if (hasSelection() && focused_ && text_layout_) {
        size_t sel_start = std::min(selection_start_, selection_end_);
        size_t sel_end = std::max(selection_start_, selection_end_);

        DWRITE_HIT_TEST_METRICS start_metrics, end_metrics;
        float start_x, end_x, y;

        text_layout_->HitTestTextPosition(static_cast<UINT32>(sel_start), FALSE, &start_x, &y,
                                          &start_metrics);
        text_layout_->HitTestTextPosition(static_cast<UINT32>(sel_end), FALSE, &end_x, &y,
                                          &end_metrics);

        D2D1_RECT_F sel_rect =
            D2D1::RectF(text_x + start_x, content.y, text_x + end_x, content.bottom());

        if (selection_brush_) {
            rt->FillRectangle(sel_rect, selection_brush_.Get());
        }
    }

    // Draw text or placeholder
    if (!text_.empty() && text_layout_ && text_brush_) {
        rt->DrawTextLayout(D2D1::Point2F(text_x, text_y), text_layout_.Get(), text_brush_.Get());
    } else if (text_.empty() && placeholder_layout_ && placeholder_brush_) {
        rt->DrawTextLayout(D2D1::Point2F(content.x, text_y), placeholder_layout_.Get(),
                           placeholder_brush_.Get());
    }

    // Draw caret
    if (focused_ && caret_brush_) {
        float caret_x = text_x + getCaretX();
        D2D1_RECT_F caret_rect = D2D1::RectF(
            caret_x, content.y + 2.0f, caret_x + Style::caretWidth(), content.bottom() - 2.0f);
        rt->FillRectangle(caret_rect, caret_brush_.Get());
    }

    rt->PopAxisAlignedClip();
}

float D2DEditBox::getCaretX() const {
    if (!text_layout_ || text_.empty()) {
        return 0.0f;
    }

    float x, y;
    DWRITE_HIT_TEST_METRICS metrics;
    text_layout_->HitTestTextPosition(static_cast<UINT32>(caret_pos_), FALSE, &x, &y, &metrics);

    return x;
}

size_t D2DEditBox::hitTestPosition(float x) const {
    if (!text_layout_ || text_.empty()) {
        return 0;
    }

    // x is in local coordinates (0 = left edge of this component)
    // Convert to text layout coordinates by subtracting padding
    Thickness pad = Style::padding();
    float local_x = x - pad.left + scroll_offset_;
    float local_y = (bounds_.height - pad.verticalTotal()) / 2.0f;

    BOOL is_trailing, is_inside;
    DWRITE_HIT_TEST_METRICS metrics;
    text_layout_->HitTestPoint(local_x, local_y, &is_trailing, &is_inside, &metrics);

    return static_cast<size_t>(metrics.textPosition) + (is_trailing ? 1 : 0);
}

void D2DEditBox::ensureCaretVisible() {
    Rect content = getContentRect();
    float caret_x = getCaretX();

    // Scroll left if caret is before visible area
    if (caret_x < scroll_offset_) {
        scroll_offset_ = caret_x;
    }
    // Scroll right if caret is after visible area
    else if (caret_x > scroll_offset_ + content.width) {
        scroll_offset_ = caret_x - content.width;
    }
}

void D2DEditBox::insertText(const std::wstring& text) {
    if (read_only_ || text.empty()) {
        return;
    }

    // Delete selection first
    if (hasSelection()) {
        deleteSelection();
    }

    // Check max length
    std::wstring to_insert = text;
    if (max_length_ > 0) {
        size_t available = max_length_ - text_.length();
        if (available == 0) {
            return;
        }
        if (to_insert.length() > available) {
            to_insert = to_insert.substr(0, available);
        }
    }

    text_.insert(caret_pos_, to_insert);
    caret_pos_ += to_insert.length();
    selection_start_ = selection_end_ = caret_pos_;

    text_layout_.Reset();
    ensureCaretVisible();
    invalidate();

    if (on_change_) {
        on_change_(text_);
    }
}

void D2DEditBox::deleteSelection() {
    if (!hasSelection()) {
        return;
    }

    size_t start = std::min(selection_start_, selection_end_);
    size_t end = std::max(selection_start_, selection_end_);

    text_.erase(start, end - start);
    caret_pos_ = start;
    selection_start_ = selection_end_ = caret_pos_;

    text_layout_.Reset();
    ensureCaretVisible();
    invalidate();

    if (on_change_) {
        on_change_(text_);
    }
}

void D2DEditBox::deleteCharacter(bool forward) {
    if (read_only_) {
        return;
    }

    if (hasSelection()) {
        deleteSelection();
        return;
    }

    if (forward) {
        // Delete key
        if (caret_pos_ < text_.length()) {
            text_.erase(caret_pos_, 1);
            text_layout_.Reset();
            invalidate();
            if (on_change_) {
                on_change_(text_);
            }
        }
    } else {
        // Backspace
        if (caret_pos_ > 0) {
            caret_pos_--;
            text_.erase(caret_pos_, 1);
            selection_start_ = selection_end_ = caret_pos_;
            text_layout_.Reset();
            ensureCaretVisible();
            invalidate();
            if (on_change_) {
                on_change_(text_);
            }
        }
    }
}

void D2DEditBox::moveCaret(int delta, bool extend_selection) {
    size_t new_pos = caret_pos_;
    if (delta < 0 && static_cast<size_t>(-delta) > caret_pos_) {
        new_pos = 0;
    } else if (delta > 0 && caret_pos_ + delta > text_.length()) {
        new_pos = text_.length();
    } else {
        new_pos = caret_pos_ + delta;
    }

    caret_pos_ = new_pos;

    if (extend_selection) {
        selection_end_ = caret_pos_;
    } else {
        selection_start_ = selection_end_ = caret_pos_;
    }

    ensureCaretVisible();
    invalidate();
}

void D2DEditBox::moveCaretToWord(bool forward, bool extend_selection) {
    if (forward) {
        // Move to end of current/next word
        size_t pos = caret_pos_;
        // Skip whitespace
        while (pos < text_.length() && iswspace(text_[pos])) {
            pos++;
        }
        // Skip word characters
        while (pos < text_.length() && !iswspace(text_[pos])) {
            pos++;
        }
        caret_pos_ = pos;
    } else {
        // Move to start of current/previous word
        size_t pos = caret_pos_;
        if (pos > 0)
            pos--;
        // Skip whitespace
        while (pos > 0 && iswspace(text_[pos])) {
            pos--;
        }
        // Skip word characters
        while (pos > 0 && !iswspace(text_[pos - 1])) {
            pos--;
        }
        caret_pos_ = pos;
    }

    if (extend_selection) {
        selection_end_ = caret_pos_;
    } else {
        selection_start_ = selection_end_ = caret_pos_;
    }

    ensureCaretVisible();
    invalidate();
}

void D2DEditBox::moveCaretToEnd(bool start, bool extend_selection) {
    caret_pos_ = start ? 0 : text_.length();

    if (extend_selection) {
        selection_end_ = caret_pos_;
    } else {
        selection_start_ = selection_end_ = caret_pos_;
    }

    ensureCaretVisible();
    invalidate();
}

void D2DEditBox::copyToClipboard() {
    if (!hasSelection()) {
        return;
    }

    std::wstring text = selectedText();
    if (text.empty()) {
        return;
    }

    if (!OpenClipboard(nullptr)) {
        return;
    }

    EmptyClipboard();

    size_t size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem) {
        wchar_t* dest = static_cast<wchar_t*>(GlobalLock(hMem));
        if (dest) {
            wcscpy_s(dest, text.length() + 1, text.c_str());
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
    }

    CloseClipboard();
}

void D2DEditBox::cutToClipboard() {
    if (read_only_ || !hasSelection()) {
        return;
    }

    copyToClipboard();
    deleteSelection();
}

void D2DEditBox::pasteFromClipboard() {
    if (read_only_) {
        return;
    }

    if (!OpenClipboard(nullptr)) {
        return;
    }

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData) {
        wchar_t* text = static_cast<wchar_t*>(GlobalLock(hData));
        if (text) {
            // Filter to single line (remove newlines)
            std::wstring clean_text;
            for (const wchar_t* p = text; *p; ++p) {
                if (*p != L'\r' && *p != L'\n') {
                    clean_text += *p;
                }
            }
            GlobalUnlock(hData);
            insertText(clean_text);
        }
    }

    CloseClipboard();
}

bool D2DEditBox::onMouseDown(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }

    // Request focus
    if (parent_) {
        parent_->requestFocus(this);
    }

    size_t pos = hitTestPosition(event.position.x);

    if (hasModifier(event.modifiers, Modifiers::Shift)) {
        // Extend selection
        selection_end_ = pos;
        caret_pos_ = pos;
    } else {
        // Start new selection
        caret_pos_ = pos;
        selection_start_ = selection_end_ = pos;
    }

    dragging_ = true;
    ensureCaretVisible();
    invalidate();
    return true;
}

bool D2DEditBox::onMouseMove(const MouseEvent& event) {
    if (!enabled_ || !dragging_) {
        return false;
    }

    size_t pos = hitTestPosition(event.position.x);
    selection_end_ = pos;
    caret_pos_ = pos;
    ensureCaretVisible();
    invalidate();
    return true;
}

bool D2DEditBox::onMouseUp(const MouseEvent& event) {
    if (!enabled_ || event.button != MouseButton::Left) {
        return false;
    }
    dragging_ = false;
    return true;
}

bool D2DEditBox::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    bool shift = hasModifier(event.modifiers, Modifiers::Shift);
    bool ctrl = hasModifier(event.modifiers, Modifiers::Ctrl);

    switch (event.keyCode) {
    case VK_LEFT:
        if (ctrl) {
            moveCaretToWord(false, shift);
        } else {
            moveCaret(-1, shift);
        }
        return true;

    case VK_RIGHT:
        if (ctrl) {
            moveCaretToWord(true, shift);
        } else {
            moveCaret(1, shift);
        }
        return true;

    case VK_HOME:
        moveCaretToEnd(true, shift);
        return true;

    case VK_END:
        moveCaretToEnd(false, shift);
        return true;

    case VK_BACK:
        deleteCharacter(false);
        return true;

    case VK_DELETE:
        deleteCharacter(true);
        return true;

    case 'A':
        if (ctrl) {
            selectAll();
            return true;
        }
        break;

    case 'C':
        if (ctrl) {
            copyToClipboard();
            return true;
        }
        break;

    case 'X':
        if (ctrl) {
            cutToClipboard();
            return true;
        }
        break;

    case 'V':
        if (ctrl) {
            pasteFromClipboard();
            return true;
        }
        break;
    }

    return false;
}

bool D2DEditBox::onChar(const KeyEvent& event) {
    if (!enabled_ || read_only_) {
        return false;
    }

    // Filter control characters
    wchar_t ch = event.character;
    if (ch < 32 && ch != L'\t') {
        return false;
    }

    // Don't handle tabs for now
    if (ch == L'\t') {
        return false;
    }

    // Check if character is valid for current input mode
    if (!isValidInputChar(ch)) {
        return false;
    }

    insertText(std::wstring(1, ch));
    return true;
}

bool D2DEditBox::isValidInputChar(wchar_t ch) const {
    switch (input_mode_) {
    case InputMode::Text:
        // Accept any printable character
        return true;

    case InputMode::Integer:
        // Accept digits and minus sign (only at start)
        if (ch >= L'0' && ch <= L'9') {
            return true;
        }
        if (ch == L'-') {
            // Minus only allowed at the beginning
            // Check if selection includes position 0 or caret is at 0
            size_t insert_pos =
                hasSelection() ? std::min(selection_start_, selection_end_) : caret_pos_;
            return insert_pos == 0 && text_.find(L'-') == std::wstring::npos;
        }
        return false;

    case InputMode::Decimal:
        // Accept digits, minus sign (only at start), and decimal point (only one)
        if (ch >= L'0' && ch <= L'9') {
            return true;
        }
        if (ch == L'-') {
            size_t insert_pos =
                hasSelection() ? std::min(selection_start_, selection_end_) : caret_pos_;
            return insert_pos == 0 && text_.find(L'-') == std::wstring::npos;
        }
        if (ch == L'.') {
            // Only one decimal point allowed
            return text_.find(L'.') == std::wstring::npos;
        }
        return false;
    }
    return false;
}

void D2DEditBox::onFocusChanged(const FocusEvent& event) {
    if (!event.gained) {
        // Clear selection on focus loss (optional behavior)
        // clearSelection();
    }
    invalidate();
}

}  // namespace nive::ui::d2d
