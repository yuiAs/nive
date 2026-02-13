/// @file main.cpp
/// @brief nive application entry point

#include <Windows.h>

#include <CommCtrl.h>
#include <ShlObj.h>
#include <objbase.h>

#include "core/util/logger.hpp"
#include "ui/app.hpp"

// Link with required libraries
#pragma comment(lib, "comctl32.lib")

// Enable visual styles
#pragma comment(linker,                                                                            \
                "\"/manifestdependency:type='win32' \
    name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
    processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

/// @brief Initialize Common Controls
bool InitializeCommonControls() {
    INITCOMMONCONTROLSEX icc = {};
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    return InitCommonControlsEx(&icc) != FALSE;
}

/// @brief Parse command line for initial path
std::wstring ParseCommandLine(LPWSTR lpCmdLine) {
    if (!lpCmdLine || lpCmdLine[0] == L'\0') {
        return L"";
    }

    // Simple parsing: use the entire command line as a path
    // Strip quotes if present
    std::wstring path = lpCmdLine;
    if (path.front() == L'"' && path.back() == L'"') {
        path = path.substr(1, path.length() - 2);
    }

    return path;
}

}  // namespace

/// @brief Windows application entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/,
                    _In_ LPWSTR lpCmdLine, _In_ int /*nCmdShow*/
) {
    // Initialize COM (required for various Windows APIs)
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(hr)) {
        MessageBoxW(nullptr, L"Failed to initialize COM", L"Error", MB_ICONERROR);
        return 1;
    }

    // Initialize Common Controls
    if (!InitializeCommonControls()) {
        MessageBoxW(nullptr, L"Failed to initialize Common Controls", L"Error", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // Initialize logging
    {
        std::filesystem::path log_path;
        wchar_t* local_app_data = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data))) {
            log_path = std::filesystem::path(local_app_data) / L"nive" / L"nive.log";
            CoTaskMemFree(local_app_data);
        } else {
            log_path = L"nive.log";
        }
        std::filesystem::create_directories(log_path.parent_path());
        nive::init_logging(log_path, false);  // File only, no console for GUI app
        LOG_INFO("nive starting...");
    }

    // Parse command line
    nive::ui::AppConfig config;
    config.initial_path = ParseCommandLine(lpCmdLine);

    // Initialize and run application
    auto& app = nive::ui::App::instance();
    if (!app.initialize(hInstance, config)) {
        CoUninitialize();
        return 1;
    }

    int result = app.run();

    // Cleanup after message loop exits
    app.shutdown();

    LOG_INFO("nive shutting down");
    nive::shutdown_logging();

    CoUninitialize();

    return result;
}
