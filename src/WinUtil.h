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
[[nodiscard]] std::wstring ParentDirectory(const std::wstring& path);
[[nodiscard]] std::wstring FileName(const std::wstring& path);
[[nodiscard]] std::wstring JoinPath(const std::wstring& directory, const std::wstring& name);
[[nodiscard]] std::wstring CanonicalPath(const std::wstring& path);
[[nodiscard]] bool SamePath(const std::wstring& left, const std::wstring& right);
[[nodiscard]] std::wstring ReadTextFile(const std::wstring& path);
[[nodiscard]] std::wstring FindWindowsKitTool(const wchar_t* name);
[[nodiscard]] DWORD RunHiddenCommand(const std::wstring& command_line, const std::wstring& cwd,
                                     std::wstring& output, DWORD timeout_ms = 120000);
[[nodiscard]] std::vector<BYTE> ReadAllBytes(const std::wstring& path);
void WriteAllBytes(const std::wstring& path, const std::vector<BYTE>& data);
void EnsureExistingFile(const std::wstring& path, const wchar_t* what);

[[noreturn]] void ThrowLastError(const wchar_t* action, DWORD code = ::GetLastError());
[[noreturn]] void ThrowHResult(const wchar_t* action, HRESULT hr);
[[noreturn]] void ThrowNCrypt(const wchar_t* action, SECURITY_STATUS status);

}  // namespace testsign
