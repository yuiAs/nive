/// @file combobox.cpp
/// @brief Implementation of D2D combo box component

#include "combobox.hpp"

#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

const std::wstring D2DComboBox::kEmptyString;

D2DComboBox::D2DComboBox() = default;

void D2DComboBox::addItem(const std::wstring& text) {
    items_.push_back(text);
}

void D2DComboBox::addItems(const std::vector<std::wstring>& items) {
    items_.insert(items_.end(), items.begin(), items.end());
}

void D2DComboBox::clearItems() {
    items_.clear();
    selected_index_ = -1;
    hovered_item_ = -1;
    text_layout_.Reset();
}

const std::wstring& D2DComboBox::itemAt(size_t index) const {
    if (index < items_.size()) {
        return items_[index];
    }
    return kEmptyString;
}

void D2DComboBox::setSelectedIndex(int index) {
    if (index < -1)
        index = -1;
    if (index >= static_cast<int>(items_.size()))
        index = static_cast<int>(items_.size()) - 1;

    if (selected_index_ != index) {
        selected_index_ = index;
        updateTextLayout();
        invalidate();

        if (on_selection_changed_) {
            on_selection_changed_(selected_index_);
        }
    }
}

std::wstring D2DComboBox::selectedText() const {
    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(items_.size())) {
        return items_[selected_index_];
    }
    return {};
}

void D2DComboBox::openDropdown() {
    if (!dropdown_open_ && !items_.empty()) {
        dropdown_open_ = true;
        hovered_item_ = selected_index_;
        invalidate();
    }
}

void D2DComboBox::closeDropdown() {
    if (dropdown_open_) {
        dropdown_open_ = false;
        hovered_item_ = -1;
        invalidate();
    }
}

void D2DComboBox::toggleDropdown() {
    if (dropdown_open_) {
        closeDropdown();
    } else {
        openDropdown();
    }
}

Rect D2DComboBox::dropdownRect() const {
    if (items_.empty()) {
        return {};
    }

    float item_height = Style::itemHeight();
    float total_height = item_height * static_cast<float>(items_.size());
    float max_height = Style::maxDropdownHeight();
    float dropdown_height = std::min(total_height, max_height);

    return Rect{bounds_.x, bounds_.y + bounds_.height, bounds_.width, dropdown_height};
}

int D2DComboBox::hitTestDropdownItem(const Point& point) const {
    if (!dropdown_open_ || items_.empty()) {
        return -1;
    }

    Rect dropdown = dropdownRect();
    // Convert point to local dropdown coordinates
    float local_y = point.y - dropdown.y;

    if (local_y < 0 || local_y >= dropdown.height) {
        return -1;
    }

    int index = static_cast<int>(local_y / Style::itemHeight());
    if (index >= 0 && index < static_cast<int>(items_.size())) {
        return index;
    }
    return -1;
}

bool D2DComboBox::selectDropdownItem(const Point& point) {
    int index = hitTestDropdownItem(point);
    if (index >= 0) {
        setSelectedIndex(index);
        closeDropdown();
        return true;
    }
    return false;
}

void D2DComboBox::setHoveredItem(int index) {
    if (hovered_item_ != index) {
        hovered_item_ = index;
        invalidate();
    }
}

Size D2DComboBox::measure(const Size& available_size) {
    // ComboBox has a fixed height, width is flexible
    float width = available_size.width > 0 ? available_size.width : 150.0f;
    float height = Style::height();

    desired_size_ = Size{width, height};
    return desired_size_;
}

void D2DComboBox::render(ID2D1RenderTarget* rt) {
    if (!visible_ || bounds_.width <= 0 || bounds_.height <= 0) {
        return;
    }

    // Determine colors based on state
    Color bg_color, border_color, text_color, arrow_color;

    if (!enabled_) {
        bg_color = Style::disabledBackground();
        border_color = Style::disabledBorder();
        text_color = Style::disabledTextColor();
        arrow_color = Style::disabledTextColor();
    } else if (focused_ || dropdown_open_) {
        bg_color = Style::background();
        border_color = Style::focusedBorder();
        text_color = Style::textColor();
        arrow_color = Style::arrowColor();
    } else if (hovered_) {
        bg_color = Style::background();
        border_color = Style::hoverBorder();
        text_color = Style::textColor();
        arrow_color = Style::arrowColor();
    } else {
        bg_color = Style::background();
        border_color = Style::border();
        text_color = Style::textColor();
        arrow_color = Style::arrowColor();
    }

    // Update brushes
    background_brush_->SetColor(bg_color.toD2D());
    border_brush_->SetColor(border_color.toD2D());
    text_brush_->SetColor(text_color.toD2D());
    arrow_brush_->SetColor(arrow_color.toD2D());

    float border_width = Style::borderWidth();
    float radius = Style::borderRadius();

    D2D1_ROUNDED_RECT rounded_rect = {
        D2D1::RectF(bounds_.x, bounds_.y, bounds_.x + bounds_.width, bounds_.y + bounds_.height),
        radius, radius};

    // Draw background
    rt->FillRoundedRectangle(rounded_rect, background_brush_.Get());

    // Draw border
    rt->DrawRoundedRectangle(rounded_rect, border_brush_.Get(), border_width);

    // Calculate text area (excluding arrow)
    float arrow_width = Style::arrowWidth();
    Thickness padding = Style::padding();

    float text_x = bounds_.x + padding.left;
    // Note: text_layout_ uses PARAGRAPH_ALIGNMENT_CENTER with full component height,
    // so we draw at bounds_.y without padding offset
    float text_y = bounds_.y;

    // Draw selected text
    if (text_layout_) {
        rt->DrawTextLayout(D2D1::Point2F(text_x, text_y), text_layout_.Get(), text_brush_.Get());
    }

    // Draw arrow
    Rect arrow_rect{bounds_.x + bounds_.width - arrow_width, bounds_.y, arrow_width,
                    bounds_.height};
    renderArrow(rt, arrow_rect);
}

void D2DComboBox::renderDropdown(ID2D1RenderTarget* rt) {
    if (!dropdown_open_ || items_.empty()) {
        return;
    }

    Rect dropdown = dropdownRect();
    float item_height = Style::itemHeight();
    float border_width = Style::borderWidth();
    Thickness padding = Style::padding();

    // Draw dropdown background
    D2D1_RECT_F dropdown_d2d = D2D1::RectF(dropdown.x, dropdown.y, dropdown.x + dropdown.width,
                                           dropdown.y + dropdown.height);

    rt->FillRectangle(dropdown_d2d, dropdown_background_brush_.Get());
    rt->DrawRectangle(dropdown_d2d, dropdown_border_brush_.Get(), border_width);

    // Draw items
    float y = dropdown.y;
    for (size_t i = 0; i < items_.size(); ++i) {
        D2D1_RECT_F item_rect =
            D2D1::RectF(dropdown.x, y, dropdown.x + dropdown.width, y + item_height);

        // Draw item background
        if (static_cast<int>(i) == selected_index_) {
            rt->FillRectangle(item_rect, selected_item_brush_.Get());
        } else if (static_cast<int>(i) == hovered_item_) {
            rt->FillRectangle(item_rect, item_hover_brush_.Get());
        }

        // Draw item text
        ComPtr<IDWriteTextLayout> item_layout;
        D2DFactory::instance().dwriteFactory()->CreateTextLayout(
            items_[i].c_str(), static_cast<UINT32>(items_[i].length()), text_format_.Get(),
            dropdown.width - padding.left - padding.right, item_height, &item_layout);

        if (item_layout) {
            ID2D1SolidColorBrush* brush = (static_cast<int>(i) == selected_index_)
                                              ? selected_item_text_brush_.Get()
                                              : text_brush_.Get();

            rt->DrawTextLayout(D2D1::Point2F(dropdown.x + padding.left, y), item_layout.Get(),
                               brush);
        }

        y += item_height;

        // Stop if we exceed dropdown height
        if (y >= dropdown.y + dropdown.height) {
            break;
        }
    }
}

void D2DComboBox::renderArrow(ID2D1RenderTarget* rt, const Rect& arrow_rect) {
    // Draw a downward-pointing chevron
    float center_x = arrow_rect.x + arrow_rect.width / 2.0f;
    float center_y = arrow_rect.y + arrow_rect.height / 2.0f;
    float arrow_size = 4.0f;

    ComPtr<ID2D1PathGeometry> path;
    D2DFactory::instance().d2dFactory()->CreatePathGeometry(&path);

    if (path) {
        ComPtr<ID2D1GeometrySink> sink;
        path->Open(&sink);

        if (sink) {
            sink->BeginFigure(D2D1::Point2F(center_x - arrow_size, center_y - arrow_size / 2.0f),
                              D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(D2D1::Point2F(center_x, center_y + arrow_size / 2.0f));
            sink->AddLine(D2D1::Point2F(center_x + arrow_size, center_y - arrow_size / 2.0f));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();

            rt->DrawGeometry(path.Get(), arrow_brush_.Get(), 1.5f);
        }
    }
}

bool D2DComboBox::onMouseEnter(const MouseEvent& event) {
    hovered_ = true;
    invalidate();
    return true;
}

bool D2DComboBox::onMouseLeave(const MouseEvent& event) {
    hovered_ = false;
    invalidate();
    return true;
}

bool D2DComboBox::onMouseDown(const MouseEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (event.button == MouseButton::Left) {
        toggleDropdown();
        return true;
    }
    return false;
}

bool D2DComboBox::onKeyDown(const KeyEvent& event) {
    if (!enabled_) {
        return false;
    }

    if (dropdown_open_) {
        switch (event.keyCode) {
        case VK_UP:
            if (hovered_item_ > 0) {
                setHoveredItem(hovered_item_ - 1);
            } else if (hovered_item_ < 0 && !items_.empty()) {
                setHoveredItem(static_cast<int>(items_.size()) - 1);
            }
            return true;

        case VK_DOWN:
            if (hovered_item_ < static_cast<int>(items_.size()) - 1) {
                setHoveredItem(hovered_item_ + 1);
            } else if (hovered_item_ < 0 && !items_.empty()) {
                setHoveredItem(0);
            }
            return true;

        case VK_RETURN:
        case VK_SPACE:
            if (hovered_item_ >= 0) {
                setSelectedIndex(hovered_item_);
            }
            closeDropdown();
            return true;

        case VK_ESCAPE:
            closeDropdown();
            return true;

        case VK_HOME:
            if (!items_.empty()) {
                setHoveredItem(0);
            }
            return true;

        case VK_END:
            if (!items_.empty()) {
                setHoveredItem(static_cast<int>(items_.size()) - 1);
            }
            return true;
        }
    } else {
        // Dropdown closed
        switch (event.keyCode) {
        case VK_SPACE:
        case VK_DOWN:
        case VK_F4:
            openDropdown();
            return true;

        case VK_UP:
            if (selected_index_ > 0) {
                setSelectedIndex(selected_index_ - 1);
            }
            return true;
        }
    }

    return false;
}

void D2DComboBox::onFocusChanged(const FocusEvent& event) {
    if (!event.gained && dropdown_open_) {
        closeDropdown();
    }
    invalidate();
}

void D2DComboBox::createResources(DeviceResources& resources) {
    // Create text format
    D2DFactory::instance().dwriteFactory()->CreateTextFormat(
        Style::fontFamily(), nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL, Style::fontSize(), L"", &text_format_);

    if (text_format_) {
        text_format_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
        text_format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }

    // Create brushes
    background_brush_ = resources.createSolidBrush(Style::background());
    border_brush_ = resources.createSolidBrush(Style::border());
    text_brush_ = resources.createSolidBrush(Style::textColor());
    arrow_brush_ = resources.createSolidBrush(Style::arrowColor());
    dropdown_background_brush_ = resources.createSolidBrush(Style::dropdownBackground());
    dropdown_border_brush_ = resources.createSolidBrush(Style::dropdownBorder());
    item_hover_brush_ = resources.createSolidBrush(Style::itemHoverBackground());
    selected_item_brush_ = resources.createSolidBrush(Style::selectedItemBackground());
    selected_item_text_brush_ = resources.createSolidBrush(Style::selectedItemTextColor());

    updateTextLayout();
}

void D2DComboBox::updateTextLayout() {
    text_layout_.Reset();

    if (!text_format_) {
        return;
    }

    std::wstring display_text = selectedText();
    if (!display_text.empty()) {
        Thickness padding = Style::padding();
        float layout_width = bounds_.width - padding.left - padding.right - Style::arrowWidth();
        if (layout_width <= 0)
            layout_width = 100.0f;

        D2DFactory::instance().dwriteFactory()->CreateTextLayout(
            display_text.c_str(), static_cast<UINT32>(display_text.length()), text_format_.Get(),
            layout_width, Style::height(), &text_layout_);
    }
}

}  // namespace nive::ui::d2d
