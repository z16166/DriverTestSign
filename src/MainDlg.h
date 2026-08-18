#pragma once

#include "pch.h"
#include "resource.h"

class CMainDlg : public CDialogImpl<CMainDlg> {
public:
    enum { IDD = IDD_MAIN };
    static constexpr UINT WM_APP_OPERATION_DONE = WM_APP + 1;

    enum class OperationKind { LoadSettings, Persist, Generate, Import, Sign };

    struct OperationResult {
        OperationKind kind{};
        bool success{false};
        bool close_after{false};
        std::wstring message;
        std::wstring key_path;
        std::wstring sys_path;
    };

    BEGIN_MSG_MAP(CMainDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CLOSE, OnWmClose)
        MESSAGE_HANDLER(WM_APP_OPERATION_DONE, OnOperationDone)
        COMMAND_ID_HANDLER(IDC_BROWSE_KEY, OnBrowseKey)
        COMMAND_ID_HANDLER(IDC_GENERATE, OnGenerate)
        COMMAND_ID_HANDLER(IDC_IMPORT, OnImport)
        COMMAND_ID_HANDLER(IDC_BROWSE_SYS, OnBrowseSys)
        COMMAND_ID_HANDLER(IDC_SIGN, OnSign)
        COMMAND_ID_HANDLER(IDCANCEL, OnClose)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnWmClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnOperationDone(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnBrowseKey(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnGenerate(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnImport(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnBrowseSys(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnSign(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnClose(WORD notify_code, WORD id, HWND control, BOOL& handled);

private:
    [[nodiscard]] std::wstring GetText(int control_id) const;
    void SetText(int control_id, const std::wstring& text);
    void AppendStatus(const std::wstring& line);
    void SetBusy(bool busy);
    void RequestClose();
    void PersistAsync(bool close_after);
    void RunAsync(OperationKind kind, std::function<OperationResult()> work);
    bool ChooseFile(bool open_dialog, const wchar_t* default_ext, const wchar_t* default_name,
                    const wchar_t* filter, std::wstring& path);

    std::jthread worker_;
    bool busy_{false};
};
