#include "pch.h"
#include "MainDlg.h"
#include "Raii.h"

CAppModule _Module;

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    ::InitCommonControlsEx(&icc);

    testsign::ComInitializer com;
    if (FAILED(com.result()) && com.result() != RPC_E_CHANGED_MODE) {
        return 0;
    }

    testsign::AppModuleGuard module_guard(_Module, instance);
    if (!module_guard.initialized()) {
        return 0;
    }

    CMainDlg dialog;
    return static_cast<int>(dialog.DoModal());
}
