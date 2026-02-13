/// @file container.hpp
/// @brief Container policies for D2D UI components

#pragma once

#include "component.hpp"

namespace nive::ui::d2d {

/// @brief Layout policy for containers
///
/// Determines how children are arranged within a container.
enum class LayoutPolicy {
    Manual,      // Children are positioned manually
    Vertical,    // Children are stacked vertically
    Horizontal,  // Children are stacked horizontally
};

/// @brief Vertical layout container
///
/// Arranges children vertically with configurable spacing.
class D2DVerticalLayout : public D2DContainerComponent {
public:
    D2DVerticalLayout() = default;

    [[nodiscard]] float spacing() const noexcept { return spacing_; }
    void setSpacing(float spacing) {
        spacing_ = spacing;
        invalidate();
    }

    Size measure(const Size& available_size) override {
        float total_height = 0.0f;
        float max_width = 0.0f;

        for (size_t i = 0; i < children_.size(); ++i) {
            Size child_size = children_[i]->measure(available_size);
            max_width = std::max(max_width, child_size.width);
            total_height += child_size.height;
            if (i > 0) {
                total_height += spacing_;
            }
        }

        desired_size_ = {max_width, total_height};
        return desired_size_;
    }

    void arrange(const Rect& bounds) override {
        D2DContainerComponent::arrange(bounds);

        float y = bounds.y;
        for (auto& child : children_) {
            Rect child_bounds{bounds.x, y, bounds.width, child->desiredSize().height};
            child->arrange(child_bounds);
            y += child->desiredSize().height + spacing_;
        }
    }

private:
    float spacing_ = 8.0f;
};

/// @brief Horizontal layout container
///
/// Arranges children horizontally with configurable spacing.
class D2DHorizontalLayout : public D2DContainerComponent {
public:
    D2DHorizontalLayout() = default;

    [[nodiscard]] float spacing() const noexcept { return spacing_; }
    void setSpacing(float spacing) {
        spacing_ = spacing;
        invalidate();
    }

    Size measure(const Size& available_size) override {
        float total_width = 0.0f;
        float max_height = 0.0f;

        for (size_t i = 0; i < children_.size(); ++i) {
            Size child_size = children_[i]->measure(available_size);
            max_height = std::max(max_height, child_size.height);
            total_width += child_size.width;
            if (i > 0) {
                total_width += spacing_;
            }
        }

        desired_size_ = {total_width, max_height};
        return desired_size_;
    }

    void arrange(const Rect& bounds) override {
        D2DContainerComponent::arrange(bounds);

        float x = bounds.x;
        for (auto& child : children_) {
            Rect child_bounds{x, bounds.y, child->desiredSize().width, bounds.height};
            child->arrange(child_bounds);
            x += child->desiredSize().width + spacing_;
        }
    }

private:
    float spacing_ = 8.0f;
};

}  // namespace nive::ui::d2d
