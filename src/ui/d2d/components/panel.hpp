/// @file panel.hpp
/// @brief D2D simple panel container component

#pragma once

#include "ui/d2d/base/component.hpp"

namespace nive::ui::d2d {

/// @brief Simple panel container without decoration
///
/// A basic container that holds child components without
/// any visual decoration. Children are positioned manually.
class D2DPanel : public D2DContainerComponent {
public:
    D2DPanel() = default;
    ~D2DPanel() override = default;

    Size measure(const Size& available_size) override {
        // Measure all children and return bounding size
        float max_right = 0.0f;
        float max_bottom = 0.0f;

        for (auto& child : children_) {
            Size child_size = child->measure(available_size);
            max_right = std::max(max_right, child_size.width);
            max_bottom = std::max(max_bottom, child_size.height);
        }

        desired_size_ = {max_right, max_bottom};
        return desired_size_;
    }
};

}  // namespace nive::ui::d2d
