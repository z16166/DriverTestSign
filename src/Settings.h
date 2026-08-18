#pragma once

#include "pch.h"

namespace testsign {

class Settings {
public:
    static constexpr wchar_t kKeyPathName[] = L"LastKeyPath";
    static constexpr wchar_t kSysPathName[] = L"LastSysPath";

    struct Paths {
        std::wstring key;
        std::wstring sys;
    };

    [[nodiscard]] static std::wstring Load(std::wstring_view name);
    static void Save(std::wstring_view name, const std::wstring& value);
    [[nodiscard]] static Paths LoadAll();
    static void SaveAll(const Paths& paths);
};

}  // namespace testsign
