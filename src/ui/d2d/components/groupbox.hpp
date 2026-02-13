/// @file groupbox.hpp
/// @brief D2D group box container component

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <string>

#include "ui/d2d/base/component.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/styles/default_style.hpp"

namespace nive::ui::d2d {

/// @brief Group box container with a titled border
///
/// Renders a border with a title text in the top-left corner.
/// Can contain child components.
class D2DGroupBox : public D2DContainerComponent {
public:
    using Style = StyleTraits<D2DGroupBox>;

    D2DGroupBox();
    explicit D2DGroupBox(const std::wstring& title);
    ~D2DGroupBox() override = default;

    // Properties

    [[nodiscard]] const std::wstring& title() const noexcept { return title_; }
    void setTitle(const std::wstring& title);

    // D2DUIComponent interface
    Size measure(const Size& available_size) override;
    void render(ID2D1RenderTarget* rt) override;

    /// @brief Get the content area bounds (inside padding)
    [[nodiscard]] Rect contentArea() const noexcept;

    /// @brief Create resources (call before first render)
    void createResources(DeviceResources& resources);

private:
    std::wstring title_;

    // DirectWrite resources
    ComPtr<IDWriteTextFormat> text_format_;
    ComPtr<IDWriteTextLayout> text_layout_;

    // Brushes
    ComPtr<ID2D1SolidColorBrush> border_brush_;
    ComPtr<ID2D1SolidColorBrush> text_brush_;
    ComPtr<ID2D1SolidColorBrush> label_bg_brush_;

    float title_height_ = 0.0f;
    float title_width_ = 0.0f;
};

}  // namespace nive::ui::d2d
