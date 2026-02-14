/// @file d2d_settings_dialog.cpp
/// @brief Implementation of D2D settings dialog

#include "d2d_settings_dialog.hpp"

#include <ShlObj.h>

#include <filesystem>
#include <format>

#include "core/config/settings_manager.hpp"
#include "core/i18n/i18n.hpp"
#include "ui/d2d/core/d2d_factory.hpp"

namespace nive::ui::d2d {

namespace {

// Helper to convert wstring to string (UTF-8)
std::string toString(const std::wstring& wstr) {
    if (wstr.empty())
        return {};
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()),
                                          nullptr, 0, nullptr, nullptr);
    std::string result(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(),
                        size_needed, nullptr, nullptr);
    return result;
}

// Helper to convert string (UTF-8) to wstring
std::wstring toWstring(const std::string& str) {
    if (str.empty())
        return {};
    int size_needed =
        MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(),
                        size_needed);
    return result;
}

// Browse for folder dialog
std::wstring browseForFolder(HWND parent, const wchar_t* title) {
    wchar_t path[MAX_PATH] = {};

    BROWSEINFOW bi = {};
    bi.hwndOwner = parent;
    bi.pszDisplayName = path;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl) {
        if (SHGetPathFromIDListW(pidl, path)) {
            CoTaskMemFree(pidl);
            return path;
        }
        CoTaskMemFree(pidl);
    }
    return {};
}

}  // namespace

D2DSettingsDialog::D2DSettingsDialog() {
    setTitle(i18n::tr("dialog.settings.title"));
    setInitialSize(Size{480.0f, 400.0f});
    setMinimumSize(Size{400.0f, 350.0f});
    setResizable(true);
}

SettingsDialogResult D2DSettingsDialog::show(HWND parent, config::Settings& settings) {
    settings_ = &settings;
    result_ = SettingsDialogResult::Cancel;

    showModal(parent);

    return result_;
}

void D2DSettingsDialog::onCreate() {
    createComponents();
    populateFromSettings();
    layoutComponents();
}

void D2DSettingsDialog::createComponents() {
    // Create tab control
    auto tabs = std::make_unique<D2DTabControl>();
    tabs->createResources(deviceResources());
    tab_control_ = tabs.get();

    // Add tabs
    tabs->addTab(i18n::tr("dialog.settings.tab.general"), createGeneralTab());
    tabs->addTab(i18n::tr("dialog.settings.tab.thumbnails"), createThumbnailsTab());
    tabs->addTab(i18n::tr("dialog.settings.tab.cache"), createCacheTab());
    tabs->addTab(i18n::tr("dialog.settings.tab.sorting"), createSortingTab());
    tabs->addTab(i18n::tr("dialog.settings.tab.network"), createNetworkTab());
    tabs->setSelectedIndex(0);

    addChild(std::move(tabs));

    // OK button
    auto ok = std::make_unique<D2DButton>(i18n::tr("dialog.settings.ok"));
    ok->setVariant(ButtonVariant::Primary);
    ok->createResources(deviceResources());
    ok->onClick([this]() {
        if (validateAndSave()) {
            result_ = SettingsDialogResult::Ok;
            endDialog(IDOK);
        }
    });
    ok_button_ = ok.get();
    addChild(std::move(ok));

    // Cancel button
    auto cancel = std::make_unique<D2DButton>(i18n::tr("dialog.settings.cancel"));
    cancel->createResources(deviceResources());
    cancel->onClick([this]() {
        result_ = SettingsDialogResult::Cancel;
        endDialog(IDCANCEL);
    });
    cancel_button_ = cancel.get();
    addChild(std::move(cancel));
}

std::unique_ptr<D2DContainerComponent> D2DSettingsDialog::createGeneralTab() {
    auto container = std::make_unique<D2DPanel>();

    // Startup directory group
    auto group = std::make_unique<D2DGroupBox>(i18n::tr("dialog.settings.general.startup_directory"));
    group->createResources(deviceResources());
    startup_group_ = group.get();

    // Radio buttons
    auto home = std::make_unique<D2DRadioButton>(i18n::tr("dialog.settings.general.home"));
    home->setGroup(&startup_radio_group_);
    home->createResources(deviceResources());
    startup_home_ = home.get();
    group->addChild(std::move(home));

    auto last = std::make_unique<D2DRadioButton>(i18n::tr("dialog.settings.general.last_opened"));
    last->setGroup(&startup_radio_group_);
    last->createResources(deviceResources());
    startup_last_ = last.get();
    group->addChild(std::move(last));

    auto custom = std::make_unique<D2DRadioButton>(i18n::tr("dialog.settings.general.custom"));
    custom->setGroup(&startup_radio_group_);
    custom->createResources(deviceResources());
    custom->onChange([this](bool selected) {
        startup_path_->setEnabled(selected);
        startup_browse_->setEnabled(selected);
    });
    startup_custom_ = custom.get();
    group->addChild(std::move(custom));

    // Path edit and browse button
    auto path_edit = std::make_unique<D2DEditBox>();
    path_edit->setEnabled(false);
    path_edit->createResources(deviceResources());
    startup_path_ = path_edit.get();
    group->addChild(std::move(path_edit));

    auto browse = std::make_unique<D2DButton>(i18n::tr("dialog.settings.general.browse"));
    browse->setEnabled(false);
    browse->createResources(deviceResources());
    browse->onClick([this]() { browseStartupPath(); });
    startup_browse_ = browse.get();
    group->addChild(std::move(browse));

    container->addChild(std::move(group));

    // Language setting
    auto lang_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.general.language"));
    lang_lbl->createResources(deviceResources());
    language_label_ = lang_lbl.get();
    container->addChild(std::move(lang_lbl));

    auto lang_combo = std::make_unique<D2DComboBox>();
    // Scan available locales and build combo items
    {
        wchar_t exe_path[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
        auto locale_dir = std::filesystem::path(exe_path).parent_path() / L"locales";
        available_locales_ = i18n::availableLocales(locale_dir);
    }
    std::vector<std::wstring> lang_items;
    lang_items.push_back(std::wstring(i18n::tr("dialog.settings.general.auto_system")));
    for (const auto& tag : available_locales_) {
        lang_items.push_back(toWstring(tag));
    }
    lang_combo->addItems(lang_items);
    lang_combo->createResources(deviceResources());
    language_combo_ = lang_combo.get();
    container->addChild(std::move(lang_combo));

    return container;
}

std::unique_ptr<D2DContainerComponent> D2DSettingsDialog::createThumbnailsTab() {
    auto container = std::make_unique<D2DPanel>();

    // Stored size
    auto stored_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.thumbnails.stored_size"));
    stored_lbl->createResources(deviceResources());
    stored_size_label_ = stored_lbl.get();
    container->addChild(std::move(stored_lbl));

    auto stored_spin = std::make_unique<D2DSpinEdit>();
    stored_spin->setRange(64, 2048);
    stored_spin->setStep(32);
    stored_spin->createResources(deviceResources());
    stored_size_spin_ = stored_spin.get();
    container->addChild(std::move(stored_spin));

    // Display size
    auto display_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.thumbnails.display_size"));
    display_lbl->createResources(deviceResources());
    display_size_label_ = display_lbl.get();
    container->addChild(std::move(display_lbl));

    auto display_spin = std::make_unique<D2DSpinEdit>();
    display_spin->setRange(32, 512);
    display_spin->setStep(16);
    display_spin->createResources(deviceResources());
    display_size_spin_ = display_spin.get();
    container->addChild(std::move(display_spin));

    // Buffer count
    auto buffer_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.thumbnails.buffer_count"));
    buffer_lbl->createResources(deviceResources());
    buffer_count_label_ = buffer_lbl.get();
    container->addChild(std::move(buffer_lbl));

    auto buffer_spin = std::make_unique<D2DSpinEdit>();
    buffer_spin->setRange(0, 1000);
    buffer_spin->setStep(10);
    buffer_spin->createResources(deviceResources());
    buffer_count_spin_ = buffer_spin.get();
    container->addChild(std::move(buffer_spin));

    // Worker count
    auto worker_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.thumbnails.worker_threads"));
    worker_lbl->createResources(deviceResources());
    worker_count_label_ = worker_lbl.get();
    container->addChild(std::move(worker_lbl));

    auto worker_spin = std::make_unique<D2DSpinEdit>();
    worker_spin->setRange(1, 16);
    worker_spin->createResources(deviceResources());
    worker_count_spin_ = worker_spin.get();
    container->addChild(std::move(worker_spin));

    return container;
}

std::unique_ptr<D2DContainerComponent> D2DSettingsDialog::createCacheTab() {
    auto container = std::make_unique<D2DPanel>();

    // Cache location group
    auto group = std::make_unique<D2DGroupBox>(i18n::tr("dialog.settings.cache.location"));
    group->createResources(deviceResources());
    cache_location_group_ = group.get();

    auto appdata = std::make_unique<D2DRadioButton>(i18n::tr("dialog.settings.cache.appdata"));
    appdata->setGroup(&cache_radio_group_);
    appdata->createResources(deviceResources());
    cache_appdata_ = appdata.get();
    group->addChild(std::move(appdata));

    auto portable = std::make_unique<D2DRadioButton>(i18n::tr("dialog.settings.cache.portable"));
    portable->setGroup(&cache_radio_group_);
    portable->createResources(deviceResources());
    cache_portable_ = portable.get();
    group->addChild(std::move(portable));

    auto custom = std::make_unique<D2DRadioButton>(i18n::tr("dialog.settings.cache.custom"));
    custom->setGroup(&cache_radio_group_);
    custom->createResources(deviceResources());
    custom->onChange([this](bool selected) {
        cache_path_->setEnabled(selected);
        cache_browse_->setEnabled(selected);
    });
    cache_custom_ = custom.get();
    group->addChild(std::move(custom));

    auto path_edit = std::make_unique<D2DEditBox>();
    path_edit->setEnabled(false);
    path_edit->createResources(deviceResources());
    cache_path_ = path_edit.get();
    group->addChild(std::move(path_edit));

    auto browse = std::make_unique<D2DButton>(i18n::tr("dialog.settings.cache.browse"));
    browse->setEnabled(false);
    browse->createResources(deviceResources());
    browse->onClick([this]() { browseCustomCachePath(); });
    cache_browse_ = browse.get();
    group->addChild(std::move(browse));

    container->addChild(std::move(group));

    // Compression level
    auto comp_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.cache.compression"));
    comp_lbl->createResources(deviceResources());
    compression_label_ = comp_lbl.get();
    container->addChild(std::move(comp_lbl));

    auto comp_spin = std::make_unique<D2DSpinEdit>();
    comp_spin->setRange(0, 19);
    comp_spin->createResources(deviceResources());
    compression_spin_ = comp_spin.get();
    container->addChild(std::move(comp_spin));

    // Retention checkbox
    auto retention_cb = std::make_unique<D2DCheckBox>(i18n::tr("dialog.settings.cache.retention_enabled"));
    retention_cb->createResources(deviceResources());
    retention_cb->onChange([this](bool checked) {
        retention_label_->setEnabled(checked);
        retention_spin_->setEnabled(checked);
    });
    retention_checkbox_ = retention_cb.get();
    container->addChild(std::move(retention_cb));

    // Retention days
    auto ret_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.cache.retention_days"));
    ret_lbl->setEnabled(false);
    ret_lbl->createResources(deviceResources());
    retention_label_ = ret_lbl.get();
    container->addChild(std::move(ret_lbl));

    auto ret_spin = std::make_unique<D2DSpinEdit>();
    ret_spin->setRange(1, 365);
    ret_spin->setEnabled(false);
    ret_spin->createResources(deviceResources());
    retention_spin_ = ret_spin.get();
    container->addChild(std::move(ret_spin));

    return container;
}

std::unique_ptr<D2DContainerComponent> D2DSettingsDialog::createSortingTab() {
    auto container = std::make_unique<D2DPanel>();

    // Sort method
    auto method_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.sorting.method"));
    method_lbl->createResources(deviceResources());
    sort_method_label_ = method_lbl.get();
    container->addChild(std::move(method_lbl));

    auto method_combo = std::make_unique<D2DComboBox>();
    method_combo->addItems({std::wstring(i18n::tr("dialog.settings.sorting.natural")),
                            std::wstring(i18n::tr("dialog.settings.sorting.lexicographic")),
                            std::wstring(i18n::tr("dialog.settings.sorting.date_modified")),
                            std::wstring(i18n::tr("dialog.settings.sorting.date_created")),
                            std::wstring(i18n::tr("dialog.settings.sorting.size"))});
    method_combo->createResources(deviceResources());
    sort_method_combo_ = method_combo.get();
    container->addChild(std::move(method_combo));

    // Sort order
    auto order_lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.sorting.order"));
    order_lbl->createResources(deviceResources());
    sort_order_label_ = order_lbl.get();
    container->addChild(std::move(order_lbl));

    auto order_combo = std::make_unique<D2DComboBox>();
    order_combo->addItems({std::wstring(i18n::tr("dialog.settings.sorting.ascending")),
                           std::wstring(i18n::tr("dialog.settings.sorting.descending"))});
    order_combo->createResources(deviceResources());
    sort_order_combo_ = order_combo.get();
    container->addChild(std::move(order_combo));

    return container;
}

std::unique_ptr<D2DContainerComponent> D2DSettingsDialog::createNetworkTab() {
    auto container = std::make_unique<D2DPanel>();

    // Label
    auto lbl = std::make_unique<D2DLabel>(i18n::tr("dialog.settings.network.label"));
    lbl->createResources(deviceResources());
    network_label_ = lbl.get();
    container->addChild(std::move(lbl));

    // List box
    auto list = std::make_unique<D2DListBox>();
    list->createResources(deviceResources());
    network_list_ = list.get();
    container->addChild(std::move(list));

    // Add button
    auto add = std::make_unique<D2DButton>(i18n::tr("dialog.settings.network.add"));
    add->createResources(deviceResources());
    add->onClick([this]() { addNetworkShare(); });
    network_add_ = add.get();
    container->addChild(std::move(add));

    // Remove button
    auto remove = std::make_unique<D2DButton>(i18n::tr("dialog.settings.network.remove"));
    remove->createResources(deviceResources());
    remove->onClick([this]() { removeNetworkShare(); });
    network_remove_ = remove.get();
    container->addChild(std::move(remove));

    return container;
}

void D2DSettingsDialog::layoutComponents() {
    if (!tab_control_)
        return;

    auto size = deviceResources().getSize();
    float padding = 15.0f;
    float button_height = 28.0f;
    float button_width = 80.0f;
    float button_spacing = 10.0f;

    // Buttons at bottom
    float button_y = size.height - padding - button_height;
    float cancel_x = size.width - padding - button_width;
    float ok_x = cancel_x - button_spacing - button_width;

    ok_button_->arrange(Rect{ok_x, button_y, button_width, button_height});
    cancel_button_->arrange(Rect{cancel_x, button_y, button_width, button_height});

    // Tab control fills the rest
    float tab_height = button_y - padding * 2;
    tab_control_->arrange(Rect{padding, padding, size.width - padding * 2, tab_height});

    // Layout each tab's content
    layoutGeneralTab();
    layoutThumbnailsTab();
    layoutCacheTab();
    layoutSortingTab();
    layoutNetworkTab();
}

void D2DSettingsDialog::layoutGeneralTab() {
    if (!startup_group_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float item_height = 24.0f;
    float item_spacing = 8.0f;
    float button_width = 35.0f;

    // Startup group takes full width
    float group_height = padding + (item_height + item_spacing) * 4 + item_spacing;
    startup_group_->arrange(
        Rect{content.x + padding, content.y + padding, content.width - padding * 2, group_height});

    // Layout items inside group
    Rect group_content = startup_group_->contentArea();
    float y = group_content.y;

    startup_home_->arrange(Rect{group_content.x, y, group_content.width, item_height});
    y += item_height + item_spacing;

    startup_last_->arrange(Rect{group_content.x, y, group_content.width, item_height});
    y += item_height + item_spacing;

    float custom_radio_width = 80.0f;
    startup_custom_->arrange(Rect{group_content.x, y, custom_radio_width, item_height});

    float path_x = group_content.x + custom_radio_width + 5.0f;
    float path_width = group_content.width - custom_radio_width - button_width - 10.0f;
    startup_path_->arrange(Rect{path_x, y, path_width, item_height});

    float browse_x = path_x + path_width + 5.0f;
    startup_browse_->arrange(Rect{browse_x, y, button_width, item_height});

    // Language row (below startup group)
    float lang_y = content.y + padding + group_height + padding;
    float label_x = content.x + padding;
    float label_width = 80.0f;
    float combo_width = 150.0f;

    language_label_->arrange(Rect{label_x, lang_y, label_width, item_height});
    language_combo_->arrange(
        Rect{label_x + label_width + 10.0f, lang_y, combo_width, item_height});
}

void D2DSettingsDialog::layoutThumbnailsTab() {
    if (!stored_size_label_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float label_width = 180.0f;
    float spin_width = 100.0f;
    float item_height = 24.0f;
    float row_spacing = 12.0f;

    float y = content.y + padding;
    float label_x = content.x + padding;
    float spin_x = label_x + label_width + 10.0f;

    stored_size_label_->arrange(Rect{label_x, y, label_width, item_height});
    stored_size_spin_->arrange(Rect{spin_x, y, spin_width, item_height});
    y += item_height + row_spacing;

    display_size_label_->arrange(Rect{label_x, y, label_width, item_height});
    display_size_spin_->arrange(Rect{spin_x, y, spin_width, item_height});
    y += item_height + row_spacing;

    buffer_count_label_->arrange(Rect{label_x, y, label_width, item_height});
    buffer_count_spin_->arrange(Rect{spin_x, y, spin_width, item_height});
    y += item_height + row_spacing;

    worker_count_label_->arrange(Rect{label_x, y, label_width, item_height});
    worker_count_spin_->arrange(Rect{spin_x, y, spin_width, item_height});
}

void D2DSettingsDialog::layoutCacheTab() {
    if (!cache_location_group_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float item_height = 24.0f;
    float item_spacing = 8.0f;
    float button_width = 35.0f;
    float label_width = 180.0f;
    float spin_width = 80.0f;

    // Cache location group
    float group_height = padding + (item_height + item_spacing) * 4 + item_spacing;
    cache_location_group_->arrange(
        Rect{content.x + padding, content.y + padding, content.width - padding * 2, group_height});

    // Layout items inside group
    Rect group_content = cache_location_group_->contentArea();
    float y = group_content.y;

    cache_appdata_->arrange(Rect{group_content.x, y, group_content.width, item_height});
    y += item_height + item_spacing;

    cache_portable_->arrange(Rect{group_content.x, y, group_content.width, item_height});
    y += item_height + item_spacing;

    float custom_radio_width = 80.0f;
    cache_custom_->arrange(Rect{group_content.x, y, custom_radio_width, item_height});

    float path_x = group_content.x + custom_radio_width + 5.0f;
    float path_width = group_content.width - custom_radio_width - button_width - 10.0f;
    cache_path_->arrange(Rect{path_x, y, path_width, item_height});

    float browse_x = path_x + path_width + 5.0f;
    cache_browse_->arrange(Rect{browse_x, y, button_width, item_height});

    // Compression row
    y = content.y + padding + group_height + padding;
    float label_x = content.x + padding;
    float spin_x = label_x + label_width + 10.0f;

    compression_label_->arrange(Rect{label_x, y, label_width, item_height});
    compression_spin_->arrange(Rect{spin_x, y, spin_width, item_height});
    y += item_height + item_spacing;

    // Retention checkbox
    retention_checkbox_->arrange(Rect{label_x, y, 200.0f, item_height});
    y += item_height + item_spacing;

    // Retention days
    retention_label_->arrange(Rect{label_x + 20.0f, y, label_width - 20.0f, item_height});
    retention_spin_->arrange(Rect{spin_x, y, spin_width, item_height});
}

void D2DSettingsDialog::layoutSortingTab() {
    if (!sort_method_label_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float label_width = 100.0f;
    float combo_width = 250.0f;
    float item_height = 24.0f;
    float row_spacing = 12.0f;

    float y = content.y + padding;
    float label_x = content.x + padding;
    float combo_x = label_x + label_width + 10.0f;

    sort_method_label_->arrange(Rect{label_x, y, label_width, item_height});
    sort_method_combo_->arrange(Rect{combo_x, y, combo_width, item_height});
    y += item_height + row_spacing;

    sort_order_label_->arrange(Rect{label_x, y, label_width, item_height});
    sort_order_combo_->arrange(Rect{combo_x, y, 150.0f, item_height});
}

void D2DSettingsDialog::layoutNetworkTab() {
    if (!network_label_)
        return;

    Rect content = tab_control_->contentArea();
    float padding = 15.0f;
    float item_height = 24.0f;
    float button_width = 80.0f;
    float button_height = 28.0f;
    float spacing = 8.0f;

    float y = content.y + padding;
    float label_x = content.x + padding;

    network_label_->arrange(Rect{label_x, y, 200.0f, item_height});
    y += item_height + spacing;

    // List takes most of the space
    float list_width = content.width - padding * 2 - button_width - spacing;
    float list_height = content.height - padding * 2 - item_height - spacing;
    network_list_->arrange(Rect{label_x, y, list_width, list_height});

    // Buttons on the right
    float button_x = label_x + list_width + spacing;
    network_add_->arrange(Rect{button_x, y, button_width, button_height});
    network_remove_->arrange(
        Rect{button_x, y + button_height + spacing, button_width, button_height});
}

void D2DSettingsDialog::populateFromSettings() {
    if (!settings_)
        return;

    // General tab - Startup directory
    switch (settings_->startup_directory) {
    case config::StartupDirectory::Home:
        startup_home_->setSelected(true);
        break;
    case config::StartupDirectory::LastOpened:
        startup_last_->setSelected(true);
        break;
    case config::StartupDirectory::Custom:
        startup_custom_->setSelected(true);
        startup_path_->setEnabled(true);
        startup_browse_->setEnabled(true);
        break;
    }
    if (!settings_->custom_startup_path.empty()) {
        startup_path_->setText(settings_->custom_startup_path.wstring());
    }

    // Language setting
    if (settings_->language == "auto" || settings_->language.empty()) {
        language_combo_->setSelectedIndex(0);  // "Auto (System)"
    } else {
        // Find matching locale in available_locales_
        int lang_index = 0;
        for (size_t i = 0; i < available_locales_.size(); ++i) {
            if (available_locales_[i] == settings_->language) {
                lang_index = static_cast<int>(i) + 1;  // +1 for "Auto (System)"
                break;
            }
        }
        language_combo_->setSelectedIndex(lang_index);
    }

    // Thumbnails tab
    stored_size_spin_->setValue(settings_->thumbnails.stored_size);
    display_size_spin_->setValue(settings_->thumbnails.display_size);
    buffer_count_spin_->setValue(settings_->thumbnails.buffer_count);
    worker_count_spin_->setValue(settings_->thumbnails.worker_count);

    // Cache tab
    switch (settings_->cache.location) {
    case config::CacheLocation::AppData:
        cache_appdata_->setSelected(true);
        break;
    case config::CacheLocation::Portable:
        cache_portable_->setSelected(true);
        break;
    case config::CacheLocation::Custom:
        cache_custom_->setSelected(true);
        cache_path_->setEnabled(true);
        cache_browse_->setEnabled(true);
        break;
    }
    if (!settings_->cache.custom_path.empty()) {
        cache_path_->setText(settings_->cache.custom_path.wstring());
    }
    compression_spin_->setValue(settings_->cache.compression_level);
    retention_checkbox_->setChecked(settings_->cache.retention_enabled);
    retention_spin_->setValue(settings_->cache.retention_days);
    retention_label_->setEnabled(settings_->cache.retention_enabled);
    retention_spin_->setEnabled(settings_->cache.retention_enabled);

    // Sorting tab
    int method_index = 0;
    switch (settings_->sort.method) {
    case config::SortMethod::Natural:
        method_index = 0;
        break;
    case config::SortMethod::Lexicographic:
        method_index = 1;
        break;
    case config::SortMethod::DateModified:
        method_index = 2;
        break;
    case config::SortMethod::DateCreated:
        method_index = 3;
        break;
    case config::SortMethod::Size:
        method_index = 4;
        break;
    }
    sort_method_combo_->setSelectedIndex(method_index);
    sort_order_combo_->setSelectedIndex(settings_->sort.ascending ? 0 : 1);

    // Network tab
    network_list_->clearItems();
    for (const auto& share : settings_->network_shares) {
        network_list_->addItem(toWstring(share));
    }
}

bool D2DSettingsDialog::validateAndSave() {
    if (!settings_)
        return false;

    // General tab
    if (startup_home_->isSelected()) {
        settings_->startup_directory = config::StartupDirectory::Home;
    } else if (startup_last_->isSelected()) {
        settings_->startup_directory = config::StartupDirectory::LastOpened;
    } else {
        settings_->startup_directory = config::StartupDirectory::Custom;
    }
    settings_->custom_startup_path = startup_path_->text();

    // Language setting
    int lang_index = language_combo_->selectedIndex();
    if (lang_index <= 0) {
        settings_->language = "auto";
    } else {
        size_t locale_idx = static_cast<size_t>(lang_index) - 1;
        if (locale_idx < available_locales_.size()) {
            settings_->language = available_locales_[locale_idx];
        } else {
            settings_->language = "auto";
        }
    }

    // Thumbnails tab
    settings_->thumbnails.stored_size = stored_size_spin_->value();
    settings_->thumbnails.display_size = display_size_spin_->value();
    settings_->thumbnails.buffer_count = buffer_count_spin_->value();
    settings_->thumbnails.worker_count = worker_count_spin_->value();

    // Cache tab
    if (cache_appdata_->isSelected()) {
        settings_->cache.location = config::CacheLocation::AppData;
    } else if (cache_portable_->isSelected()) {
        settings_->cache.location = config::CacheLocation::Portable;
    } else {
        settings_->cache.location = config::CacheLocation::Custom;
    }
    settings_->cache.custom_path = cache_path_->text();
    settings_->cache.compression_level = compression_spin_->value();
    settings_->cache.retention_enabled = retention_checkbox_->isChecked();
    settings_->cache.retention_days = retention_spin_->value();

    // Sorting tab
    int method_index = sort_method_combo_->selectedIndex();
    switch (method_index) {
    case 0:
        settings_->sort.method = config::SortMethod::Natural;
        break;
    case 1:
        settings_->sort.method = config::SortMethod::Lexicographic;
        break;
    case 2:
        settings_->sort.method = config::SortMethod::DateModified;
        break;
    case 3:
        settings_->sort.method = config::SortMethod::DateCreated;
        break;
    case 4:
        settings_->sort.method = config::SortMethod::Size;
        break;
    default:
        settings_->sort.method = config::SortMethod::Natural;
        break;
    }
    settings_->sort.ascending = (sort_order_combo_->selectedIndex() == 0);

    // Network tab
    settings_->network_shares.clear();
    for (size_t i = 0; i < network_list_->itemCount(); ++i) {
        settings_->network_shares.push_back(toString(network_list_->itemAt(i)));
    }

    // Validate
    auto validation = config::SettingsManager::validate(*settings_);
    if (!validation.valid) {
        std::wstring msg(i18n::tr("dialog.settings.validation.invalid_settings"));
        for (const auto& error : validation.errors) {
            msg += std::wstring(i18n::tr("dialog.settings.validation.error_prefix"))
                + toWstring(error) + L"\n";
        }
        MessageBoxW(hwnd(), msg.c_str(),
                     i18n::tr("dialog.settings.validation.title").c_str(),
                     MB_ICONERROR | MB_OK);
        return false;
    }

    return true;
}

void D2DSettingsDialog::browseStartupPath() {
    auto path = browseForFolder(hwnd(), i18n::tr("dialog.settings.general.select_startup_dir").c_str());
    if (!path.empty()) {
        startup_path_->setText(path);
    }
}

void D2DSettingsDialog::browseCustomCachePath() {
    auto path = browseForFolder(hwnd(), i18n::tr("dialog.settings.cache.select_cache_dir").c_str());
    if (!path.empty()) {
        cache_path_->setText(path);
    }
}

void D2DSettingsDialog::addNetworkShare() {
    auto path = browseForFolder(hwnd(), i18n::tr("dialog.settings.network.select_share").c_str());
    if (!path.empty()) {
        network_list_->addItem(path);
    }
}

void D2DSettingsDialog::removeNetworkShare() {
    int sel = network_list_->selectedIndex();
    if (sel >= 0) {
        network_list_->removeItem(static_cast<size_t>(sel));
    }
}

void D2DSettingsDialog::recreateAllComponentResources() {
    auto& res = deviceResources();

    if (tab_control_)
        tab_control_->createResources(res);
    if (ok_button_)
        ok_button_->createResources(res);
    if (cancel_button_)
        cancel_button_->createResources(res);

    // General tab
    if (startup_group_)
        startup_group_->createResources(res);
    if (startup_home_)
        startup_home_->createResources(res);
    if (startup_last_)
        startup_last_->createResources(res);
    if (startup_custom_)
        startup_custom_->createResources(res);
    if (startup_path_)
        startup_path_->createResources(res);
    if (startup_browse_)
        startup_browse_->createResources(res);
    if (language_label_)
        language_label_->createResources(res);
    if (language_combo_)
        language_combo_->createResources(res);

    // Thumbnails tab
    if (stored_size_label_)
        stored_size_label_->createResources(res);
    if (stored_size_spin_)
        stored_size_spin_->createResources(res);
    if (display_size_label_)
        display_size_label_->createResources(res);
    if (display_size_spin_)
        display_size_spin_->createResources(res);
    if (buffer_count_label_)
        buffer_count_label_->createResources(res);
    if (buffer_count_spin_)
        buffer_count_spin_->createResources(res);
    if (worker_count_label_)
        worker_count_label_->createResources(res);
    if (worker_count_spin_)
        worker_count_spin_->createResources(res);

    // Cache tab
    if (cache_location_group_)
        cache_location_group_->createResources(res);
    if (cache_appdata_)
        cache_appdata_->createResources(res);
    if (cache_portable_)
        cache_portable_->createResources(res);
    if (cache_custom_)
        cache_custom_->createResources(res);
    if (cache_path_)
        cache_path_->createResources(res);
    if (cache_browse_)
        cache_browse_->createResources(res);
    if (compression_label_)
        compression_label_->createResources(res);
    if (compression_spin_)
        compression_spin_->createResources(res);
    if (retention_checkbox_)
        retention_checkbox_->createResources(res);
    if (retention_label_)
        retention_label_->createResources(res);
    if (retention_spin_)
        retention_spin_->createResources(res);

    // Sorting tab
    if (sort_method_label_)
        sort_method_label_->createResources(res);
    if (sort_method_combo_)
        sort_method_combo_->createResources(res);
    if (sort_order_label_)
        sort_order_label_->createResources(res);
    if (sort_order_combo_)
        sort_order_combo_->createResources(res);

    // Network tab
    if (network_label_)
        network_label_->createResources(res);
    if (network_list_)
        network_list_->createResources(res);
    if (network_add_)
        network_add_->createResources(res);
    if (network_remove_)
        network_remove_->createResources(res);
}

void D2DSettingsDialog::updateActiveTabResources() {
    auto& res = deviceResources();

    // Always update shared interactive components
    if (tab_control_)
        tab_control_->createResources(res);
    if (ok_button_)
        ok_button_->createResources(res);
    if (cancel_button_)
        cancel_button_->createResources(res);

    // Only update components on the active tab
    int active = tab_control_ ? tab_control_->selectedIndex() : 0;
    switch (active) {
    case 0:  // General
        if (startup_home_)
            startup_home_->createResources(res);
        if (startup_last_)
            startup_last_->createResources(res);
        if (startup_custom_)
            startup_custom_->createResources(res);
        if (startup_path_)
            startup_path_->createResources(res);
        if (startup_browse_)
            startup_browse_->createResources(res);
        if (language_combo_)
            language_combo_->createResources(res);
        break;
    case 1:  // Thumbnails
        if (stored_size_spin_)
            stored_size_spin_->createResources(res);
        if (display_size_spin_)
            display_size_spin_->createResources(res);
        if (buffer_count_spin_)
            buffer_count_spin_->createResources(res);
        if (worker_count_spin_)
            worker_count_spin_->createResources(res);
        break;
    case 2:  // Cache
        if (cache_appdata_)
            cache_appdata_->createResources(res);
        if (cache_portable_)
            cache_portable_->createResources(res);
        if (cache_custom_)
            cache_custom_->createResources(res);
        if (cache_path_)
            cache_path_->createResources(res);
        if (cache_browse_)
            cache_browse_->createResources(res);
        if (compression_spin_)
            compression_spin_->createResources(res);
        if (retention_checkbox_)
            retention_checkbox_->createResources(res);
        if (retention_spin_)
            retention_spin_->createResources(res);
        break;
    case 3:  // Sorting
        if (sort_method_combo_)
            sort_method_combo_->createResources(res);
        if (sort_order_combo_)
            sort_order_combo_->createResources(res);
        break;
    case 4:  // Network
        if (network_list_)
            network_list_->createResources(res);
        if (network_add_)
            network_add_->createResources(res);
        if (network_remove_)
            network_remove_->createResources(res);
        break;
    }
}

void D2DSettingsDialog::onRender(ID2D1RenderTarget* rt) {
    // Clear with background color
    rt->Clear(D2D1::ColorF(0xF5F5F5));

    // On device-lost recovery, recreate ALL component resources
    uint32_t epoch = deviceResources().resourceEpoch();
    if (epoch != last_resource_epoch_) {
        recreateAllComponentResources();
        last_resource_epoch_ = epoch;
    } else {
        // Normal frame: only update active tab's interactive components
        updateActiveTabResources();
    }

    // Render all child components
    D2DDialog::onRender(rt);

    // Render popup (dropdown) on top if any
    if (active_popup_ && active_popup_->isDropdownOpen()) {
        active_popup_->renderDropdown(rt);
    }
}

void D2DSettingsDialog::onResize(float width, float height) {
    D2DDialog::onResize(width, height);
    layoutComponents();
}

bool D2DSettingsDialog::onClose() {
    // Close any open dropdown
    if (active_popup_) {
        active_popup_->closeDropdown();
        active_popup_ = nullptr;
    }
    return true;
}

bool D2DSettingsDialog::onMouseDown(const MouseEvent& event) {
    // Check if click is on dropdown popup
    if (active_popup_ && active_popup_->isDropdownOpen()) {
        Rect dropdown = active_popup_->dropdownRect();
        if (event.position.x >= dropdown.x && event.position.x < dropdown.x + dropdown.width &&
            event.position.y >= dropdown.y && event.position.y < dropdown.y + dropdown.height) {
            // Click in dropdown - select item
            if (active_popup_->selectDropdownItem(event.position)) {
                active_popup_->closeDropdown();
                active_popup_ = nullptr;
                InvalidateRect(hwnd(), nullptr, FALSE);
                return true;
            }
        }
        // Click outside dropdown - close it
        active_popup_->closeDropdown();
        active_popup_ = nullptr;
        InvalidateRect(hwnd(), nullptr, FALSE);
    }

    // Forward to base class for normal handling
    bool result = D2DDialog::onMouseDown(event);

    // Check if any combo box opened a dropdown
    if (sort_method_combo_ && sort_method_combo_->isDropdownOpen()) {
        active_popup_ = sort_method_combo_;
    } else if (sort_order_combo_ && sort_order_combo_->isDropdownOpen()) {
        active_popup_ = sort_order_combo_;
    } else if (language_combo_ && language_combo_->isDropdownOpen()) {
        active_popup_ = language_combo_;
    }

    return result;
}

bool D2DSettingsDialog::onMouseMove(const MouseEvent& event) {
    // Update hover state in dropdown
    if (active_popup_ && active_popup_->isDropdownOpen()) {
        Rect dropdown = active_popup_->dropdownRect();
        if (event.position.x >= dropdown.x && event.position.x < dropdown.x + dropdown.width &&
            event.position.y >= dropdown.y && event.position.y < dropdown.y + dropdown.height) {
            int item = active_popup_->hitTestDropdownItem(event.position);
            active_popup_->setHoveredItem(item);
            InvalidateRect(hwnd(), nullptr, FALSE);
            return true;
        }
    }

    return D2DDialog::onMouseMove(event);
}

bool showD2DSettingsDialog(HWND parent, config::Settings& settings) {
    D2DSettingsDialog dialog;
    return dialog.show(parent, settings) == SettingsDialogResult::Ok;
}

}  // namespace nive::ui::d2d
