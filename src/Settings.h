#pragma once

#include "pch.h"

namespace testsign {

enum class ThemeMode { Dark, Light, System };
enum class UiLanguage { English, ChineseSimplified };

class Settings {
public:
    static constexpr wchar_t kKeyPathName[] = L"LastKeyPath";
    static constexpr wchar_t kSysPathName[] = L"LastSysPath";
    static constexpr wchar_t kInfPathName[] = L"LastInfPath";
    static constexpr wchar_t kThemeModeName[] = L"ThemeMode";
    static constexpr wchar_t kLanguageName[] = L"Language";

    struct Paths {
        std::wstring key;
        std::wstring sys;
        std::wstring inf;
    };

    [[nodiscard]] static std::wstring Load(std::wstring_view name);
    static void Save(std::wstring_view name, const std::wstring& value);
    [[nodiscard]] static Paths LoadAll();
    static void SaveAll(const Paths& paths);
    [[nodiscard]] static ThemeMode LoadThemeMode();
    static void SaveThemeMode(ThemeMode mode);
    [[nodiscard]] static UiLanguage LoadLanguage();
    static void SaveLanguage(UiLanguage language);
};

}  // namespace testsign
