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

std::wstring ParentDirectory(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos) {
        return {};
    }
    if (slash == 0) {
        return path.substr(0, 1);
    }
    if (slash == 2 && path[1] == L':') {
        return path.substr(0, 3);
    }
    return path.substr(0, slash);
}

std::wstring FileName(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

std::wstring JoinPath(const std::wstring& directory, const std::wstring& name) {
    if (directory.empty()) {
        return name;
    }
    const wchar_t last = directory.back();
    if (last == L'\\' || last == L'/') {
        return directory + name;
    }
    return directory + L'\\' + name;
}

std::wstring CanonicalPath(const std::wstring& path) {
    const DWORD needed = ::GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (needed == 0) {
        return path;
    }
    std::wstring full(needed, L'\0');
    const DWORD written = ::GetFullPathNameW(path.c_str(), needed, full.data(), nullptr);
    if (written == 0 || written >= needed) {
        return path;
    }
    full.resize(written);
    return full;
}

bool SamePath(const std::wstring& left, const std::wstring& right) {
    return ::CompareStringOrdinal(CanonicalPath(left).c_str(), -1, CanonicalPath(right).c_str(), -1,
                                  TRUE) == CSTR_EQUAL;
}

std::wstring ReadTextFile(const std::wstring& path) {
    const std::vector<BYTE> bytes = ReadAllBytes(path);
    if (bytes.size() >= 2 && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        const auto* wide = reinterpret_cast<const wchar_t*>(bytes.data() + 2);
        const size_t chars = (bytes.size() - 2) / sizeof(wchar_t);
        return std::wstring(wide, chars);
    }

    if (bytes.empty()) {
        return {};
    }
    UINT code_page = CP_ACP;
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        code_page = CP_UTF8;
    }
    const int offset = code_page == CP_UTF8 ? 3 : 0;
    const int chars = ::MultiByteToWideChar(code_page, 0,
                                            reinterpret_cast<const char*>(bytes.data() + offset),
                                            static_cast<int>(bytes.size() - offset), nullptr, 0);
    if (chars <= 0) {
        return {};
    }
    std::wstring text(static_cast<size_t>(chars), L'\0');
    ::MultiByteToWideChar(code_page, 0, reinterpret_cast<const char*>(bytes.data() + offset),
                          static_cast<int>(bytes.size() - offset), text.data(), chars);
    return text;
}

std::wstring FindWindowsKitTool(const wchar_t* name) {
    const wchar_t* const env_names[] = {L"ProgramFiles(x86)", L"ProgramFiles"};
    const wchar_t* const arches[] = {L"x86", L"x64"};
    std::wstring best;
    for (const wchar_t* env_name : env_names) {
        wchar_t root[MAX_PATH]{};
        const DWORD root_len = ::GetEnvironmentVariableW(env_name, root, MAX_PATH);
        if (root_len == 0 || root_len >= MAX_PATH) {
            continue;
        }
        const std::wstring bin = std::wstring(root) + L"\\Windows Kits\\10\\bin";
        const std::wstring glob = bin + L"\\*";
        WIN32_FIND_DATAW data{};
        UniqueFindHandle find(::FindFirstFileW(glob.c_str(), &data));
        if (!find) {
            continue;
        }
        do {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                continue;
            }
            if (data.cFileName[0] == L'.') {
                continue;
            }
            for (const wchar_t* arch : arches) {
                const std::wstring candidate =
                    bin + L'\\' + data.cFileName + L'\\' + arch + L'\\' + name;
                if (::GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES &&
                    candidate > best) {
                    best = candidate;
                }
            }
        } while (::FindNextFileW(find.get(), &data));
    }
    return best;
}

namespace {

class UniqueProcThreadAttributes {
public:
    UniqueProcThreadAttributes() {
        SIZE_T bytes = 0;
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        if (bytes == 0) {
            return;
        }
        buffer_.resize(bytes);
        auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer_.data());
        if (!::InitializeProcThreadAttributeList(list, 1, 0, &bytes)) {
            buffer_.clear();
            return;
        }
        list_ = list;
    }

    ~UniqueProcThreadAttributes() {
        if (list_ != nullptr) {
            ::DeleteProcThreadAttributeList(list_);
        }
    }

    UniqueProcThreadAttributes(const UniqueProcThreadAttributes&) = delete;
    UniqueProcThreadAttributes& operator=(const UniqueProcThreadAttributes&) = delete;

    [[nodiscard]] LPPROC_THREAD_ATTRIBUTE_LIST get() const noexcept { return list_; }
    [[nodiscard]] explicit operator bool() const noexcept { return list_ != nullptr; }

private:
    std::vector<unsigned char> buffer_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_{nullptr};
};

class ThreadJoiner {
public:
    explicit ThreadJoiner(std::thread&& thread) : thread_(std::move(thread)) {}
    ~ThreadJoiner() {
        if (thread_.joinable()) {
            thread_.join();
        }
    }

    ThreadJoiner(const ThreadJoiner&) = delete;
    ThreadJoiner& operator=(const ThreadJoiner&) = delete;

private:
    std::thread thread_;
};

void AssignKillOnCloseJob(HANDLE job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    ::SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
}

void BytesToOutput(const std::string& bytes, std::wstring& output) {
    if (bytes.empty()) {
        return;
    }
    const int chars = ::MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()),
                                            nullptr, 0);
    if (chars > 0) {
        output.assign(static_cast<size_t>(chars), L'\0');
        ::MultiByteToWideChar(CP_ACP, 0, bytes.data(), static_cast<int>(bytes.size()), output.data(),
                              chars);
    }
}

}  // namespace

DWORD RunHiddenCommand(const std::wstring& command_line, const std::wstring& cwd,
                       std::wstring& output, DWORD timeout_ms) {
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;
    HANDLE raw_read = nullptr;
    HANDLE raw_write = nullptr;
    if (!::CreatePipe(&raw_read, &raw_write, &security, 0)) {
        ThrowLastError(Tr(Str::CreateProcessFailed));
    }
    UniqueHandle reader(raw_read);
    UniqueHandle writer(raw_write);
    if (!::SetHandleInformation(reader.get(), HANDLE_FLAG_INHERIT, 0)) {
        ThrowLastError(Tr(Str::CreateProcessFailed));
    }

    UniqueHandle nul(::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &security, OPEN_EXISTING, 0, nullptr));
    if (!nul) {
        ThrowLastError(Tr(Str::CreateProcessFailed));
    }

    UniqueHandle job(::CreateJobObjectW(nullptr, nullptr));
    if (job) {
        AssignKillOnCloseJob(job.get());
    }

    UniqueProcThreadAttributes attributes;
    HANDLE inherit_handles[2] = {nul.get(), writer.get()};
    const bool use_attribute_list =
        attributes &&
        ::UpdateProcThreadAttribute(attributes.get(), 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                    inherit_handles, sizeof(inherit_handles), nullptr, nullptr);

    STARTUPINFOEXW startup_ex{};
    startup_ex.StartupInfo.cb = use_attribute_list ? sizeof(startup_ex) : sizeof(STARTUPINFOW);
    startup_ex.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup_ex.StartupInfo.wShowWindow = SW_HIDE;
    startup_ex.StartupInfo.hStdInput = nul.get();
    startup_ex.StartupInfo.hStdOutput = writer.get();
    startup_ex.StartupInfo.hStdError = writer.get();
    if (use_attribute_list) {
        startup_ex.lpAttributeList = attributes.get();
    }

    std::wstring mutable_command = command_line;
    PROCESS_INFORMATION info{};
    DWORD creation_flags = CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;
    if (use_attribute_list) {
        creation_flags |= EXTENDED_STARTUPINFO_PRESENT;
    }
    if (!::CreateProcessW(nullptr, mutable_command.data(), nullptr, nullptr, TRUE, creation_flags,
                          nullptr, cwd.empty() ? nullptr : cwd.c_str(), &startup_ex.StartupInfo,
                          &info)) {
        ThrowLastError(Tr(Str::CreateProcessFailed));
    }
    UniqueHandle process(info.hProcess);
    UniqueHandle thread(info.hThread);
    writer.reset();
    nul.reset();

    bool in_job = false;
    if (job) {
        in_job = ::AssignProcessToJobObject(job.get(), process.get()) != FALSE;
    }
    if (::ResumeThread(thread.get()) == static_cast<DWORD>(-1)) {
        const DWORD error = ::GetLastError();
        ::TerminateProcess(process.get(), error);
        ThrowLastError(Tr(Str::CreateProcessFailed), error);
    }

    std::string bytes;
    const HANDLE read_handle = reader.get();
    const DWORD wait = [&]() -> DWORD {
        ThreadJoiner reader_thread(std::thread([read_handle, &bytes]() {
            char buffer[4096]{};
            DWORD read = 0;
            while (::ReadFile(read_handle, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
                bytes.append(buffer, read);
            }
        }));

        const DWORD result = ::WaitForSingleObject(process.get(), timeout_ms);
        if (result == WAIT_TIMEOUT) {
            if (in_job) {
                ::TerminateJobObject(job.get(), ERROR_TIMEOUT);
            } else {
                ::TerminateProcess(process.get(), ERROR_TIMEOUT);
            }
            ::WaitForSingleObject(process.get(), 5000);
        } else if (in_job) {
            DWORD exit_code = 0;
            ::GetExitCodeProcess(process.get(), &exit_code);
            ::TerminateJobObject(job.get(), exit_code);
        }
        reader.reset();
        return result;
    }();

    BytesToOutput(bytes, output);
    if (wait == WAIT_TIMEOUT) {
        throw WinException(Tr(Str::ProcessTimedOut), ERROR_TIMEOUT);
    }

    DWORD exit_code = 0;
    ::GetExitCodeProcess(process.get(), &exit_code);
    return exit_code;
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
