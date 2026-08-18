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

void SignDriver(const std::wstring& pfx_path, const std::wstring& sys_path) {
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
    file_info.pwszFileName = sys_path.c_str();
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

}  // namespace testsign
