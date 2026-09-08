#pragma once

#include "pch.h"

namespace testsign {

inline constexpr wchar_t kTimestampUrl[] = L"http://timestamp.digicert.com";
inline constexpr wchar_t kTimestampUrlFallback[] = L"http://timestamp.sectigo.com";

void SignDriver(const std::wstring& pfx_path, const std::wstring& sys_path);
std::wstring SignCatalog(const std::wstring& pfx_path, const std::wstring& inf_path,
                         const std::wstring& sys_path,
                         const std::function<void(const wchar_t*)>& progress = {});

}  // namespace testsign
