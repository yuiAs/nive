/// @file component.cpp
/// @brief Implementation of D2D UI component base classes

#include "component.hpp"

namespace nive::ui::d2d {

// D2DUIComponent implementation

D2DUIComponent::D2DUIComponent() = default;

void D2DUIComponent::arrange(const Rect& bounds) {
    bounds_ = bounds;
}

void D2DUIComponent::setVisible(bool visible) {
    if (visible_ != visible) {
        visible_ = visible;
        invalidate();
    }
}

void D2DUIComponent::setEnabled(bool enabled) {
    if (enabled_ != enabled) {
        enabled_ = enabled;
        // Clear focus when disabled
        if (!enabled_ && focused_) {
            setFocused(false);
        }
        invalidate();
    }
}

void D2DUIComponent::setFocused(bool focused) {
    if (focused_ != focused) {
        focused_ = focused;
        FocusEvent event;
        event.gained = focused;
        onFocusChanged(event);
        invalidate();
    }
}

void D2DUIComponent::setMargin(const Thickness& margin) {
    if (!(margin_ == margin)) {
        margin_ = margin;
        invalidate();
    }
}

void D2DUIComponent::setPadding(const Thickness& padding) {
    if (!(padding_ == padding)) {
        padding_ = padding;
        invalidate();
    }
}

bool D2DUIComponent::hitTest(const Point& point) const {
    return visible_ && bounds_.contains(point);
}

void D2DUIComponent::invalidate() {
    onInvalidate();
    if (parent_) {
        parent_->invalidate();
    }
}

Rect D2DUIComponent::contentBounds() const noexcept {
    return Rect{bounds_.x + padding_.left, bounds_.y + padding_.top,
                bounds_.width - padding_.left - padding_.right,
                bounds_.height - padding_.top - padding_.bottom};
}

// D2DContainerComponent implementation

D2DContainerComponent::~D2DContainerComponent() = default;

void D2DContainerComponent::addChild(std::unique_ptr<D2DUIComponent> child) {
    if (child) {
        child->parent_ = this;
        children_.push_back(std::move(child));
        invalidate();
    }
}

std::unique_ptr<D2DUIComponent> D2DContainerComponent::removeChild(D2DUIComponent* child) {
    for (auto it = children_.begin(); it != children_.end(); ++it) {
        if (it->get() == child) {
            if (hovered_child_ == child) {
                hovered_child_ = nullptr;
            }
            if (focused_child_ == child) {
                focused_child_ = nullptr;
            }
            child->parent_ = nullptr;
            auto result = std::move(*it);
            children_.erase(it);
            invalidate();
            return result;
        }
    }
    return nullptr;
}

D2DUIComponent* D2DContainerComponent::childAt(size_t index) const {
    if (index < children_.size()) {
        return children_[index].get();
    }
    return nullptr;
}

D2DUIComponent* D2DContainerComponent::findChildById(int id) const {
    for (const auto& child : children_) {
        if (child->id() == id) {
            return child.get();
        }
        // Recursive search in containers
        if (auto container = dynamic_cast<D2DContainerComponent*>(child.get())) {
            if (auto found = container->findChildById(id)) {
                return found;
            }
        }
    }
    return nullptr;
}

D2DUIComponent* D2DContainerComponent::findComponentAt(const Point& point) {
    if (!hitTest(point)) {
        return nullptr;
    }

    // Check children in reverse order (top-most first)
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        auto& child = *it;
        if (!child->isVisible()) {
            continue;
        }

        // Transform point to child coordinates
        Point child_point{point.x - child->bounds().x, point.y - child->bounds().y};

        if (auto container = dynamic_cast<D2DContainerComponent*>(child.get())) {
            if (auto found = container->findComponentAt(child_point)) {
                return found;
            }
        } else if (child->hitTest(point)) {
            return child.get();
        }
    }

    return this;
}

void D2DContainerComponent::render(ID2D1RenderTarget* rt) {
    if (!visible_) {
        return;
    }
    renderChildren(rt);
}

void D2DContainerComponent::renderChildren(ID2D1RenderTarget* rt) {
    for (const auto& child : children_) {
        if (child->isVisible()) {
            child->render(rt);
        }
    }
}

MouseEvent D2DContainerComponent::transformEventForChild(const MouseEvent& event,
                                                         D2DUIComponent* child) const {
    // Transform from this container's local coordinates to child's local coordinates
    // Child's position relative to this container = child->bounds().xy - this->bounds_.xy
    MouseEvent transformed = event;
    transformed.position.x -= (child->bounds().x - bounds_.x);
    transformed.position.y -= (child->bounds().y - bounds_.y);
    return transformed;
}

bool D2DContainerComponent::onMouseMove(const MouseEvent& event) {
    // event.position is in this container's local coordinates
    // Convert to absolute for hitTest (children have absolute bounds)
    Point abs_pos{event.position.x + bounds_.x, event.position.y + bounds_.y};

    // Find child under mouse
    D2DUIComponent* child_under_mouse = nullptr;
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if ((*it)->isVisible() && (*it)->hitTest(abs_pos)) {
            child_under_mouse = it->get();
            break;
        }
    }

    // Handle hover state changes
    if (child_under_mouse != hovered_child_) {
        if (hovered_child_) {
            hovered_child_->hovered_ = false;
            hovered_child_->onMouseLeave(event);
        }
        hovered_child_ = child_under_mouse;
        if (hovered_child_) {
            hovered_child_->hovered_ = true;
            auto child_event = transformEventForChild(event, hovered_child_);
            hovered_child_->onMouseEnter(child_event);
        }
    }

    // Dispatch to hovered child
    if (hovered_child_) {
        auto child_event = transformEventForChild(event, hovered_child_);
        if (hovered_child_->onMouseMove(child_event)) {
            return true;
        }
    }

    return false;
}

bool D2DContainerComponent::onMouseDown(const MouseEvent& event) {
    // event.position is in this container's local coordinates
    // Convert to absolute for hitTest (children have absolute bounds)
    Point abs_pos{event.position.x + bounds_.x, event.position.y + bounds_.y};

    // Find child under mouse
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if ((*it)->isVisible() && (*it)->isEnabled() && (*it)->hitTest(abs_pos)) {
            auto child_event = transformEventForChild(event, it->get());
            if ((*it)->onMouseDown(child_event)) {
                return true;
            }
        }
    }
    return false;
}

bool D2DContainerComponent::onMouseUp(const MouseEvent& event) {
    // event.position is in this container's local coordinates
    // Convert to absolute for hitTest (children have absolute bounds)
    Point abs_pos{event.position.x + bounds_.x, event.position.y + bounds_.y};

    // Dispatch to child under mouse
    for (auto it = children_.rbegin(); it != children_.rend(); ++it) {
        if ((*it)->isVisible() && (*it)->isEnabled() && (*it)->hitTest(abs_pos)) {
            auto child_event = transformEventForChild(event, it->get());
            if ((*it)->onMouseUp(child_event)) {
                return true;
            }
        }
    }
    return false;
}

bool D2DContainerComponent::onMouseWheel(const MouseEvent& event) {
    // Dispatch to focused child first, then hovered child
    if (focused_child_ && focused_child_->isEnabled()) {
        auto child_event = transformEventForChild(event, focused_child_);
        if (focused_child_->onMouseWheel(child_event)) {
            return true;
        }
    }

    if (hovered_child_ && hovered_child_ != focused_child_ && hovered_child_->isEnabled()) {
        auto child_event = transformEventForChild(event, hovered_child_);
        if (hovered_child_->onMouseWheel(child_event)) {
            return true;
        }
    }

    return false;
}

void D2DContainerComponent::requestFocus(D2DUIComponent* child) {
    // Only skip if child is already tracked AND actually has focus
    // (focused_ might have been cleared externally, e.g., when disabled)
    if (focused_child_ == child && child && child->isFocused()) {
        return;
    }

    if (focused_child_) {
        focused_child_->setFocused(false);
    }

    focused_child_ = child;

    if (focused_child_) {
        focused_child_->setFocused(true);
    }

    // Bubble up: tell our parent that we (this container) now have focus
    // This ensures keyboard events can flow down through the hierarchy
    if (parent_) {
        parent_->requestFocus(this);
    }
}

bool D2DContainerComponent::onKeyDown(const KeyEvent& event) {
    // Forward to focused child
    if (focused_child_ && focused_child_->isEnabled()) {
        return focused_child_->onKeyDown(event);
    }
    return false;
}

bool D2DContainerComponent::onKeyUp(const KeyEvent& event) {
    // Forward to focused child
    if (focused_child_ && focused_child_->isEnabled()) {
        return focused_child_->onKeyUp(event);
    }
    return false;
}

bool D2DContainerComponent::onChar(const KeyEvent& event) {
    // Forward to focused child
    if (focused_child_ && focused_child_->isEnabled()) {
        return focused_child_->onChar(event);
    }
    return false;
}

}  // namespace nive::ui::d2d
