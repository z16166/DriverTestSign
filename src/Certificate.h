#pragma once

#include "pch.h"
#include "Raii.h"

namespace testsign {

inline constexpr wchar_t kCertificateSubject[] = L"CN=TestSign Kernel Test CA";

class PfxSession {
public:
    explicit PfxSession(const std::wstring& pfx_path);

    [[nodiscard]] HCERTSTORE store() const noexcept { return store_.get(); }
    [[nodiscard]] PCCERT_CONTEXT certificate() const noexcept { return certificate_.get(); }

private:
    UniqueCertStore store_;
    UniqueCertContext certificate_;
};

void GenerateKeyPair(const std::wstring& pfx_path);
void ImportPublicKeyToLocalMachineRoot(const std::wstring& pfx_path);

}  // namespace testsign
