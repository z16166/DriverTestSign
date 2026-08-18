#pragma once

#include "pch.h"

namespace testsign {

class WinException : public std::exception {
public:
    WinException(std::wstring message, DWORD code);
    WinException(const wchar_t* action, DWORD code);

    [[nodiscard]] const char* what() const noexcept override;
    [[nodiscard]] const std::wstring& wide() const noexcept { return message_; }
    [[nodiscard]] DWORD code() const noexcept { return code_; }

private:
    std::wstring message_;
    std::string utf8_;
    DWORD code_{0};
};

[[nodiscard]] std::wstring FormatWin32Message(DWORD code);
[[nodiscard]] std::wstring Trim(std::wstring text);
[[nodiscard]] std::vector<BYTE> ReadAllBytes(const std::wstring& path);
void WriteAllBytes(const std::wstring& path, const std::vector<BYTE>& data);
void EnsureExistingFile(const std::wstring& path, const wchar_t* what);

[[noreturn]] void ThrowLastError(const wchar_t* action, DWORD code = ::GetLastError());
[[noreturn]] void ThrowHResult(const wchar_t* action, HRESULT hr);
[[noreturn]] void ThrowNCrypt(const wchar_t* action, SECURITY_STATUS status);

}  // namespace testsign
