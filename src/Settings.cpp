#include "pch.h"
#include "Settings.h"
#include "Raii.h"

namespace testsign {
namespace {

std::mutex g_settings_mutex;

constexpr wchar_t kSettingsSubKey[] = L"Software\\TestSign";

UniqueRegistryKey OpenSettingsKey(bool create) {
    HKEY raw = nullptr;
    const LSTATUS status = create
        ? ::RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsSubKey, 0, nullptr, 0,
                            KEY_READ | KEY_WRITE, nullptr, &raw, nullptr)
        : ::RegOpenKeyExW(HKEY_CURRENT_USER, kSettingsSubKey, 0, KEY_READ, &raw);
    UniqueRegistryKey key(raw);
    if (status != ERROR_SUCCESS) {
        return {};
    }
    return key;
}

}  // namespace

std::wstring Settings::Load(std::wstring_view name) {
    const std::lock_guard<std::mutex> lock(g_settings_mutex);
    const UniqueRegistryKey key = OpenSettingsKey(false);
    if (!key) {
        return {};
    }

    const std::wstring value_name(name);
    DWORD type = 0;
    DWORD bytes = 0;
    LSTATUS status = ::RegQueryValueExW(key.get(), value_name.c_str(), nullptr, &type, nullptr, &bytes);
    if (status != ERROR_SUCCESS || type != REG_SZ || bytes < sizeof(wchar_t)) {
        return {};
    }

    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    status = ::RegQueryValueExW(key.get(), value_name.c_str(), nullptr, &type,
                               reinterpret_cast<LPBYTE>(value.data()), &bytes);
    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return {};
    }

    if (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    const auto terminator = value.find(L'\0');
    if (terminator != std::wstring::npos) {
        value.resize(terminator);
    }
    return value;
}

void Settings::Save(std::wstring_view name, const std::wstring& value) {
    const std::lock_guard<std::mutex> lock(g_settings_mutex);
    UniqueRegistryKey key = OpenSettingsKey(true);
    if (!key) {
        return;
    }

    const std::wstring value_name(name);
    const DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    ::RegSetValueExW(key.get(), value_name.c_str(), 0, REG_SZ,
                     reinterpret_cast<const BYTE*>(value.c_str()), bytes);
}

Settings::Paths Settings::LoadAll() {
    Paths paths;
    paths.key = Load(kKeyPathName);
    paths.sys = Load(kSysPathName);
    return paths;
}

void Settings::SaveAll(const Paths& paths) {
    Save(kKeyPathName, paths.key);
    Save(kSysPathName, paths.sys);
}

}  // namespace testsign
