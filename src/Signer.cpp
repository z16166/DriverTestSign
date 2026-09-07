#include "pch.h"
#include "Signer.h"
#include "Certificate.h"
#include "Raii.h"
#include "Strings.h"
#include "WinUtil.h"

namespace testsign {
namespace {

constexpr DWORD kSignerSubjectFile = 0x01;
constexpr DWORD kSignerCertStore = 0x02;
constexpr DWORD kSignerAuthcodeAttr = 0x01;
constexpr DWORD kSignerCertPolicyStore = 0x0001;
constexpr DWORD kSignerTimestampRfc3161 = 0x02;

struct SignerFileInfo {
    DWORD cbSize;
    LPCWSTR pwszFileName;
    HANDLE hFile;
};

struct SignerBlobInfo {
    DWORD cbSize;
    GUID* pGuidSubject;
    DWORD cbBlob;
    BYTE* pbBlob;
    LPCWSTR pwszDisplayName;
};

struct SignerSubjectInfo {
    DWORD cbSize;
    DWORD* pdwIndex;
    DWORD dwSubjectChoice;
    union {
        SignerFileInfo* pSignerFileInfo;
        SignerBlobInfo* pSignerBlobInfo;
    };
};

struct SignerCertStoreInfo {
    DWORD cbSize;
    PCCERT_CONTEXT pSigningCert;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
};

struct SignerSpcChainInfo {
    DWORD cbSize;
    LPCWSTR pwszSpcFile;
    DWORD dwCertPolicy;
    HCERTSTORE hCertStore;
};

struct SignerCert {
    DWORD cbSize;
    DWORD dwCertChoice;
    union {
        LPCWSTR pwszSpcFile;
        SignerCertStoreInfo* pCertStoreInfo;
        SignerSpcChainInfo* pSpcChainInfo;
    };
    HWND hwnd;
};

struct SignerAttrAuthcode {
    DWORD cbSize;
    BOOL fCommercial;
    BOOL fIndividual;
    LPCWSTR pwszName;
    LPCWSTR pwszInfo;
};

struct SignerSignatureInfo {
    DWORD cbSize;
    ALG_ID algidHash;
    DWORD dwAttrChoice;
    union {
        SignerAttrAuthcode* pAttrAuthcode;
    };
    PCRYPT_ATTRIBUTES psAuthenticated;
    PCRYPT_ATTRIBUTES psUnauthenticated;
};

struct SignerContext {
    DWORD cbSize;
    DWORD cbBlob;
    BYTE* pbBlob;
};

using SignerSignEx2Fn = HRESULT(WINAPI*)(DWORD, SignerSubjectInfo*, SignerCert*, SignerSignatureInfo*,
                                         void*, DWORD, PCSTR, PCWSTR, PCRYPT_ATTRIBUTES, void*,
                                         SignerContext**, void*, void*);
using SignerFreeSignerContextFn = HRESULT(WINAPI*)(SignerContext*);

class UniqueSignerContext {
public:
    UniqueSignerContext(SignerContext* context, SignerFreeSignerContextFn free_fn)
        : context_(context), free_fn_(free_fn) {}

    ~UniqueSignerContext() {
        if (context_ != nullptr && free_fn_ != nullptr) {
            free_fn_(context_);
        }
    }

    UniqueSignerContext(const UniqueSignerContext&) = delete;
    UniqueSignerContext& operator=(const UniqueSignerContext&) = delete;

private:
    SignerContext* context_{nullptr};
    SignerFreeSignerContextFn free_fn_{nullptr};
};

HRESULT SignWithTimestamp(SignerSignEx2Fn sign, SignerFreeSignerContextFn free_fn,
                          SignerSubjectInfo* subject, SignerCert* cert,
                          SignerSignatureInfo* signature, const wchar_t* timestamp_url) {
    SignerContext* raw_context = nullptr;
    const HRESULT hr = sign(0, subject, cert, signature, nullptr, kSignerTimestampRfc3161,
                            szOID_NIST_sha256, timestamp_url, nullptr, nullptr, &raw_context,
                            nullptr, nullptr);
    UniqueSignerContext context(raw_context, free_fn);
    return hr;
}

}  // namespace

void SignAuthenticodeFile(const std::wstring& pfx_path, const std::wstring& path) {
    UniqueModule mssign(::LoadLibraryExW(L"mssign32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
    if (!mssign) {
        ThrowLastError(Tr(Str::LoadMssignFailed));
    }

    const auto sign = reinterpret_cast<SignerSignEx2Fn>(
        ::GetProcAddress(mssign.get(), "SignerSignEx2"));
    const auto free_fn = reinterpret_cast<SignerFreeSignerContextFn>(
        ::GetProcAddress(mssign.get(), "SignerFreeSignerContext"));
    if (sign == nullptr || free_fn == nullptr) {
        ThrowLastError(Tr(Str::SignerSignEx2Missing), ERROR_PROC_NOT_FOUND);
    }

    const PfxSession session(pfx_path);

    SignerFileInfo file_info{};
    file_info.cbSize = sizeof(file_info);
    file_info.pwszFileName = path.c_str();
    file_info.hFile = nullptr;

    DWORD index = 0;
    SignerSubjectInfo subject{};
    subject.cbSize = sizeof(subject);
    subject.pdwIndex = &index;
    subject.dwSubjectChoice = kSignerSubjectFile;
    subject.pSignerFileInfo = &file_info;

    SignerCertStoreInfo store_info{};
    store_info.cbSize = sizeof(store_info);
    store_info.pSigningCert = session.certificate();
    store_info.dwCertPolicy = kSignerCertPolicyStore;
    store_info.hCertStore = session.store();

    SignerCert signer_cert{};
    signer_cert.cbSize = sizeof(signer_cert);
    signer_cert.dwCertChoice = kSignerCertStore;
    signer_cert.pCertStoreInfo = &store_info;
    signer_cert.hwnd = nullptr;

    SignerAttrAuthcode auth{};
    auth.cbSize = sizeof(auth);
    auth.fCommercial = FALSE;
    auth.fIndividual = TRUE;
    auth.pwszName = L"";
    auth.pwszInfo = L"";

    SignerSignatureInfo signature{};
    signature.cbSize = sizeof(signature);
    signature.algidHash = CALG_SHA_256;
    signature.dwAttrChoice = kSignerAuthcodeAttr;
    signature.pAttrAuthcode = &auth;

    HRESULT hr = SignWithTimestamp(sign, free_fn, &subject, &signer_cert, &signature, kTimestampUrl);
    if (FAILED(hr)) {
        hr = SignWithTimestamp(sign, free_fn, &subject, &signer_cert, &signature,
                               kTimestampUrlFallback);
    }
    if (FAILED(hr)) {
        ThrowHResult(Tr(Str::SignTimestampFailed), hr);
    }
}

std::wstring InfDirective(const std::wstring& text, const wchar_t* key) {
    std::wstring line;
    for (wchar_t ch : text) {
        if (ch == L'\r') {
            continue;
        }
        if (ch != L'\n') {
            line.push_back(ch);
            continue;
        }
        const std::wstring trimmed = Trim(std::move(line));
        line.clear();
        if (trimmed.empty() || trimmed[0] == L';') {
            continue;
        }
        const auto eq = trimmed.find(L'=');
        if (eq == std::wstring::npos) {
            continue;
        }
        std::wstring name = Trim(trimmed.substr(0, eq));
        if (::_wcsicmp(name.c_str(), key) != 0) {
            continue;
        }
        std::wstring value = Trim(trimmed.substr(eq + 1));
        if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
            value = value.substr(1, value.size() - 2);
        }
        return value;
    }
    if (!line.empty()) {
        const std::wstring trimmed = Trim(std::move(line));
        if (!trimmed.empty() && trimmed[0] != L';') {
            const auto eq = trimmed.find(L'=');
            if (eq != std::wstring::npos) {
                std::wstring name = Trim(trimmed.substr(0, eq));
                if (::_wcsicmp(name.c_str(), key) == 0) {
                    std::wstring value = Trim(trimmed.substr(eq + 1));
                    if (value.size() >= 2 && value.front() == L'"' && value.back() == L'"') {
                        value = value.substr(1, value.size() - 2);
                    }
                    return value;
                }
            }
        }
    }
    return {};
}

std::wstring CatalogPathForInf(const std::wstring& inf_path) {
    const std::wstring directory = ParentDirectory(inf_path);
    std::wstring catalog = InfDirective(ReadTextFile(inf_path), L"CatalogFile");
    if (catalog.empty()) {
        std::wstring stem = FileName(inf_path);
        const auto dot = stem.find_last_of(L'.');
        if (dot != std::wstring::npos) {
            stem.resize(dot);
        }
        catalog = stem + L".cat";
    }
    return JoinPath(directory, catalog);
}

void SignDriver(const std::wstring& pfx_path, const std::wstring& sys_path) {
    SignAuthenticodeFile(pfx_path, sys_path);
}

std::wstring SignCatalog(const std::wstring& pfx_path, const std::wstring& inf_path,
                         const std::wstring& sys_path) {
    const std::wstring inf2cat = FindWindowsKitTool(L"inf2cat.exe");
    if (inf2cat.empty()) {
        throw WinException(Tr(Str::Inf2CatMissing), ERROR_FILE_NOT_FOUND);
    }

    const std::wstring inf_dir = ParentDirectory(inf_path);
    if (!sys_path.empty()) {
        SignAuthenticodeFile(pfx_path, sys_path);
        const std::wstring staged = JoinPath(inf_dir, FileName(sys_path));
        if (!SamePath(sys_path, staged)) {
            if (!::CopyFileW(sys_path.c_str(), staged.c_str(), FALSE)) {
                ThrowLastError(Tr(Str::CopyFileFailed));
            }
        }
    }

    const std::wstring command = L"\"" + inf2cat + L"\" /driver:\"" + inf_dir + L"\" /os:10_X64";
    std::wstring output;
    const DWORD exit_code = RunHiddenCommand(command, inf_dir, output);
    if (exit_code != 0) {
        std::wstring message = Tr(Str::Inf2CatFailed);
        message += Tr(Str::ErrorJoin);
        message += output.empty() ? FormatWin32Message(exit_code) : output;
        throw WinException(std::move(message), exit_code);
    }

    const std::wstring catalog = CatalogPathForInf(inf_path);
    if (::GetFileAttributesW(catalog.c_str()) == INVALID_FILE_ATTRIBUTES) {
        throw WinException(Tr(Str::CatalogNotCreated), ERROR_FILE_NOT_FOUND);
    }
    SignAuthenticodeFile(pfx_path, catalog);
    return catalog;
}

}  // namespace testsign
