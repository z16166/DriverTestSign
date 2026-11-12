#pragma once

#include "pch.h"
#include "MetalUi.h"
#include "resource.h"

class CMainDlg : public CDialogImpl<CMainDlg> {
public:
    enum { IDD = IDD_MAIN };
    static constexpr UINT WM_APP_OPERATION_DONE = WM_APP + 1;
    static constexpr UINT WM_APP_STATUS_LINE = WM_APP + 2;

    enum class OperationKind { LoadSettings, Persist, Generate, Import, Sign, SignCatalog };

    struct OperationResult {
        OperationKind kind{};
        bool success{false};
        bool close_after{false};
        std::wstring message;
        std::wstring key_path;
        std::wstring sys_path;
        std::wstring inf_path;
        std::wstring cat_path;
        bool copied_sys{false};
    };

    BEGIN_MSG_MAP(CMainDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_CLOSE, OnWmClose)
        MESSAGE_HANDLER(WM_DPICHANGED, OnDpiChanged)
        MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
        MESSAGE_HANDLER(WM_SETTINGCHANGE, OnSettingChange)
        MESSAGE_HANDLER(WM_THEMECHANGED, OnThemeChanged)
        MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)
        MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnCtlColorStatic)
        MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnCtlColorEdit)
        MESSAGE_HANDLER(WM_CTLCOLORLISTBOX, OnCtlColorListBox)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_APP_OPERATION_DONE, OnOperationDone)
        MESSAGE_HANDLER(WM_APP_STATUS_LINE, OnStatusLine)
        COMMAND_HANDLER(IDC_THEME, CBN_SELCHANGE, OnThemeSelChange)
        COMMAND_HANDLER(IDC_LANGUAGE, CBN_SELCHANGE, OnLanguageSelChange)
        COMMAND_ID_HANDLER(IDC_BROWSE_KEY, OnBrowseKey)
        COMMAND_ID_HANDLER(IDC_GENERATE, OnGenerate)
        COMMAND_ID_HANDLER(IDC_IMPORT, OnImport)
        COMMAND_ID_HANDLER(IDC_BROWSE_SYS, OnBrowseSys)
        COMMAND_ID_HANDLER(IDC_BROWSE_INF, OnBrowseInf)
        COMMAND_ID_HANDLER(IDC_SIGN, OnSign)
        COMMAND_ID_HANDLER(IDC_SIGN_CAT, OnSignCatalog)
        COMMAND_ID_HANDLER(IDCANCEL, OnClose)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDestroy(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnWmClose(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDpiChanged(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnEraseBkgnd(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnSettingChange(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnThemeChanged(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnMeasureItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCtlColorStatic(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCtlColorEdit(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnCtlColorListBox(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnDrawItem(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnThemeSelChange(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnLanguageSelChange(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnOperationDone(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnStatusLine(UINT message, WPARAM wparam, LPARAM lparam, BOOL& handled);
    LRESULT OnBrowseKey(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnGenerate(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnImport(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnBrowseSys(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnBrowseInf(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnSign(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnSignCatalog(WORD notify_code, WORD id, HWND control, BOOL& handled);
    LRESULT OnClose(WORD notify_code, WORD id, HWND control, BOOL& handled);

    void TrackButtonHover(HWND button);

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
    void ApplyMetalChrome();
    void ApplyControlFonts();
    void ApplyOsChrome();
    void SyncOsTheme();
    void ApplyUiText();
    void InitThemeCombo();
    void InitLanguageCombo();
    void ApplyComboMetrics(int combo_id, int label_id, int item_count);
    void ApplyAllComboMetrics();
    void CaptureLayout();
    void ApplyScaledLayout(UINT new_dpi);
    [[nodiscard]] testsign::ThemeMode SelectedThemeMode() const;
    [[nodiscard]] testsign::UiLanguage SelectedLanguage() const;
    void DrawSectionCard(HDC hdc, int group_id, const wchar_t* title);

    struct ControlPlacement {
        HWND hwnd{nullptr};
        RECT rc{};
    };

    class WorkerThread {
    public:
        WorkerThread() = default;
        ~WorkerThread() { join(); }

        WorkerThread(const WorkerThread&) = delete;
        WorkerThread& operator=(const WorkerThread&) = delete;

        void reset(HANDLE thread) {
            join();
            thread_ = thread;
        }

        void join() {
            if (thread_ != nullptr) {
                ::WaitForSingleObject(thread_, INFINITE);
                ::CloseHandle(thread_);
                thread_ = nullptr;
            }
        }

        [[nodiscard]] bool joinable() const noexcept { return thread_ != nullptr; }

    private:
        HANDLE thread_{nullptr};
    };

    WorkerThread worker_;
    bool busy_{false};
    bool theme_combo_ready_{false};
    bool language_combo_ready_{false};
    UINT dpi_{0};
    UINT layout_dpi_{0};
    std::vector<ControlPlacement> layout_;
    testsign::MetalTheme theme_;
    std::vector<testsign::UniqueSubclass> button_subclasses_;
    HWND hovered_button_{nullptr};
};
