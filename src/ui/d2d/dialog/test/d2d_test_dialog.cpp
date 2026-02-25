/// @file d2d_test_dialog.cpp
/// @brief Implementation of D2D test dialog

#ifdef NIVE_DEBUG_D2D_TEST

    #include "d2d_test_dialog.hpp"

    #include <format>

    #include "core/util/logger.hpp"
    #include "ui/d2d/components/panel.hpp"
    #include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

D2DTestDialog::D2DTestDialog() {
    setTitle(L"D2D UI Test Dialog");
    setInitialSize(Size{550.0f, 450.0f});
    setMinimumSize(Size{450.0f, 380.0f});
    setResizable(true);
}

void D2DTestDialog::onCreate() {
    border_brush_ = deviceResources().createSolidBrush(Color::gray());
    createComponents();
    layoutComponents();
}

void D2DTestDialog::createComponents() {
    // Create tab control
    auto tabs = std::make_unique<D2DTabControl>();
    tabs->createResources(deviceResources());
    tab_control_ = tabs.get();

    // Add tabs with content
    tabs->addTab(L"Basic", createBasicTab());
    tabs->addTab(L"Form", createFormTab());
    tabs->addTab(L"Input", createInputTab());
    tabs->setSelectedIndex(0);

    addChild(std::move(tabs));

    // Status label at bottom
    auto status = std::make_unique<D2DLabel>(L"Status: Ready");
    status->setTextAlignment(TextAlignment::Leading);
    status->createResources(deviceResources());
    status_label_ = status.get();
    addChild(std::move(status));

    // Close button (primary variant)
    auto close = std::make_unique<D2DButton>(L"Close");
    close->setVariant(ButtonVariant::Primary);
    close->createResources(deviceResources());
    close->onClick([this]() { endDialog(IDOK); });
    close_button_ = close.get();
    addChild(std::move(close));

    setDefaultButton(close_button_);
}

std::unique_ptr<D2DContainerComponent> D2DTestDialog::createBasicTab() {
    auto container = std::make_unique<D2DPanel>();

    // Title label
    auto title = std::make_unique<D2DLabel>(L"D2D UI Framework Test");
    title->setFontSize(18.0f);
    title->setTextAlignment(TextAlignment::Center);
    title->createResources(deviceResources());
    title_label_ = title.get();
    container->addChild(std::move(title));

    // Info label
    auto info = std::make_unique<D2DLabel>(L"Testing Label and Button components");
    info->setTextAlignment(TextAlignment::Center);
    info->createResources(deviceResources());
    info_label_ = info.get();
    container->addChild(std::move(info));

    // Counter label
    auto counter = std::make_unique<D2DLabel>(L"Counter: 0");
    counter->setFontSize(24.0f);
    counter->setTextAlignment(TextAlignment::Center);
    counter->setVerticalAlignment(VerticalAlignment::Center);
    counter->createResources(deviceResources());
    counter_label_ = counter.get();
    container->addChild(std::move(counter));

    // Increment button
    auto inc = std::make_unique<D2DButton>(L"+ Increment");
    inc->createResources(deviceResources());
    inc->onClick([this]() {
        counter_++;
        updateCounterLabel();
    });
    increment_button_ = inc.get();
    container->addChild(std::move(inc));

    // Decrement button
    auto dec = std::make_unique<D2DButton>(L"- Decrement");
    dec->createResources(deviceResources());
    dec->onClick([this]() {
        counter_--;
        updateCounterLabel();
    });
    decrement_button_ = dec.get();
    container->addChild(std::move(dec));

    // Reset button
    auto reset = std::make_unique<D2DButton>(L"Reset");
    reset->createResources(deviceResources());
    reset->onClick([this]() {
        counter_ = 0;
        updateCounterLabel();
    });
    reset_button_ = reset.get();
    container->addChild(std::move(reset));

    return container;
}

std::unique_ptr<D2DContainerComponent> D2DTestDialog::createFormTab() {
    auto container = std::make_unique<D2DPanel>();

    // CheckBox group
    auto cb_group = std::make_unique<D2DGroupBox>(L"CheckBoxes");
    cb_group->createResources(deviceResources());
    checkbox_group_ = cb_group.get();

    auto cb1 = std::make_unique<D2DCheckBox>(L"Option 1");
    cb1->createResources(deviceResources());
    cb1->onChange([this](bool checked) { updateStatusLabel(); });
    checkbox1_ = cb1.get();
    cb_group->addChild(std::move(cb1));

    auto cb2 = std::make_unique<D2DCheckBox>(L"Option 2 (checked)");
    cb2->setChecked(true);
    cb2->createResources(deviceResources());
    cb2->onChange([this](bool checked) { updateStatusLabel(); });
    checkbox2_ = cb2.get();
    cb_group->addChild(std::move(cb2));

    auto cb3 = std::make_unique<D2DCheckBox>(L"Option 3 (disabled)");
    cb3->setEnabled(false);
    cb3->createResources(deviceResources());
    checkbox3_ = cb3.get();
    cb_group->addChild(std::move(cb3));

    container->addChild(std::move(cb_group));

    // RadioButton group
    auto rb_group = std::make_unique<D2DGroupBox>(L"RadioButtons");
    rb_group->createResources(deviceResources());
    radio_group_ = rb_group.get();

    auto rb1 = std::make_unique<D2DRadioButton>(L"Choice A");
    rb1->setGroup(&radio_group_manager_);
    rb1->createResources(deviceResources());
    rb1->onChange([this](bool selected) {
        if (selected)
            updateStatusLabel();
    });
    radio1_ = rb1.get();
    rb_group->addChild(std::move(rb1));

    auto rb2 = std::make_unique<D2DRadioButton>(L"Choice B (selected)");
    rb2->setGroup(&radio_group_manager_);
    rb2->setSelected(true);
    rb2->createResources(deviceResources());
    rb2->onChange([this](bool selected) {
        if (selected)
            updateStatusLabel();
    });
    radio2_ = rb2.get();
    rb_group->addChild(std::move(rb2));

    auto rb3 = std::make_unique<D2DRadioButton>(L"Choice C");
    rb3->setGroup(&radio_group_manager_);
    rb3->createResources(deviceResources());
    rb3->onChange([this](bool selected) {
        if (selected)
            updateStatusLabel();
    });
    radio3_ = rb3.get();
    rb_group->addChild(std::move(rb3));

    container->addChild(std::move(rb_group));

    return container;
}

std::unique_ptr<D2DContainerComponent> D2DTestDialog::createInputTab() {
    auto container = std::make_unique<D2DPanel>();

    // EditBox
    auto edit_lbl = std::make_unique<D2DLabel>(L"EditBox:");
    edit_lbl->createResources(deviceResources());
    edit_label_ = edit_lbl.get();
    container->addChild(std::move(edit_lbl));

    auto edit = std::make_unique<D2DEditBox>();
    edit->setPlaceholder(L"Enter text here...");
    edit->createResources(deviceResources());
    edit->onChange([this](const std::wstring& text) { updateStatusLabel(); });
    edit_box_ = edit.get();
    container->addChild(std::move(edit));

    // SpinEdit
    auto spin_lbl = std::make_unique<D2DLabel>(L"SpinEdit (0-100):");
    spin_lbl->createResources(deviceResources());
    spin_label_ = spin_lbl.get();
    container->addChild(std::move(spin_lbl));

    auto spin = std::make_unique<D2DSpinEdit>();
    spin->setRange(0, 100);
    spin->setValue(50);
    spin->setStep(5);
    spin->createResources(deviceResources());
    spin->onChange([this](int value) { updateStatusLabel(); });
    spin_edit_ = spin.get();
    container->addChild(std::move(spin));

    // Result display
    auto result = std::make_unique<D2DLabel>(L"Edit the values above to see changes");
    result->setWordWrap(true);
    result->createResources(deviceResources());
    result_label_ = result.get();
    container->addChild(std::move(result));

    return container;
}

void D2DTestDialog::layoutComponents() {
    // Components may not be created yet (WM_SIZE during window creation)
    if (!tab_control_) {
        return;
    }

    auto size = deviceResources().getSize();
    float padding = 15.0f;
    float button_height = 28.0f;
    float status_height = 20.0f;

    // Close button at bottom right
    float close_width = 80.0f;
    float close_x = size.width - padding - close_width;
    float close_y = size.height - padding - button_height;
    close_button_->arrange(Rect{close_x, close_y, close_width, button_height});

    // Status label at bottom left (next to close button)
    float status_y = close_y + (button_height - status_height) / 2.0f;
    status_label_->arrange(Rect{padding, status_y, close_x - padding * 2, status_height});

    // Tab control fills the rest
    float tab_height = close_y - padding * 2;
    tab_control_->arrange(Rect{padding, padding, size.width - padding * 2, tab_height});

    // Layout tab content based on current selection
    layoutBasicTabContent();
    layoutFormTabContent();
    layoutInputTabContent();
}

void D2DTestDialog::layoutBasicTabContent() {
    if (!title_label_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float spacing = 10.0f;

    // Title at top
    float title_height = 30.0f;
    title_label_->arrange(
        Rect{content.x + padding, content.y + padding, content.width - padding * 2, title_height});

    // Info below title
    float info_y = content.y + padding + title_height + spacing;
    float info_height = 20.0f;
    info_label_->arrange(
        Rect{content.x + padding, info_y, content.width - padding * 2, info_height});

    // Counter in the middle
    float counter_y = info_y + info_height + spacing * 2;
    float counter_height = 40.0f;
    counter_label_->arrange(
        Rect{content.x + padding, counter_y, content.width - padding * 2, counter_height});

    // Buttons below counter
    float button_y = counter_y + counter_height + spacing * 2;
    float button_width = 100.0f;
    float button_height = 28.0f;
    float total_buttons_width = button_width * 3 + spacing * 2;
    float button_start_x = content.x + (content.width - total_buttons_width) / 2.0f;

    increment_button_->arrange(Rect{button_start_x, button_y, button_width, button_height});
    decrement_button_->arrange(
        Rect{button_start_x + button_width + spacing, button_y, button_width, button_height});
    reset_button_->arrange(
        Rect{button_start_x + (button_width + spacing) * 2, button_y, button_width, button_height});
}

void D2DTestDialog::layoutFormTabContent() {
    if (!checkbox_group_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float spacing = 15.0f;

    // Checkbox group on left
    float group_width = (content.width - padding * 2 - spacing) / 2.0f;
    float group_height = content.height - padding * 2;
    checkbox_group_->arrange(
        Rect{content.x + padding, content.y + padding, group_width, group_height});

    // Layout checkboxes inside group
    Rect cb_content = checkbox_group_->contentArea();
    float item_height = 24.0f;
    float item_spacing = 8.0f;
    float item_y = cb_content.y;
    checkbox1_->arrange(Rect{cb_content.x, item_y, cb_content.width, item_height});
    item_y += item_height + item_spacing;
    checkbox2_->arrange(Rect{cb_content.x, item_y, cb_content.width, item_height});
    item_y += item_height + item_spacing;
    checkbox3_->arrange(Rect{cb_content.x, item_y, cb_content.width, item_height});

    // Radio group on right
    float radio_x = content.x + padding + group_width + spacing;
    radio_group_->arrange(Rect{radio_x, content.y + padding, group_width, group_height});

    // Layout radio buttons inside group
    Rect rb_content = radio_group_->contentArea();
    item_y = rb_content.y;
    radio1_->arrange(Rect{rb_content.x, item_y, rb_content.width, item_height});
    item_y += item_height + item_spacing;
    radio2_->arrange(Rect{rb_content.x, item_y, rb_content.width, item_height});
    item_y += item_height + item_spacing;
    radio3_->arrange(Rect{rb_content.x, item_y, rb_content.width, item_height});
}

void D2DTestDialog::layoutInputTabContent() {
    if (!edit_box_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float spacing = 10.0f;
    float label_height = 20.0f;
    float input_height = 28.0f;

    float y = content.y + padding;

    // EditBox label
    edit_label_->arrange(Rect{content.x + padding, y, content.width - padding * 2, label_height});
    y += label_height + spacing / 2;

    // EditBox
    edit_box_->arrange(Rect{content.x + padding, y, content.width - padding * 2, input_height});
    y += input_height + spacing * 2;

    // SpinEdit label
    spin_label_->arrange(Rect{content.x + padding, y, content.width - padding * 2, label_height});
    y += label_height + spacing / 2;

    // SpinEdit
    float spin_width = 150.0f;
    spin_edit_->arrange(Rect{content.x + padding, y, spin_width, input_height});
    y += input_height + spacing * 2;

    // Result label
    float result_height = content.height - (y - content.y) - padding;
    result_label_->arrange(
        Rect{content.x + padding, y, content.width - padding * 2, result_height});
}

void D2DTestDialog::updateCounterLabel() {
    counter_label_->setText(std::format(L"Counter: {}", counter_));
    status_label_->setText(std::format(L"Counter changed to {}", counter_));
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void D2DTestDialog::updateStatusLabel() {
    std::wstring status;

    if (tab_control_->selectedIndex() == 1) {
        // Form tab
        int checked_count = 0;
        if (checkbox1_ && checkbox1_->isChecked())
            checked_count++;
        if (checkbox2_ && checkbox2_->isChecked())
            checked_count++;
        if (checkbox3_ && checkbox3_->isChecked())
            checked_count++;

        std::wstring selected_radio = L"None";
        if (radio1_ && radio1_->isSelected())
            selected_radio = L"A";
        else if (radio2_ && radio2_->isSelected())
            selected_radio = L"B";
        else if (radio3_ && radio3_->isSelected())
            selected_radio = L"C";

        status = std::format(L"Checkboxes: {} checked | Radio: Choice {}", checked_count,
                             selected_radio);
    } else if (tab_control_->selectedIndex() == 2) {
        // Input tab
        std::wstring text = edit_box_ ? edit_box_->text() : L"";
        int spin_val = spin_edit_ ? spin_edit_->value() : 0;
        status =
            std::format(L"Text: \"{}\" | SpinEdit: {}", text.empty() ? L"(empty)" : text, spin_val);

        if (result_label_) {
            result_label_->setText(std::format(L"EditBox text: \"{}\"\nSpinEdit value: {}",
                                               text.empty() ? L"(empty)" : text, spin_val));
        }
    } else {
        status = L"Status: Ready";
    }

    status_label_->setText(status);
    InvalidateRect(hwnd(), nullptr, FALSE);
}

void D2DTestDialog::onRender(ID2D1RenderTarget* rt) {
    // Clear with light gray background
    rt->Clear(D2D1::ColorF(0xF5F5F5));

    // Recreate brushes for interactive components
    if (tab_control_)
        tab_control_->createResources(deviceResources());
    if (increment_button_)
        increment_button_->createResources(deviceResources());
    if (decrement_button_)
        decrement_button_->createResources(deviceResources());
    if (reset_button_)
        reset_button_->createResources(deviceResources());
    if (close_button_)
        close_button_->createResources(deviceResources());
    if (checkbox1_)
        checkbox1_->createResources(deviceResources());
    if (checkbox2_)
        checkbox2_->createResources(deviceResources());
    if (checkbox3_)
        checkbox3_->createResources(deviceResources());
    if (radio1_)
        radio1_->createResources(deviceResources());
    if (radio2_)
        radio2_->createResources(deviceResources());
    if (radio3_)
        radio3_->createResources(deviceResources());
    if (edit_box_)
        edit_box_->createResources(deviceResources());
    if (spin_edit_)
        spin_edit_->createResources(deviceResources());

    // Render all child components
    D2DDialog::onRender(rt);
}

void D2DTestDialog::onResize(float width, float height) {
    D2DDialog::onResize(width, height);
    layoutComponents();
}

INT_PTR showD2DTestDialog(HWND parent) {
    D2DTestDialog dialog;
    return dialog.showModal(parent);
}

}  // namespace nive::ui::d2d

#endif  // NIVE_DEBUG_D2D_TEST
