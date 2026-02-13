/// @file win32_utils.hpp
/// @brief Windows API helper utilities
///
/// Provides RAII wrappers and helper functions for common Windows API patterns.

#pragma once

#include <Windows.h>

#include <objbase.h>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace nive {

/// @brief RAII wrapper for COM initialization
///
/// Usage:
///   ComInitializer com(COINIT_APARTMENTTHREADED);
///   if (!com) { handle error }
class ComInitializer {
public:
    explicit ComInitializer(DWORD flags = COINIT_APARTMENTTHREADED) {
        hr_ = CoInitializeEx(nullptr, flags);
    }

    ~ComInitializer() {
        if (SUCCEEDED(hr_)) {
            CoUninitialize();
        }
    }

    // Non-copyable, non-movable
    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;
    ComInitializer(ComInitializer&&) = delete;
    ComInitializer& operator=(ComInitializer&&) = delete;

    [[nodiscard]] bool succeeded() const noexcept { return SUCCEEDED(hr_); }

    [[nodiscard]] explicit operator bool() const noexcept { return succeeded(); }

    [[nodiscard]] HRESULT result() const noexcept { return hr_; }

private:
    HRESULT hr_;
};

/// @brief RAII wrapper for Windows HANDLE
class HandleGuard {
public:
    HandleGuard() = default;
    explicit HandleGuard(HANDLE handle) : handle_(handle) {}

    ~HandleGuard() { close(); }

    // Non-copyable
    HandleGuard(const HandleGuard&) = delete;
    HandleGuard& operator=(const HandleGuard&) = delete;

    // Movable
    HandleGuard(HandleGuard&& other) noexcept : handle_(other.handle_) {
        other.handle_ = INVALID_HANDLE_VALUE;
    }

    HandleGuard& operator=(HandleGuard&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void close() noexcept {
        if (handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }

    [[nodiscard]] HANDLE* put() noexcept {
        close();
        return &handle_;
    }

    [[nodiscard]] HANDLE release() noexcept {
        HANDLE h = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return h;
    }

    [[nodiscard]] bool valid() const noexcept {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return valid(); }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

/// @brief RAII wrapper for LocalAlloc/LocalFree
template <typename T>
class LocalMemory {
public:
    LocalMemory() = default;
    explicit LocalMemory(T* ptr) : ptr_(ptr) {}

    ~LocalMemory() { free(); }

    // Non-copyable
    LocalMemory(const LocalMemory&) = delete;
    LocalMemory& operator=(const LocalMemory&) = delete;

    // Movable
    LocalMemory(LocalMemory&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    LocalMemory& operator=(LocalMemory&& other) noexcept {
        if (this != &other) {
            free();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    void free() noexcept {
        if (ptr_) {
            LocalFree(ptr_);
            ptr_ = nullptr;
        }
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }

    [[nodiscard]] T** put() noexcept {
        free();
        return &ptr_;
    }

    [[nodiscard]] T* release() noexcept {
        T* p = ptr_;
        ptr_ = nullptr;
        return p;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_ = nullptr;
};

/// @brief Get the last Win32 error as a string
[[nodiscard]] std::string getLastErrorString();

/// @brief Get error string for specific error code
[[nodiscard]] std::string getErrorString(DWORD error_code);

/// @brief Get the path to the executable
[[nodiscard]] std::filesystem::path getExecutablePath();

/// @brief Get the directory containing the executable
[[nodiscard]] std::filesystem::path getExecutableDirectory();

/// @brief Get %APPDATA% path
[[nodiscard]] std::optional<std::filesystem::path> getAppdataPath();

/// @brief Get %LOCALAPPDATA% path
[[nodiscard]] std::optional<std::filesystem::path> getLocalAppdataPath();

/// @brief Get system temporary directory
[[nodiscard]] std::filesystem::path getTempPath();

/// @brief Check if running on Windows 10 or later
[[nodiscard]] bool isWindows10OrLater();

/// @brief Enable DPI awareness for the process
void enableDpiAwareness();

}  // namespace nive
