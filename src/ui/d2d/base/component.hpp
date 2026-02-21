/// @file component.hpp
/// @brief Base class for D2D UI components

#pragma once

#include <d2d1.h>
#include <dwrite.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "event.hpp"
#include "ui/d2d/core/device_resources.hpp"
#include "ui/d2d/core/types.hpp"

namespace nive::ui::d2d {

class D2DContainerComponent;

/// @brief Base class for all D2D UI components
///
/// Components follow a measure/arrange/render lifecycle:
/// 1. measure() - Calculate desired size given constraints
/// 2. arrange() - Position within allocated bounds
/// 3. render() - Draw to render target
class D2DUIComponent {
public:
    virtual ~D2DUIComponent() = default;

    D2DUIComponent(const D2DUIComponent&) = delete;
    D2DUIComponent& operator=(const D2DUIComponent&) = delete;

    // Layout lifecycle

    /// @brief Measure the component's desired size
    /// @param available_size Available space (may be infinite)
    /// @return Desired size
    virtual Size measure(const Size& available_size) = 0;

    /// @brief Arrange the component within its allocated bounds
    /// @param bounds Rectangle to position within
    virtual void arrange(const Rect& bounds);

    /// @brief Render the component
    /// @param rt Render target
    virtual void render(ID2D1RenderTarget* rt) = 0;

    // Event handlers (return true if handled)

    virtual bool onMouseEnter(const MouseEvent& event) { return false; }
    virtual bool onMouseLeave(const MouseEvent& event) { return false; }
    virtual bool onMouseMove(const MouseEvent& event) { return false; }
    virtual bool onMouseDown(const MouseEvent& event) { return false; }
    virtual bool onMouseUp(const MouseEvent& event) { return false; }
    virtual bool onMouseWheel(const MouseEvent& event) { return false; }
    virtual bool onKeyDown(const KeyEvent& event) { return false; }
    virtual bool onKeyUp(const KeyEvent& event) { return false; }
    virtual bool onChar(const KeyEvent& event) { return false; }
    virtual void onFocusChanged(const FocusEvent& event) {}

    // Properties

    [[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }
    [[nodiscard]] const Size& desiredSize() const noexcept { return desired_size_; }

    [[nodiscard]] bool isVisible() const noexcept { return visible_; }
    void setVisible(bool visible);

    [[nodiscard]] bool isEnabled() const noexcept { return enabled_; }
    void setEnabled(bool enabled);

    [[nodiscard]] bool isFocusable() const noexcept { return focusable_; }
    void setFocusable(bool focusable) noexcept { focusable_ = focusable; }
    [[nodiscard]] bool canReceiveFocus() const noexcept {
        return focusable_ && visible_ && enabled_;
    }

    [[nodiscard]] bool isFocused() const noexcept { return focused_; }
    void setFocused(bool focused);

    [[nodiscard]] bool isHovered() const noexcept { return hovered_; }

    [[nodiscard]] int id() const noexcept { return id_; }
    void setId(int id) noexcept { id_ = id; }

    [[nodiscard]] D2DContainerComponent* parent() const noexcept { return parent_; }

    [[nodiscard]] const Thickness& margin() const noexcept { return margin_; }
    void setMargin(const Thickness& margin);

    [[nodiscard]] const Thickness& padding() const noexcept { return padding_; }
    void setPadding(const Thickness& padding);

    // Hit testing

    /// @brief Test if a point is within this component
    /// @param point Point in parent coordinates
    /// @return true if point is within bounds
    [[nodiscard]] virtual bool hitTest(const Point& point) const;

    /// @brief Invalidate the component (request repaint)
    void invalidate();

protected:
    D2DUIComponent();

    /// @brief Called when the component needs to invalidate itself
    /// Subclasses can override to add custom invalidation logic
    virtual void onInvalidate() {}

    /// @brief Get the content area (bounds minus padding)
    [[nodiscard]] Rect contentBounds() const noexcept;

    // Layout state
    Rect bounds_;
    Size desired_size_;
    Thickness margin_;
    Thickness padding_;

    // Component state
    bool visible_ = true;
    bool enabled_ = true;
    bool focused_ = false;
    bool hovered_ = false;
    bool focusable_ = true;

    // Identity
    int id_ = 0;
    D2DContainerComponent* parent_ = nullptr;

    friend class D2DContainerComponent;
};

/// @brief Base class for components that contain other components
class D2DContainerComponent : public D2DUIComponent {
public:
    ~D2DContainerComponent() override;

    /// @brief Add a child component
    /// @param child Component to add
    void addChild(std::unique_ptr<D2DUIComponent> child);

    /// @brief Remove a child component
    /// @param child Component to remove
    /// @return The removed component, or nullptr if not found
    std::unique_ptr<D2DUIComponent> removeChild(D2DUIComponent* child);

    /// @brief Get child count
    [[nodiscard]] size_t childCount() const noexcept { return children_.size(); }

    /// @brief Get child by index
    [[nodiscard]] D2DUIComponent* childAt(size_t index) const;

    /// @brief Find child by ID
    [[nodiscard]] D2DUIComponent* findChildById(int id) const;

    /// @brief Find component at point (recursive)
    /// @param point Point in this component's coordinates
    /// @return Component at point, or this if no child matches
    [[nodiscard]] D2DUIComponent* findComponentAt(const Point& point);

    // Layout

    void render(ID2D1RenderTarget* rt) override;

    // Event dispatch to children

    bool onMouseMove(const MouseEvent& event) override;
    bool onMouseDown(const MouseEvent& event) override;
    bool onMouseUp(const MouseEvent& event) override;
    bool onMouseWheel(const MouseEvent& event) override;
    bool onKeyDown(const KeyEvent& event) override;
    bool onKeyUp(const KeyEvent& event) override;
    bool onChar(const KeyEvent& event) override;

    /// @brief Request focus for a child component
    void requestFocus(D2DUIComponent* child);

    /// @brief Get the currently focused child (if any)
    [[nodiscard]] D2DUIComponent* focusedChild() const noexcept { return focused_child_; }

protected:
    D2DContainerComponent() { focusable_ = false; }

    /// @brief Set this container as the parent of a component
    ///
    /// Used for components that are managed separately from children_
    /// (e.g., tab content in TabControl).
    static void setComponentParent(D2DUIComponent* component, D2DContainerComponent* parent) {
        if (component) {
            component->parent_ = parent;
        }
    }

    /// @brief Render children
    void renderChildren(ID2D1RenderTarget* rt);

    /// @brief Transform event coordinates for child
    [[nodiscard]] MouseEvent transformEventForChild(const MouseEvent& event,
                                                    D2DUIComponent* child) const;

    std::vector<std::unique_ptr<D2DUIComponent>> children_;
    D2DUIComponent* hovered_child_ = nullptr;
    D2DUIComponent* focused_child_ = nullptr;
};

}  // namespace nive::ui::d2d
