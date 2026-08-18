#include "pch.h"
#include "WinUtil.h"
#include "Raii.h"
#include "Strings.h"

namespace testsign {
namespace {

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int bytes = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                            nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(bytes), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), utf8.data(),
                          bytes, nullptr, nullptr);
    return utf8;
}

}  // namespace

WinException::WinException(std::wstring message, DWORD code)
    : message_(std::move(message)), code_(code) {
    utf8_ = WideToUtf8(message_);
}

WinException::WinException(const wchar_t* action, DWORD code)
    : WinException(std::wstring(action) + Tr(Str::ErrorJoin) + FormatWin32Message(code), code) {}

const char* WinException::what() const noexcept {
    return utf8_.c_str();
}

std::wstring FormatWin32Message(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD chars = ::FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    UniqueLocal<wchar_t> message(buffer);

    wchar_t prefix[32]{};
    swprintf_s(prefix, L"[0x%08X] ", static_cast<unsigned int>(code));

    std::wstring text = prefix;
    if (chars > 0 && message) {
        text.append(message.get(), chars);
        while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' ')) {
            text.pop_back();
        }
    } else {
        text += Tr(Str::UnknownError);
    }
    return text;
}

std::wstring Trim(std::wstring text) {
    const auto not_space = [](wint_t ch) { return std::iswspace(ch) == 0; };
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), not_space));
    text.erase(std::find_if(text.rbegin(), text.rend(), not_space).base(), text.end());
    return text;
}

std::vector<BYTE> ReadAllBytes(const std::wstring& path) {
    UniqueHandle file(::CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        ThrowLastError(Tr(Str::OpenFileFailed));
    }

    LARGE_INTEGER size{};
    if (!::GetFileSizeEx(file.get(), &size)) {
        ThrowLastError(Tr(Str::GetFileSizeFailed));
    }
    if (size.QuadPart < 0 || size.QuadPart > 64 * 1024 * 1024) {
        throw WinException(Tr(Str::FileTooLarge), ERROR_FILE_TOO_LARGE);
    }

    std::vector<BYTE> data(static_cast<size_t>(size.QuadPart));
    if (data.empty()) {
        return data;
    }

    DWORD read = 0;
    if (!::ReadFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &read, nullptr) ||
        read != static_cast<DWORD>(data.size())) {
        ThrowLastError(Tr(Str::ReadFileFailed));
    }
    return data;
}

void EnsureExistingFile(const std::wstring& path, const wchar_t* what) {
    if (path.empty()) {
        throw WinException(std::wstring(Tr(Str::SelectPathPrefix)) + what + Tr(Str::SelectPathSuffix),
                           ERROR_INVALID_PARAMETER);
    }
    if (::GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
        throw WinException(std::wstring(what) + Tr(Str::DoesNotExist) + path, ERROR_FILE_NOT_FOUND);
    }
}

void WriteAllBytes(const std::wstring& path, const std::vector<BYTE>& data) {
    UniqueHandle file(::CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        ThrowLastError(Tr(Str::CreateFileFailed));
    }

    if (data.empty()) {
        return;
    }

    DWORD written = 0;
    if (!::WriteFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &written, nullptr) ||
        written != static_cast<DWORD>(data.size())) {
        ThrowLastError(Tr(Str::WriteFileFailed));
    }
}

void ThrowLastError(const wchar_t* action, DWORD code) {
    throw WinException(action, code);
}

void ThrowHResult(const wchar_t* action, HRESULT hr) {
    throw WinException(action, static_cast<DWORD>(hr));
}

void ThrowNCrypt(const wchar_t* action, SECURITY_STATUS status) {
    throw WinException(action, static_cast<DWORD>(status));
}

}  // namespace testsign
