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

void D2DEditBox::setSelection(size_t start, size_t end) {
    start = std::min(start, text_.length());
    end = std::min(end, text_.length());
    selection_start_ = start;
    selection_end_ = end;
    caret_pos_ = end;
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

    // Draw caret (with blink)
    if (focused_ && caret_brush_) {
        UINT blink_time = GetCaretBlinkTime();
        bool caret_visible = true;
        if (blink_time != INFINITE && blink_time > 0) {
            DWORD elapsed = GetTickCount() - caret_blink_reset_time_;
            // Caret is visible for the first half of the blink cycle
            caret_visible = (elapsed % (blink_time * 2)) < blink_time;
        }
        if (caret_visible) {
            float caret_x = text_x + getCaretX();
            D2D1_RECT_F caret_rect =
                D2D1::RectF(caret_x, content.y + 2.0f, caret_x + Style::caretWidth(),
                            content.bottom() - 2.0f);
            rt->FillRectangle(caret_rect, caret_brush_.Get());
        }
    }

    rt->PopAxisAlignedClip();
}

void D2DEditBox::resetCaretBlink() {
    caret_blink_reset_time_ = GetTickCount();
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

    resetCaretBlink();
}

void D2DEditBox::pushUndoState() {
    // Truncate any redo history beyond current position
    if (undo_index_ < undo_stack_.size()) {
        undo_stack_.resize(undo_index_);
    }
    undo_stack_.push_back({text_, caret_pos_, selection_start_, selection_end_});
    if (undo_stack_.size() > kMaxUndoHistory) {
        undo_stack_.erase(undo_stack_.begin());
    }
    undo_index_ = undo_stack_.size();
}

void D2DEditBox::undo() {
    if (undo_index_ == 0) {
        return;
    }

    // Save current state for redo if we're at the top
    if (undo_index_ == undo_stack_.size()) {
        undo_stack_.push_back({text_, caret_pos_, selection_start_, selection_end_});
    }

    undo_index_--;
    const auto& state = undo_stack_[undo_index_];
    text_ = state.text;
    caret_pos_ = state.caret_pos;
    selection_start_ = state.selection_start;
    selection_end_ = state.selection_end;

    text_layout_.Reset();
    ensureCaretVisible();
    invalidate();

    if (on_change_) {
        on_change_(text_);
    }
}

void D2DEditBox::redo() {
    if (undo_index_ + 1 >= undo_stack_.size()) {
        return;
    }

    undo_index_++;
    const auto& state = undo_stack_[undo_index_];
    text_ = state.text;
    caret_pos_ = state.caret_pos;
    selection_start_ = state.selection_start;
    selection_end_ = state.selection_end;

    text_layout_.Reset();
    ensureCaretVisible();
    invalidate();

    if (on_change_) {
        on_change_(text_);
    }
}

void D2DEditBox::insertText(const std::wstring& text) {
    if (read_only_ || text.empty()) {
        return;
    }

    pushUndoState();

    // Delete selection first (no separate undo state - part of insert)
    if (hasSelection()) {
        size_t start = std::min(selection_start_, selection_end_);
        size_t end = std::max(selection_start_, selection_end_);
        text_.erase(start, end - start);
        caret_pos_ = start;
        selection_start_ = selection_end_ = caret_pos_;
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

    pushUndoState();
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
            pushUndoState();
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
            pushUndoState();
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

void D2DEditBox::deleteWord(bool forward) {
    if (read_only_) {
        return;
    }

    if (hasSelection()) {
        deleteSelection();
        return;
    }

    if (forward) {
        size_t word_end = findWordEnd(caret_pos_);
        if (word_end > caret_pos_) {
            pushUndoState();
            text_.erase(caret_pos_, word_end - caret_pos_);
            selection_start_ = selection_end_ = caret_pos_;
            text_layout_.Reset();
            invalidate();
            if (on_change_) {
                on_change_(text_);
            }
        }
    } else {
        size_t word_start = findWordStart(caret_pos_);
        if (word_start < caret_pos_) {
            pushUndoState();
            text_.erase(word_start, caret_pos_ - word_start);
            caret_pos_ = word_start;
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

size_t D2DEditBox::findWordStart(size_t pos) const {
    if (pos == 0 || text_.empty()) {
        return 0;
    }
    size_t p = pos;
    if (p > 0) {
        p--;
    }
    // Skip whitespace backwards
    while (p > 0 && iswspace(text_[p])) {
        p--;
    }
    // Skip word characters backwards
    while (p > 0 && !iswspace(text_[p - 1])) {
        p--;
    }
    return p;
}

size_t D2DEditBox::findWordEnd(size_t pos) const {
    if (pos >= text_.length()) {
        return text_.length();
    }
    size_t p = pos;
    // Skip whitespace forwards
    while (p < text_.length() && iswspace(text_[p])) {
        p++;
    }
    // Skip word characters forwards
    while (p < text_.length() && !iswspace(text_[p])) {
        p++;
    }
    return p;
}

void D2DEditBox::moveCaretToWord(bool forward, bool extend_selection) {
    if (forward) {
        caret_pos_ = findWordEnd(caret_pos_);
    } else {
        caret_pos_ = findWordStart(caret_pos_);
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
    if (!enabled_) {
        return false;
    }

    // Right-click: show context menu
    if (event.button == MouseButton::Right) {
        if (parent_) {
            parent_->requestFocus(this);
        }
        showContextMenu(event.screenPosition);
        return true;
    }

    if (event.button != MouseButton::Left) {
        return false;
    }

    // Request focus
    if (parent_) {
        parent_->requestFocus(this);
    }

    size_t pos = hitTestPosition(event.position.x);

    if (event.clickCount == 3) {
        // Triple-click: select all
        selectAll();
        drag_mode_ = DragMode::Character;
        dragging_ = false;
        return true;
    }

    if (event.clickCount == 2) {
        // Double-click: select word
        size_t word_start = findWordStart(pos);
        size_t word_end = findWordEnd(pos);
        // For double-click on whitespace, select at least the clicked position
        if (word_start == word_end) {
            word_start = pos;
            word_end = pos;
        }
        selection_start_ = word_start;
        selection_end_ = word_end;
        caret_pos_ = word_end;
        word_anchor_start_ = word_start;
        word_anchor_end_ = word_end;
        drag_mode_ = DragMode::Word;
        dragging_ = true;
        ensureCaretVisible();
        invalidate();
        return true;
    }

    // Single click
    drag_mode_ = DragMode::Character;

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

    if (drag_mode_ == DragMode::Word) {
        // Snap to word boundaries, keeping the anchor word as minimum selection
        size_t drag_word_start = findWordStart(pos);
        size_t drag_word_end = findWordEnd(pos);
        if (pos < word_anchor_start_) {
            // Dragging left of anchor
            selection_start_ = word_anchor_end_;
            selection_end_ = drag_word_start;
            caret_pos_ = drag_word_start;
        } else {
            // Dragging right of anchor (or within anchor)
            selection_start_ = word_anchor_start_;
            selection_end_ = drag_word_end;
            caret_pos_ = drag_word_end;
        }
    } else {
        selection_end_ = pos;
        caret_pos_ = pos;
    }

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
        if (ctrl) {
            deleteWord(false);
        } else {
            deleteCharacter(false);
        }
        return true;

    case VK_DELETE:
        if (ctrl) {
            deleteWord(true);
        } else {
            deleteCharacter(true);
        }
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

    case 'Z':
        if (ctrl) {
            undo();
            return true;
        }
        break;

    case 'Y':
        if (ctrl) {
            redo();
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

void D2DEditBox::showContextMenu(const Point& screen_pos) {
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        return;
    }

    enum { kCmdCut = 1, kCmdCopy, kCmdPaste, kCmdSelectAll };

    bool has_sel = hasSelection();
    bool has_text = !text_.empty();
    bool can_paste = IsClipboardFormatAvailable(CF_UNICODETEXT);

    AppendMenuW(menu, MF_STRING | ((!read_only_ && has_sel) ? 0 : MF_GRAYED), kCmdCut, L"Cut");
    AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), kCmdCopy, L"Copy");
    AppendMenuW(menu, MF_STRING | ((!read_only_ && can_paste) ? 0 : MF_GRAYED), kCmdPaste,
                L"Paste");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (has_text ? 0 : MF_GRAYED), kCmdSelectAll, L"Select All");

    // Find the HWND for TrackPopupMenu
    POINT pt = {static_cast<LONG>(screen_pos.x), static_cast<LONG>(screen_pos.y)};
    HWND hwnd = WindowFromPoint(pt);

    int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);

    switch (cmd) {
    case kCmdCut:
        cutToClipboard();
        break;
    case kCmdCopy:
        copyToClipboard();
        break;
    case kCmdPaste:
        pasteFromClipboard();
        break;
    case kCmdSelectAll:
        selectAll();
        break;
    }
}

HCURSOR D2DEditBox::cursor() const {
    static HCURSOR ibeam = LoadCursorW(nullptr, IDC_IBEAM);
    return ibeam;
}

void D2DEditBox::onFocusChanged(const FocusEvent& event) {
    if (event.gained) {
        resetCaretBlink();
    }
    invalidate();
}

}  // namespace nive::ui::d2d
