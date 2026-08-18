#include "pch.h"
#include "MainDlg.h"
#include "Certificate.h"
#include "Settings.h"
#include "Signer.h"
#include "WinUtil.h"

namespace {

void PostOperationResult(HWND hwnd, std::unique_ptr<CMainDlg::OperationResult> result) {
    CMainDlg::OperationResult* raw = result.release();
    if (!::PostMessageW(hwnd, CMainDlg::WM_APP_OPERATION_DONE, 0, reinterpret_cast<LPARAM>(raw))) {
        std::unique_ptr<CMainDlg::OperationResult> discarded(raw);
    }
}

}  // namespace

LRESULT CMainDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    CenterWindow();
    SetIcon(::LoadIconW(nullptr, IDI_SHIELD), TRUE);
    SetIcon(::LoadIconW(nullptr, IDI_SHIELD), FALSE);
    SetBusy(true);

    AppendStatus(L"正在读取上次使用的路径...");
    RunAsync(OperationKind::LoadSettings, []() {
        const testsign::Settings::Paths paths = testsign::Settings::LoadAll();
        OperationResult result;
        result.success = true;
        result.key_path = paths.key;
        result.sys_path = paths.sys;
        return result;
    });
    return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
    return 0;
}

LRESULT CMainDlg::OnWmClose(UINT, WPARAM, LPARAM, BOOL&) {
    RequestClose();
    return 0;
}

LRESULT CMainDlg::OnOperationDone(UINT, WPARAM, LPARAM lparam, BOOL&) {
    std::unique_ptr<OperationResult> result(reinterpret_cast<OperationResult*>(lparam));
    if (worker_.joinable()) {
        worker_.join();
    }

    const bool close_after = result && result->success && result->close_after;
    if (!close_after) {
        SetBusy(false);
    }

    if (!result) {
        return 0;
    }

    if (!result->success) {
        AppendStatus(std::wstring(L"失败：") + result->message);
        return 0;
    }

    switch (result->kind) {
    case OperationKind::LoadSettings:
        SetText(IDC_KEY_PATH, result->key_path);
        SetText(IDC_SYS_PATH, result->sys_path);
        AppendStatus(L"就绪。证书主题：CN=TestSign Kernel Test CA");
        AppendStatus(L"时间戳服务器：http://timestamp.digicert.com （失败时回退 Sectigo）");
        AppendStatus(L"导入本机根证书与签名驱动需要管理员权限。");
        break;
    case OperationKind::Persist:
        if (result->close_after) {
            EndDialog(IDCANCEL);
        }
        break;
    case OperationKind::Generate:
        SetText(IDC_KEY_PATH, result->key_path);
        AppendStatus(std::wstring(L"已生成空密码的 RSA-2048 / SHA256 测试证书（CN=TestSign Kernel Test CA）：") +
                     result->key_path);
        break;
    case OperationKind::Import:
        AppendStatus(L"已将公钥导入本机“受信任的根证书颁发机构”和“受信任的发布者”。验证测试签名仍需开启 testsigning。");
        break;
    case OperationKind::Sign:
        AppendStatus(std::wstring(L"已使用 SHA256 签名：") + result->sys_path);
        AppendStatus(L"加载该驱动前请确认本机已开启测试签名（bcdedit /set testsigning on）。");
        break;
    }
    return 0;
}

LRESULT CMainDlg::OnBrowseKey(WORD, WORD, HWND, BOOL&) {
    if (busy_) {
        return 0;
    }

    SetBusy(true);
    std::wstring path;
    if (!ChooseFile(true, L"pfx", nullptr,
                    L"PFX 密钥文件 (*.pfx)\0*.pfx\0所有文件 (*.*)\0*.*\0", path)) {
        SetBusy(false);
        return 0;
    }

    SetText(IDC_KEY_PATH, path);
    AppendStatus(std::wstring(L"已选择密钥：") + path);
    PersistAsync(false);
    return 0;
}

LRESULT CMainDlg::OnGenerate(WORD, WORD, HWND, BOOL&) {
    if (busy_) {
        return 0;
    }

    SetBusy(true);
    std::wstring path;
    if (!ChooseFile(false, L"pfx", L"TestSignKernelTest.pfx",
                    L"PFX 密钥文件 (*.pfx)\0*.pfx\0所有文件 (*.*)\0*.*\0", path)) {
        SetBusy(false);
        return 0;
    }

    AppendStatus(std::wstring(L"正在生成密钥：") + path);
    RunAsync(OperationKind::Generate, [path]() {
        testsign::GenerateKeyPair(path);
        testsign::Settings::Save(testsign::Settings::kKeyPathName, path);
        OperationResult result;
        result.success = true;
        result.key_path = path;
        return result;
    });
    return 0;
}

LRESULT CMainDlg::OnImport(WORD, WORD, HWND, BOOL&) {
    if (busy_) {
        return 0;
    }

    const std::wstring pfx = GetText(IDC_KEY_PATH);
    if (pfx.empty()) {
        AppendStatus(L"请先选择 PFX 密钥 的完整路径。");
        return 0;
    }

    AppendStatus(L"正在导入公钥到本机证书存储...");
    RunAsync(OperationKind::Import, [pfx]() {
        testsign::EnsureExistingFile(pfx, L"PFX 密钥");
        testsign::ImportPublicKeyToLocalMachineRoot(pfx);
        OperationResult result;
        result.success = true;
        result.key_path = pfx;
        return result;
    });
    return 0;
}

LRESULT CMainDlg::OnBrowseSys(WORD, WORD, HWND, BOOL&) {
    if (busy_) {
        return 0;
    }

    SetBusy(true);
    std::wstring path;
    if (!ChooseFile(true, L"sys", nullptr, L"驱动程序 (*.sys)\0*.sys\0所有文件 (*.*)\0*.*\0",
                    path)) {
        SetBusy(false);
        return 0;
    }

    SetText(IDC_SYS_PATH, path);
    AppendStatus(std::wstring(L"已选择驱动：") + path);
    PersistAsync(false);
    return 0;
}

LRESULT CMainDlg::OnSign(WORD, WORD, HWND, BOOL&) {
    if (busy_) {
        return 0;
    }

    const std::wstring pfx = GetText(IDC_KEY_PATH);
    const std::wstring sys = GetText(IDC_SYS_PATH);
    if (pfx.empty() || sys.empty()) {
        AppendStatus(L"请先选择 PFX 密钥和 SYS 文件的完整路径。");
        return 0;
    }

    AppendStatus(std::wstring(L"正在签名：") + sys);
    RunAsync(OperationKind::Sign, [pfx, sys]() {
        testsign::EnsureExistingFile(pfx, L"PFX 密钥");
        testsign::EnsureExistingFile(sys, L"SYS 文件");
        testsign::SignDriver(pfx, sys);
        testsign::Settings::SaveAll({pfx, sys});
        OperationResult result;
        result.success = true;
        result.key_path = pfx;
        result.sys_path = sys;
        return result;
    });
    return 0;
}

LRESULT CMainDlg::OnClose(WORD, WORD, HWND, BOOL&) {
    RequestClose();
    return 0;
}

std::wstring CMainDlg::GetText(int control_id) const {
    const int length = ::GetWindowTextLengthW(GetDlgItem(control_id));
    if (length <= 0) {
        return {};
    }

    std::wstring text(static_cast<size_t>(length), L'\0');
    ::GetWindowTextW(GetDlgItem(control_id), text.data(), length + 1);
    return testsign::Trim(std::move(text));
}

void CMainDlg::SetText(int control_id, const std::wstring& text) {
    SetDlgItemTextW(control_id, text.c_str());
}

void CMainDlg::AppendStatus(const std::wstring& line) {
    const std::wstring existing = GetText(IDC_STATUS);
    std::wstring text = existing;
    if (!text.empty()) {
        text += L"\r\n";
    }
    text += line;
    SetText(IDC_STATUS, text);

    const HWND status = GetDlgItem(IDC_STATUS);
    const int length = ::GetWindowTextLengthW(status);
    ::SendMessageW(status, EM_SETSEL, static_cast<WPARAM>(length), static_cast<LPARAM>(length));
    ::SendMessageW(status, EM_SCROLLCARET, 0, 0);
}

void CMainDlg::SetBusy(bool busy) {
    busy_ = busy;
    const HWND controls[] = {
        GetDlgItem(IDC_KEY_PATH),   GetDlgItem(IDC_BROWSE_KEY), GetDlgItem(IDC_GENERATE),
        GetDlgItem(IDC_IMPORT),     GetDlgItem(IDC_SYS_PATH),  GetDlgItem(IDC_BROWSE_SYS),
        GetDlgItem(IDC_SIGN),       GetDlgItem(IDCANCEL),
    };
    for (const HWND control : controls) {
        ::EnableWindow(control, busy ? FALSE : TRUE);
    }
}

void CMainDlg::RequestClose() {
    if (busy_) {
        AppendStatus(L"请等待当前操作完成后再关闭。");
        return;
    }
    PersistAsync(true);
}

void CMainDlg::PersistAsync(bool close_after) {
    const testsign::Settings::Paths paths{GetText(IDC_KEY_PATH), GetText(IDC_SYS_PATH)};
    RunAsync(OperationKind::Persist, [paths, close_after]() {
        testsign::Settings::SaveAll(paths);
        OperationResult result;
        result.success = true;
        result.close_after = close_after;
        return result;
    });
}

void CMainDlg::RunAsync(OperationKind kind, std::function<OperationResult()> work) {
    if (worker_.joinable()) {
        AppendStatus(L"已有操作正在进行。");
        return;
    }

    if (!busy_) {
        SetBusy(true);
    }
    const HWND hwnd = m_hWnd;
    worker_ = std::jthread([hwnd, kind, work = std::move(work)]() {
        testsign::ComInitializer com;
        auto result = std::make_unique<OperationResult>();
        result->kind = kind;
        try {
            *result = work();
            result->kind = kind;
        } catch (const testsign::WinException& ex) {
            result->success = false;
            result->message = ex.wide();
        } catch (const std::exception&) {
            result->success = false;
            result->message = L"操作失败。";
        } catch (...) {
            result->success = false;
            result->message = L"操作失败（未知错误）。";
        }
        PostOperationResult(hwnd, std::move(result));
    });
}

bool CMainDlg::ChooseFile(bool open_dialog, const wchar_t* default_ext, const wchar_t* default_name,
                          const wchar_t* filter, std::wstring& path) {
    CFileDialog dialog(open_dialog, default_ext, default_name,
                       (open_dialog ? OFN_FILEMUSTEXIST : OFN_OVERWRITEPROMPT) | OFN_HIDEREADONLY |
                           OFN_PATHMUSTEXIST | OFN_EXPLORER,
                       filter, m_hWnd);
    if (dialog.DoModal() != IDOK) {
        return false;
    }
    path = dialog.m_szFileName;
    return true;
}
