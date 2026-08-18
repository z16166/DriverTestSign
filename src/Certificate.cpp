#include "pch.h"
#include "Certificate.h"
#include "Raii.h"
#include "Strings.h"
#include "WinUtil.h"

namespace testsign {
namespace {

constexpr DWORD kRsaKeyBits = 2048;
constexpr DWORD kPfxExportFlags =
    EXPORT_PRIVATE_KEYS | PKCS12_INCLUDE_EXTENDED_PROPERTIES | PKCS12_EXPORT_SILENT;
constexpr DWORD kPfxImportFlags =
    CRYPT_EXPORTABLE | PKCS12_NO_PERSIST_KEY | PKCS12_ALWAYS_CNG_KSP | PKCS12_IMPORT_SILENT;

struct SoftwareCsp {
    const wchar_t* name;
    DWORD type;
};

constexpr SoftwareCsp kSoftwareCsps[] = {
    {MS_ENH_RSA_AES_PROV, PROV_RSA_AES},
    {MS_ENHANCED_PROV, PROV_RSA_FULL},
};

class SoftwareKeyContainer {
public:
    explicit SoftwareKeyContainer(std::wstring container) : container_(std::move(container)) {
        HCRYPTPROV raw_provider = 0;
        for (const SoftwareCsp& csp : kSoftwareCsps) {
            if (::CryptAcquireContextW(&raw_provider, container_.c_str(), csp.name, csp.type,
                                       CRYPT_NEWKEYSET | CRYPT_SILENT)) {
                provider_.reset(raw_provider);
                provider_name_ = csp.name;
                provider_type_ = csp.type;
                break;
            }
        }
        if (!provider_) {
            ThrowLastError(Tr(Str::OpenSoftCspFailed));
        }

        HCRYPTKEY raw_key = 0;
        const DWORD flags = (kRsaKeyBits << 16) | CRYPT_EXPORTABLE;
        if (!::CryptGenKey(provider_.get(), AT_SIGNATURE, flags, &raw_key)) {
            ThrowLastError(Tr(Str::GenerateSoftRsaFailed));
        }
        key_.reset(raw_key);
    }

    ~SoftwareKeyContainer() {
        key_.reset();
        provider_.reset();
        if (!container_.empty() && provider_name_ != nullptr) {
            HCRYPTPROV unused = 0;
            ::CryptAcquireContextW(&unused, container_.c_str(), provider_name_, provider_type_,
                                   CRYPT_DELETEKEYSET | CRYPT_SILENT);
        }
    }

    SoftwareKeyContainer(const SoftwareKeyContainer&) = delete;
    SoftwareKeyContainer& operator=(const SoftwareKeyContainer&) = delete;

    [[nodiscard]] HCRYPTPROV provider() const noexcept { return provider_.get(); }
    [[nodiscard]] const wchar_t* container() const noexcept { return container_.c_str(); }
    [[nodiscard]] const wchar_t* provider_name() const noexcept { return provider_name_; }
    [[nodiscard]] DWORD provider_type() const noexcept { return provider_type_; }

private:
    std::wstring container_;
    const wchar_t* provider_name_{nullptr};
    DWORD provider_type_{0};
    UniqueCryptProvider provider_;
    UniqueCryptKey key_;
};

std::wstring MakeContainerName() {
    GUID guid{};
    const HRESULT hr = ::CoCreateGuid(&guid);
    if (FAILED(hr)) {
        ThrowHResult(Tr(Str::GenerateKeyContainerFailed), hr);
    }

    wchar_t name[80]{};
    swprintf_s(name, L"TestSign-%08lX-%04X-%04X-%02X%02X%02X%02X",
               static_cast<unsigned long>(guid.Data1), guid.Data2, guid.Data3, guid.Data4[0],
               guid.Data4[1], guid.Data4[2], guid.Data4[3]);
    return name;
}

std::vector<BYTE> EncodeSubjectName(const wchar_t* subject) {
    DWORD bytes = 0;
    if (!::CertStrToNameW(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, nullptr, nullptr, &bytes,
                          nullptr)) {
        ThrowLastError(Tr(Str::EncodeSubjectFailed));
    }

    std::vector<BYTE> name(bytes);
    if (!::CertStrToNameW(X509_ASN_ENCODING, subject, CERT_X500_NAME_STR, nullptr, name.data(),
                          &bytes, nullptr)) {
        ThrowLastError(Tr(Str::EncodeSubjectFailed));
    }
    name.resize(bytes);
    return name;
}

UniqueLocal<BYTE> EncodeObject(LPCSTR struct_type, const void* struct_info, DWORD& bytes) {
    void* encoded = nullptr;
    bytes = 0;
    if (!::CryptEncodeObjectEx(X509_ASN_ENCODING, struct_type, struct_info, CRYPT_ENCODE_ALLOC_FLAG,
                               nullptr, &encoded, &bytes)) {
        ThrowLastError(Tr(Str::EncodeExtensionFailed));
    }
    return UniqueLocal<BYTE>(static_cast<BYTE*>(encoded));
}

UniqueCertStore ImportPfxStore(const std::wstring& pfx_path) {
    std::vector<BYTE> bytes = ReadAllBytes(pfx_path);
    CRYPT_DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(bytes.size());
    blob.pbData = bytes.data();

    UniqueCertStore store(::PFXImportCertStore(&blob, L"", kPfxImportFlags));
    if (!store) {
        store.reset(::PFXImportCertStore(&blob, nullptr, kPfxImportFlags));
    }
    if (!store) {
        store.reset(::PFXImportCertStore(&blob, L"", CRYPT_EXPORTABLE | PKCS12_ALWAYS_CNG_KSP));
    }
    if (!store) {
        ThrowLastError(Tr(Str::ImportPfxFailed));
    }
    return store;
}

UniqueCertContext FindSigningCertificate(HCERTSTORE store) {
    UniqueCertContext context;
    while (context.Enumerate(store)) {
        DWORD size = 0;
        if (::CertGetCertificateContextProperty(context.get(), CERT_KEY_PROV_INFO_PROP_ID, nullptr,
                                                &size) ||
            ::CertGetCertificateContextProperty(context.get(), CERT_KEY_CONTEXT_PROP_ID, nullptr,
                                                &size)) {
            return UniqueCertContext(::CertDuplicateCertificateContext(context.get()));
        }
    }
    throw WinException(Tr(Str::NoCodeSigningCert), ERROR_NOT_FOUND);
}

void AddCertificateToLocalMachineStore(PCCERT_CONTEXT context, const wchar_t* store_name) {
    UniqueCertStore store(::CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0,
                                          CERT_SYSTEM_STORE_LOCAL_MACHINE, store_name));
    if (!store) {
        ThrowLastError(Tr(Str::OpenMachineStoreFailed));
    }

    if (!::CertAddCertificateContextToStore(store.get(), context, CERT_STORE_ADD_REPLACE_EXISTING,
                                            nullptr)) {
        ThrowLastError(Tr(Str::WriteMachineStoreFailed));
    }
}

}  // namespace

void GenerateKeyPair(const std::wstring& pfx_path) {
    SoftwareKeyContainer key_container(MakeContainerName());

    std::vector<BYTE> subject_bytes = EncodeSubjectName(kCertificateSubject);
    CERT_NAME_BLOB subject{};
    subject.cbData = static_cast<DWORD>(subject_bytes.size());
    subject.pbData = subject_bytes.data();

    CERT_ENHKEY_USAGE eku{};
    LPSTR usages[] = {const_cast<LPSTR>(szOID_PKIX_KP_CODE_SIGNING)};
    eku.cUsageIdentifier = 1;
    eku.rgpszUsageIdentifier = usages;
    DWORD eku_bytes = 0;
    UniqueLocal<BYTE> eku_encoded = EncodeObject(X509_ENHANCED_KEY_USAGE, &eku, eku_bytes);

    CRYPT_BIT_BLOB key_usage{};
    BYTE key_usage_bits = CERT_DIGITAL_SIGNATURE_KEY_USAGE;
    key_usage.cbData = sizeof(key_usage_bits);
    key_usage.pbData = &key_usage_bits;
    key_usage.cUnusedBits = 7;
    DWORD usage_bytes = 0;
    UniqueLocal<BYTE> usage_encoded = EncodeObject(X509_KEY_USAGE, &key_usage, usage_bytes);

    CERT_EXTENSION extensions[2]{};
    extensions[0].pszObjId = const_cast<char*>(szOID_ENHANCED_KEY_USAGE);
    extensions[0].fCritical = TRUE;
    extensions[0].Value.cbData = eku_bytes;
    extensions[0].Value.pbData = eku_encoded.get();

    extensions[1].pszObjId = const_cast<char*>(szOID_KEY_USAGE);
    extensions[1].fCritical = TRUE;
    extensions[1].Value.cbData = usage_bytes;
    extensions[1].Value.pbData = usage_encoded.get();

    CERT_EXTENSIONS extension_list{};
    extension_list.cExtension = 2;
    extension_list.rgExtension = extensions;

    CRYPT_ALGORITHM_IDENTIFIER signature_algorithm{};
    signature_algorithm.pszObjId = const_cast<char*>(szOID_RSA_SHA256RSA);

    SYSTEMTIME start{};
    ::GetSystemTime(&start);
    SYSTEMTIME expire = start;
    expire.wYear = static_cast<WORD>(expire.wYear + 10);

    CRYPT_KEY_PROV_INFO key_info{};
    key_info.pwszContainerName = const_cast<wchar_t*>(key_container.container());
    key_info.pwszProvName = const_cast<wchar_t*>(key_container.provider_name());
    key_info.dwProvType = key_container.provider_type();
    key_info.dwFlags = CRYPT_SILENT;
    key_info.dwKeySpec = AT_SIGNATURE;

    UniqueCertContext cert(::CertCreateSelfSignCertificate(
        key_container.provider(), &subject, 0, &key_info, &signature_algorithm, &start, &expire,
        &extension_list));
    if (!cert) {
        ThrowLastError(Tr(Str::CreateSelfSignedFailed));
    }

    std::wstring friendly_name = L"TestSign Kernel Test CA";
    CRYPT_DATA_BLOB friendly_name_blob{};
    friendly_name_blob.cbData = static_cast<DWORD>((friendly_name.size() + 1) * sizeof(wchar_t));
    friendly_name_blob.pbData = reinterpret_cast<BYTE*>(friendly_name.data());
    if (!::CertSetCertificateContextProperty(cert.get(), CERT_FRIENDLY_NAME_PROP_ID, 0,
                                             &friendly_name_blob)) {
        ThrowLastError(Tr(Str::SetFriendlyNameFailed));
    }

    UniqueCertStore memory_store(::CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, nullptr));
    if (!memory_store) {
        ThrowLastError(Tr(Str::CreateMemoryStoreFailed));
    }
    if (!::CertAddCertificateContextToStore(memory_store.get(), cert.get(), CERT_STORE_ADD_ALWAYS,
                                            nullptr)) {
        ThrowLastError(Tr(Str::AddCertToMemoryFailed));
    }

    CRYPT_DATA_BLOB pfx{};
    if (!::PFXExportCertStoreEx(memory_store.get(), &pfx, L"", nullptr, kPfxExportFlags) &&
        ::GetLastError() != ERROR_MORE_DATA) {
        ThrowLastError(Tr(Str::ExportPfxSizeFailed));
    }
    if (pfx.cbData == 0) {
        ThrowLastError(Tr(Str::ExportPfxSizeFailed), ERROR_INVALID_DATA);
    }

    std::vector<BYTE> pfx_bytes(pfx.cbData);
    pfx.pbData = pfx_bytes.data();
    if (!::PFXExportCertStoreEx(memory_store.get(), &pfx, L"", nullptr, kPfxExportFlags)) {
        ThrowLastError(Tr(Str::ExportPfxFailed));
    }
    pfx_bytes.resize(pfx.cbData);

    WriteAllBytes(pfx_path, pfx_bytes);
}

PfxSession::PfxSession(const std::wstring& pfx_path)
    : store_(ImportPfxStore(pfx_path)), certificate_(FindSigningCertificate(store_.get())) {}

void ImportPublicKeyToLocalMachineRoot(const std::wstring& pfx_path) {
    const PfxSession session(pfx_path);
    AddCertificateToLocalMachineStore(session.certificate(), L"ROOT");
    AddCertificateToLocalMachineStore(session.certificate(), L"TrustedPublisher");
}

}  // namespace testsign
