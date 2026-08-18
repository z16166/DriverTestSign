#pragma once

#include "pch.h"

namespace testsign {

template <typename T>
class UniqueLocal {
public:
    UniqueLocal() = default;
    explicit UniqueLocal(T* ptr) noexcept : ptr_(ptr) {}
    ~UniqueLocal() { reset(); }

    UniqueLocal(const UniqueLocal&) = delete;
    UniqueLocal& operator=(const UniqueLocal&) = delete;

    UniqueLocal(UniqueLocal&& other) noexcept : ptr_(other.release()) {}
    UniqueLocal& operator=(UniqueLocal&& other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = other.release();
        }
        return *this;
    }

    void reset(T* ptr = nullptr) noexcept {
        if (ptr_ != nullptr) {
            ::LocalFree(ptr_);
        }
        ptr_ = ptr;
    }

    [[nodiscard]] T* release() noexcept {
        T* ptr = ptr_;
        ptr_ = nullptr;
        return ptr;
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }
    [[nodiscard]] T** put() noexcept {
        reset();
        return &ptr_;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    T* ptr_{nullptr};
};

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueHandle() { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : handle_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.release();
        }
        return *this;
    }

    void reset(HANDLE handle = INVALID_HANDLE_VALUE) noexcept {
        if (is_valid(handle_)) {
            ::CloseHandle(handle_);
        }
        handle_ = handle;
    }

    [[nodiscard]] HANDLE release() noexcept {
        HANDLE handle = handle_;
        handle_ = INVALID_HANDLE_VALUE;
        return handle;
    }

    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return is_valid(handle_); }

private:
    [[nodiscard]] static bool is_valid(HANDLE handle) noexcept {
        return handle != nullptr && handle != INVALID_HANDLE_VALUE;
    }

    HANDLE handle_{INVALID_HANDLE_VALUE};
};

class UniqueModule {
public:
    UniqueModule() = default;
    explicit UniqueModule(HMODULE module) noexcept : module_(module) {}
    ~UniqueModule() { reset(); }

    UniqueModule(const UniqueModule&) = delete;
    UniqueModule& operator=(const UniqueModule&) = delete;

    UniqueModule(UniqueModule&& other) noexcept : module_(other.release()) {}
    UniqueModule& operator=(UniqueModule&& other) noexcept {
        if (this != &other) {
            reset();
            module_ = other.release();
        }
        return *this;
    }

    void reset(HMODULE module = nullptr) noexcept {
        if (module_ != nullptr) {
            ::FreeLibrary(module_);
        }
        module_ = module;
    }

    [[nodiscard]] HMODULE release() noexcept {
        HMODULE module = module_;
        module_ = nullptr;
        return module;
    }

    [[nodiscard]] HMODULE get() const noexcept { return module_; }
    [[nodiscard]] explicit operator bool() const noexcept { return module_ != nullptr; }

private:
    HMODULE module_{nullptr};
};

class UniqueCertStore {
public:
    UniqueCertStore() = default;
    explicit UniqueCertStore(HCERTSTORE store) noexcept : store_(store) {}
    ~UniqueCertStore() { reset(); }

    UniqueCertStore(const UniqueCertStore&) = delete;
    UniqueCertStore& operator=(const UniqueCertStore&) = delete;

    UniqueCertStore(UniqueCertStore&& other) noexcept : store_(other.release()) {}
    UniqueCertStore& operator=(UniqueCertStore&& other) noexcept {
        if (this != &other) {
            reset();
            store_ = other.release();
        }
        return *this;
    }

    void reset(HCERTSTORE store = nullptr) noexcept {
        if (store_ != nullptr) {
            ::CertCloseStore(store_, 0);
        }
        store_ = store;
    }

    [[nodiscard]] HCERTSTORE release() noexcept {
        HCERTSTORE store = store_;
        store_ = nullptr;
        return store;
    }

    [[nodiscard]] HCERTSTORE get() const noexcept { return store_; }
    [[nodiscard]] explicit operator bool() const noexcept { return store_ != nullptr; }

private:
    HCERTSTORE store_{nullptr};
};

class UniqueCertContext {
public:
    UniqueCertContext() = default;
    explicit UniqueCertContext(PCCERT_CONTEXT context) noexcept : context_(context) {}
    ~UniqueCertContext() { reset(); }

    UniqueCertContext(const UniqueCertContext&) = delete;
    UniqueCertContext& operator=(const UniqueCertContext&) = delete;

    UniqueCertContext(UniqueCertContext&& other) noexcept : context_(other.release()) {}
    UniqueCertContext& operator=(UniqueCertContext&& other) noexcept {
        if (this != &other) {
            reset();
            context_ = other.release();
        }
        return *this;
    }

    void reset(PCCERT_CONTEXT context = nullptr) noexcept {
        if (context_ != nullptr) {
            ::CertFreeCertificateContext(context_);
        }
        context_ = context;
    }

    [[nodiscard]] PCCERT_CONTEXT release() noexcept {
        PCCERT_CONTEXT context = context_;
        context_ = nullptr;
        return context;
    }

    [[nodiscard]] PCCERT_CONTEXT get() const noexcept { return context_; }
    [[nodiscard]] explicit operator bool() const noexcept { return context_ != nullptr; }

    // CertEnumCertificatesInStore frees the previous context.
    bool Enumerate(HCERTSTORE store) {
        PCCERT_CONTEXT previous = release();
        reset(::CertEnumCertificatesInStore(store, previous));
        return context_ != nullptr;
    }

private:
    PCCERT_CONTEXT context_{nullptr};
};

class UniqueRegistryKey {
public:
    UniqueRegistryKey() = default;
    explicit UniqueRegistryKey(HKEY key) noexcept : key_(key) {}
    ~UniqueRegistryKey() { reset(); }

    UniqueRegistryKey(const UniqueRegistryKey&) = delete;
    UniqueRegistryKey& operator=(const UniqueRegistryKey&) = delete;

    UniqueRegistryKey(UniqueRegistryKey&& other) noexcept : key_(other.release()) {}
    UniqueRegistryKey& operator=(UniqueRegistryKey&& other) noexcept {
        if (this != &other) {
            reset();
            key_ = other.release();
        }
        return *this;
    }

    void reset(HKEY key = nullptr) noexcept {
        if (key_ != nullptr) {
            ::RegCloseKey(key_);
        }
        key_ = key;
    }

    [[nodiscard]] HKEY release() noexcept {
        HKEY key = key_;
        key_ = nullptr;
        return key;
    }

    [[nodiscard]] HKEY get() const noexcept { return key_; }
    [[nodiscard]] explicit operator bool() const noexcept { return key_ != nullptr; }

private:
    HKEY key_{nullptr};
};

class UniqueCryptProvider {
public:
    UniqueCryptProvider() = default;
    explicit UniqueCryptProvider(HCRYPTPROV provider) noexcept : provider_(provider) {}
    ~UniqueCryptProvider() { reset(); }

    UniqueCryptProvider(const UniqueCryptProvider&) = delete;
    UniqueCryptProvider& operator=(const UniqueCryptProvider&) = delete;

    UniqueCryptProvider(UniqueCryptProvider&& other) noexcept : provider_(other.release()) {}
    UniqueCryptProvider& operator=(UniqueCryptProvider&& other) noexcept {
        if (this != &other) {
            reset();
            provider_ = other.release();
        }
        return *this;
    }

    void reset(HCRYPTPROV provider = 0) noexcept {
        if (provider_ != 0) {
            ::CryptReleaseContext(provider_, 0);
        }
        provider_ = provider;
    }

    [[nodiscard]] HCRYPTPROV release() noexcept {
        HCRYPTPROV provider = provider_;
        provider_ = 0;
        return provider;
    }

    [[nodiscard]] HCRYPTPROV get() const noexcept { return provider_; }
    [[nodiscard]] explicit operator bool() const noexcept { return provider_ != 0; }

private:
    HCRYPTPROV provider_{0};
};

class UniqueCryptKey {
public:
    UniqueCryptKey() = default;
    explicit UniqueCryptKey(HCRYPTKEY key) noexcept : key_(key) {}
    ~UniqueCryptKey() { reset(); }

    UniqueCryptKey(const UniqueCryptKey&) = delete;
    UniqueCryptKey& operator=(const UniqueCryptKey&) = delete;

    UniqueCryptKey(UniqueCryptKey&& other) noexcept : key_(other.release()) {}
    UniqueCryptKey& operator=(UniqueCryptKey&& other) noexcept {
        if (this != &other) {
            reset();
            key_ = other.release();
        }
        return *this;
    }

    void reset(HCRYPTKEY key = 0) noexcept {
        if (key_ != 0) {
            ::CryptDestroyKey(key_);
        }
        key_ = key;
    }

    [[nodiscard]] HCRYPTKEY release() noexcept {
        HCRYPTKEY key = key_;
        key_ = 0;
        return key;
    }

    [[nodiscard]] HCRYPTKEY get() const noexcept { return key_; }
    [[nodiscard]] explicit operator bool() const noexcept { return key_ != 0; }

private:
    HCRYPTKEY key_{0};
};

class UniqueNCryptObject {
public:
    UniqueNCryptObject() = default;
    explicit UniqueNCryptObject(NCRYPT_HANDLE handle) noexcept : handle_(handle) {}
    ~UniqueNCryptObject() { reset(); }

    UniqueNCryptObject(const UniqueNCryptObject&) = delete;
    UniqueNCryptObject& operator=(const UniqueNCryptObject&) = delete;

    UniqueNCryptObject(UniqueNCryptObject&& other) noexcept : handle_(other.release()) {}
    UniqueNCryptObject& operator=(UniqueNCryptObject&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.release();
        }
        return *this;
    }

    void reset(NCRYPT_HANDLE handle = 0) noexcept {
        if (handle_ != 0) {
            ::NCryptFreeObject(handle_);
        }
        handle_ = handle;
    }

    [[nodiscard]] NCRYPT_HANDLE release() noexcept {
        NCRYPT_HANDLE handle = handle_;
        handle_ = 0;
        return handle;
    }

    [[nodiscard]] NCRYPT_HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }

private:
    NCRYPT_HANDLE handle_{0};
};

// Deletes the persisted CNG key on destruction so generation never leaks a container.
class UniqueNCryptKey {
public:
    UniqueNCryptKey() = default;
    explicit UniqueNCryptKey(NCRYPT_KEY_HANDLE key) noexcept : key_(key) {}
    ~UniqueNCryptKey() { reset(); }

    UniqueNCryptKey(const UniqueNCryptKey&) = delete;
    UniqueNCryptKey& operator=(const UniqueNCryptKey&) = delete;

    UniqueNCryptKey(UniqueNCryptKey&& other) noexcept
        : key_(other.release()), delete_on_destroy_(other.delete_on_destroy_) {}

    UniqueNCryptKey& operator=(UniqueNCryptKey&& other) noexcept {
        if (this != &other) {
            reset();
            key_ = other.release();
            delete_on_destroy_ = other.delete_on_destroy_;
        }
        return *this;
    }

    void reset(NCRYPT_KEY_HANDLE key = 0) noexcept {
        if (key_ != 0) {
            if (delete_on_destroy_) {
                ::NCryptDeleteKey(key_, 0);
            } else {
                ::NCryptFreeObject(key_);
            }
        }
        key_ = key;
        delete_on_destroy_ = true;
    }

    [[nodiscard]] NCRYPT_KEY_HANDLE release() noexcept {
        NCRYPT_KEY_HANDLE key = key_;
        key_ = 0;
        return key;
    }

    [[nodiscard]] NCRYPT_KEY_HANDLE get() const noexcept { return key_; }
    [[nodiscard]] explicit operator bool() const noexcept { return key_ != 0; }

private:
    NCRYPT_KEY_HANDLE key_{0};
    bool delete_on_destroy_{true};
};

class ComInitializer {
public:
    ComInitializer() : hr_(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComInitializer() {
        if (SUCCEEDED(hr_)) {
            ::CoUninitialize();
        }
    }

    ComInitializer(const ComInitializer&) = delete;
    ComInitializer& operator=(const ComInitializer&) = delete;

    [[nodiscard]] HRESULT result() const noexcept { return hr_; }

private:
    HRESULT hr_;
};

class AppModuleGuard {
public:
    AppModuleGuard(CAppModule& module, HINSTANCE instance)
        : module_(module), initialized_(SUCCEEDED(module.Init(nullptr, instance))) {}

    ~AppModuleGuard() {
        if (initialized_) {
            module_.Term();
        }
    }

    AppModuleGuard(const AppModuleGuard&) = delete;
    AppModuleGuard& operator=(const AppModuleGuard&) = delete;

    [[nodiscard]] bool initialized() const noexcept { return initialized_; }

private:
    CAppModule& module_;
    bool initialized_;
};

}  // namespace testsign
