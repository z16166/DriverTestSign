#include "pch.h"
#include "MainDlg.h"
#include "Certificate.h"
#include "Settings.h"
#include "Signer.h"
#include "Strings.h"
#include "WinUtil.h"

#include <uxtheme.h>

namespace {

void PostOperationResult(HWND hwnd, std::unique_ptr<CMainDlg::OperationResult> result) {
    CMainDlg::OperationResult* raw = result.release();
    if (!::PostMessageW(hwnd, CMainDlg::WM_APP_OPERATION_DONE, 0, reinterpret_cast<LPARAM>(raw))) {
        std::unique_ptr<CMainDlg::OperationResult> discarded(raw);
    }
}

LRESULT CALLBACK MetalButtonProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                                 UINT_PTR, DWORD_PTR ref_data) {
    auto* dialog = reinterpret_cast<CMainDlg*>(ref_data);
    if (dialog != nullptr) {
        if (message == WM_MOUSEMOVE) {
            dialog->TrackButtonHover(hwnd);
        } else if (message == WM_MOUSELEAVE) {
            dialog->TrackButtonHover(nullptr);
        }
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

constexpr int kMetalButtons[] = {IDC_BROWSE_KEY, IDC_GENERATE, IDC_IMPORT,
                                 IDC_BROWSE_SYS, IDC_SIGN,     IDCANCEL};

struct ThemeOption {
    testsign::ThemeMode mode;
    testsign::Str label;
};

constexpr ThemeOption kThemeOptions[] = {
    {testsign::ThemeMode::Dark, testsign::Str::ThemeDark},
    {testsign::ThemeMode::Light, testsign::Str::ThemeLight},
    {testsign::ThemeMode::System, testsign::Str::ThemeSystem},
};

struct LanguageOption {
    testsign::UiLanguage language;
    const wchar_t* label;
};

constexpr LanguageOption kLanguageOptions[] = {
    {testsign::UiLanguage::English, L"English"},
    {testsign::UiLanguage::ChineseSimplified, L"简体中文"},
};

[[nodiscard]] int ThemeComboItemHeight(HWND dialog, HFONT font, UINT dpi) {
    RECT dlu{0, 0, 0, 18};
    if (dialog != nullptr) {
        ::MapDialogRect(dialog, &dlu);
    }
    int height = dlu.bottom > 0 ? dlu.bottom : testsign::ScalePx(24, dpi);

    if (font != nullptr) {
        const HDC hdc = ::GetDC(dialog);
        const HGDIOBJ previous = ::SelectObject(hdc, font);
        TEXTMETRICW metrics{};
        ::GetTextMetricsW(hdc, &metrics);
        ::SelectObject(hdc, previous);
        ::ReleaseDC(dialog, hdc);
        const int from_font =
            metrics.tmHeight + metrics.tmExternalLeading + testsign::ScalePx(10, dpi);
        if (from_font > height) {
            height = from_font;
        }
    }
    return height;
}

}  // namespace

LRESULT CMainDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    testsign::SetUiLanguage(testsign::Settings::LoadLanguage());
    CenterWindow();
    SetIcon(::LoadIconW(nullptr, IDI_SHIELD), TRUE);
    SetIcon(::LoadIconW(nullptr, IDI_SHIELD), FALSE);
    ApplyMetalChrome();
    SetBusy(true);

    AppendStatus(testsign::Tr(testsign::Str::StatusLoading));
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
    button_subclasses_.clear();
    hovered_button_ = nullptr;
    layout_.clear();
    return 0;
}

LRESULT CMainDlg::OnDpiChanged(UINT, WPARAM wparam, LPARAM lparam, BOOL&) {
    const UINT new_dpi = HIWORD(wparam);
    const auto* suggested = reinterpret_cast<const RECT*>(lparam);
    if (new_dpi == 0) {
        return 0;
    }

    if (suggested != nullptr) {
        SetWindowPos(nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }

    if (layout_dpi_ != 0 && new_dpi != layout_dpi_) {
        ApplyScaledLayout(new_dpi);
    }

    dpi_ = new_dpi;
    if (theme_.ui_font() != nullptr) {
        theme_.RecreateFonts(new_dpi);
        ApplyControlFonts();
        ApplyAllComboMetrics();
        CaptureLayout();
        ApplyOsChrome();
    }
    return 0;
}

LRESULT CMainDlg::OnEraseBkgnd(UINT, WPARAM wparam, LPARAM, BOOL&) {
    const HDC hdc = reinterpret_cast<HDC>(wparam);
    RECT client{};
    GetClientRect(&client);
    testsign::DrawMetalBackground(hdc, client, theme_.palette(), theme_.dpi());
    DrawSectionCard(hdc, IDC_GROUP_KEY, testsign::Tr(testsign::Str::CardKey));
    DrawSectionCard(hdc, IDC_GROUP_DRV, testsign::Tr(testsign::Str::CardDriver));
    DrawSectionCard(hdc, IDC_GROUP_LOG, testsign::Tr(testsign::Str::CardStatus));
    return TRUE;
}

LRESULT CMainDlg::OnSettingChange(UINT, WPARAM, LPARAM, BOOL&) {
    SyncOsTheme();
    return 0;
}

LRESULT CMainDlg::OnThemeChanged(UINT, WPARAM, LPARAM, BOOL&) {
    SyncOsTheme();
    return 0;
}

LRESULT CMainDlg::OnMeasureItem(UINT, WPARAM, LPARAM lparam, BOOL& handled) {
    auto* measure = reinterpret_cast<MEASUREITEMSTRUCT*>(lparam);
    if (measure == nullptr || (measure->CtlID != IDC_THEME && measure->CtlID != IDC_LANGUAGE)) {
        handled = FALSE;
        return 0;
    }
    measure->itemHeight = ThemeComboItemHeight(m_hWnd, theme_.ui_font(), theme_.dpi());
    return TRUE;
}

LRESULT CMainDlg::OnCtlColorStatic(UINT, WPARAM wparam, LPARAM lparam, BOOL&) {
    const HWND control = reinterpret_cast<HWND>(lparam);
    wchar_t class_name[16]{};
    ::GetClassNameW(control, class_name, 16);
    if (::_wcsicmp(class_name, L"Edit") == 0) {
        const HDC hdc = reinterpret_cast<HDC>(wparam);
        ::SetTextColor(hdc, theme_.palette().text);
        ::SetBkColor(hdc, theme_.palette().edit);
        return reinterpret_cast<LRESULT>(theme_.edit_brush());
    }

    const HDC hdc = reinterpret_cast<HDC>(wparam);
    ::SetTextColor(hdc, theme_.palette().muted);
    ::SetBkMode(hdc, TRANSPARENT);
    return reinterpret_cast<LRESULT>(::GetStockObject(NULL_BRUSH));
}

LRESULT CMainDlg::OnCtlColorEdit(UINT, WPARAM wparam, LPARAM, BOOL&) {
    const HDC hdc = reinterpret_cast<HDC>(wparam);
    ::SetTextColor(hdc, theme_.palette().text);
    ::SetBkColor(hdc, theme_.palette().edit);
    return reinterpret_cast<LRESULT>(theme_.edit_brush());
}

LRESULT CMainDlg::OnCtlColorListBox(UINT, WPARAM wparam, LPARAM, BOOL&) {
    const HDC hdc = reinterpret_cast<HDC>(wparam);
    ::SetTextColor(hdc, theme_.palette().text);
    ::SetBkColor(hdc, theme_.palette().edit);
    return reinterpret_cast<LRESULT>(theme_.edit_brush());
}

LRESULT CMainDlg::OnDrawItem(UINT, WPARAM, LPARAM lparam, BOOL&) {
    const auto& item = *reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
    if (item.CtlType == ODT_COMBOBOX || item.CtlID == IDC_THEME) {
        testsign::DrawMetalComboItem(item, theme_.ui_font(), theme_.palette(), theme_.dpi());
        return TRUE;
    }

    const int id = ::GetDlgCtrlID(item.hwndItem);
    const bool primary = id == IDC_GENERATE || id == IDC_IMPORT || id == IDC_SIGN;
    testsign::DrawMetalButton(item, theme_.ui_font(), primary, item.hwndItem == hovered_button_,
                             theme_.palette(), theme_.dpi());
    return TRUE;
}

LRESULT CMainDlg::OnThemeSelChange(WORD, WORD, HWND, BOOL&) {
    if (!theme_combo_ready_) {
        return 0;
    }

    const testsign::ThemeMode mode = SelectedThemeMode();
    testsign::Settings::SaveThemeMode(mode);
    if (theme_.SetPreference(mode)) {
        ApplyOsChrome();
    }
    return 0;
}

LRESULT CMainDlg::OnLanguageSelChange(WORD, WORD, HWND, BOOL&) {
    if (!language_combo_ready_) {
        return 0;
    }

    const testsign::UiLanguage language = SelectedLanguage();
    testsign::SetUiLanguage(language);
    testsign::Settings::SaveLanguage(language);
    ApplyUiText();
    InitThemeCombo();
    ApplyAllComboMetrics();
    CaptureLayout();
    ApplyOsChrome();
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
        AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusFailedPrefix)) + result->message);
        return 0;
    }

    switch (result->kind) {
    case OperationKind::LoadSettings:
        SetText(IDC_KEY_PATH, result->key_path);
        SetText(IDC_SYS_PATH, result->sys_path);
        AppendStatus(testsign::Tr(testsign::Str::StatusReady1));
        AppendStatus(testsign::Tr(testsign::Str::StatusReady2));
        AppendStatus(testsign::Tr(testsign::Str::StatusReady3));
        break;
    case OperationKind::Persist:
        if (result->close_after) {
            EndDialog(IDCANCEL);
        }
        break;
    case OperationKind::Generate:
        SetText(IDC_KEY_PATH, result->key_path);
        AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusGenerated)) + result->key_path);
        break;
    case OperationKind::Import:
        AppendStatus(testsign::Tr(testsign::Str::StatusImported));
        break;
    case OperationKind::Sign:
        AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusSigned)) + result->sys_path);
        AppendStatus(testsign::Tr(testsign::Str::StatusSignHint));
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
    if (!ChooseFile(true, L"pfx", nullptr, testsign::Tr(testsign::Str::FilterPfx), path)) {
        SetBusy(false);
        return 0;
    }

    SetText(IDC_KEY_PATH, path);
    AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusSelectedKey)) + path);
    PersistAsync(false);
    return 0;
}

LRESULT CMainDlg::OnGenerate(WORD, WORD, HWND, BOOL&) {
    if (busy_) {
        return 0;
    }

    SetBusy(true);
    std::wstring path;
    if (!ChooseFile(false, L"pfx", L"TestSignKernelTest.pfx", testsign::Tr(testsign::Str::FilterPfx),
                    path)) {
        SetBusy(false);
        return 0;
    }

    AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusGenerating)) + path);
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
        AppendStatus(testsign::Tr(testsign::Str::StatusNeedPfx));
        return 0;
    }

    AppendStatus(testsign::Tr(testsign::Str::StatusImporting));
    RunAsync(OperationKind::Import, [pfx]() {
        testsign::EnsureExistingFile(pfx, testsign::Tr(testsign::Str::WhatPfx));
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
    if (!ChooseFile(true, L"sys", nullptr, testsign::Tr(testsign::Str::FilterSys), path)) {
        SetBusy(false);
        return 0;
    }

    SetText(IDC_SYS_PATH, path);
    AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusSelectedSys)) + path);
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
        AppendStatus(testsign::Tr(testsign::Str::StatusNeedPfxAndSys));
        return 0;
    }

    AppendStatus(std::wstring(testsign::Tr(testsign::Str::StatusSigning)) + sys);
    RunAsync(OperationKind::Sign, [pfx, sys]() {
        testsign::EnsureExistingFile(pfx, testsign::Tr(testsign::Str::WhatPfx));
        testsign::EnsureExistingFile(sys, testsign::Tr(testsign::Str::WhatSys));
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
        ::InvalidateRect(control, nullptr, FALSE);
    }
}

void CMainDlg::ApplyOsChrome() {
    testsign::ApplyWindowFrame(m_hWnd, theme_.palette());

    const wchar_t* edit_theme = theme_.palette().dark ? L"DarkMode_Explorer" : L"Explorer";
    const int edits[] = {IDC_KEY_PATH, IDC_SYS_PATH, IDC_STATUS};
    for (const int id : edits) {
        const HWND edit = GetDlgItem(id);
        ::SetWindowTheme(edit, edit_theme, nullptr);
    }

    const wchar_t* combo_theme = theme_.palette().dark ? L"DarkMode_CFD" : L"CFD";
    ::SetWindowTheme(GetDlgItem(IDC_THEME), combo_theme, nullptr);
    ::SetWindowTheme(GetDlgItem(IDC_LANGUAGE), combo_theme, nullptr);

    ::RedrawWindow(m_hWnd, nullptr, nullptr,
                   RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME);
}

void CMainDlg::SyncOsTheme() {
    if (theme_.preference() != testsign::ThemeMode::System) {
        return;
    }
    if (theme_.Refresh()) {
        ApplyOsChrome();
    }
}

void CMainDlg::ApplyComboMetrics(int combo_id, int label_id, int item_count) {
    const HWND combo = GetDlgItem(combo_id);
    const HWND label = GetDlgItem(label_id);
    const int item_height = ThemeComboItemHeight(m_hWnd, theme_.ui_font(), theme_.dpi());
    const int extra = testsign::ScalePx(6, theme_.dpi());
    RECT combo_rc{};
    ::GetWindowRect(combo, &combo_rc);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&combo_rc), 2);
    ::SetWindowPos(combo, nullptr, combo_rc.left, combo_rc.top,
                   combo_rc.right - combo_rc.left, item_height + extra,
                   SWP_NOZORDER | SWP_NOACTIVATE);
    ::SendMessageW(combo, CB_SETITEMHEIGHT, static_cast<WPARAM>(-1), item_height);
    ::SendMessageW(combo, CB_SETITEMHEIGHT, 0, item_height);

    COMBOBOXINFO info{};
    info.cbSize = sizeof(info);
    if (::GetComboBoxInfo(combo, &info) && info.hwndList != nullptr) {
        const int list_height = item_height * item_count + extra;
        ::SetWindowPos(info.hwndList, nullptr, 0, 0, combo_rc.right - combo_rc.left, list_height,
                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    RECT label_rc{};
    ::GetWindowRect(label, &label_rc);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&label_rc), 2);
    const int label_height = label_rc.bottom - label_rc.top;
    ::SetWindowPos(label, nullptr, label_rc.left,
                   combo_rc.top + (item_height + extra - label_height) / 2, 0, 0,
                   SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void CMainDlg::ApplyAllComboMetrics() {
    ApplyComboMetrics(IDC_THEME, IDC_LABEL_THEME, static_cast<int>(std::size(kThemeOptions)));
    ApplyComboMetrics(IDC_LANGUAGE, IDC_LABEL_LANG, static_cast<int>(std::size(kLanguageOptions)));
}

void CMainDlg::ApplyUiText() {
    SetWindowText(testsign::Tr(testsign::Str::WindowTitle));
    SetDlgItemTextW(IDC_LABEL_KEY, testsign::Tr(testsign::Str::LabelPfx));
    SetDlgItemTextW(IDC_LABEL_SYS, testsign::Tr(testsign::Str::LabelSys));
    SetDlgItemTextW(IDC_LABEL_THEME, testsign::Tr(testsign::Str::LabelTheme));
    SetDlgItemTextW(IDC_LABEL_LANG, testsign::Tr(testsign::Str::LabelLanguage));
    SetDlgItemTextW(IDC_BROWSE_KEY, testsign::Tr(testsign::Str::Browse));
    SetDlgItemTextW(IDC_GENERATE, testsign::Tr(testsign::Str::Generate));
    SetDlgItemTextW(IDC_IMPORT, testsign::Tr(testsign::Str::Import));
    SetDlgItemTextW(IDC_BROWSE_SYS, testsign::Tr(testsign::Str::Browse));
    SetDlgItemTextW(IDC_SIGN, testsign::Tr(testsign::Str::Sign));
    SetDlgItemTextW(IDCANCEL, testsign::Tr(testsign::Str::Close));
}

void CMainDlg::CaptureLayout() {
    layout_.clear();
    layout_dpi_ = dpi_ == 0 ? theme_.dpi() : dpi_;
    HWND child = ::GetWindow(m_hWnd, GW_CHILD);
    while (child != nullptr) {
        ControlPlacement item;
        item.hwnd = child;
        ::GetWindowRect(child, &item.rc);
        ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&item.rc), 2);
        layout_.push_back(item);
        child = ::GetWindow(child, GW_HWNDNEXT);
    }
}

void CMainDlg::ApplyScaledLayout(UINT new_dpi) {
    if (layout_.empty() || layout_dpi_ == 0 || new_dpi == 0) {
        return;
    }

    for (const ControlPlacement& item : layout_) {
        if (!::IsWindow(item.hwnd)) {
            continue;
        }
        const int x = ::MulDiv(item.rc.left, static_cast<int>(new_dpi), static_cast<int>(layout_dpi_));
        const int y = ::MulDiv(item.rc.top, static_cast<int>(new_dpi), static_cast<int>(layout_dpi_));
        const int width =
            ::MulDiv(item.rc.right - item.rc.left, static_cast<int>(new_dpi), static_cast<int>(layout_dpi_));
        const int height =
            ::MulDiv(item.rc.bottom - item.rc.top, static_cast<int>(new_dpi), static_cast<int>(layout_dpi_));
        ::SetWindowPos(item.hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

void CMainDlg::InitThemeCombo() {
    theme_combo_ready_ = false;
    const HWND combo = GetDlgItem(IDC_THEME);
    ::SendMessageW(combo, WM_SETFONT, reinterpret_cast<WPARAM>(theme_.ui_font()), TRUE);
    ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    const testsign::ThemeMode selected = theme_.preference();
    int selected_index = 2;
    for (const ThemeOption& option : kThemeOptions) {
        const int index = static_cast<int>(::SendMessageW(
            combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(testsign::Tr(option.label))));
        ::SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index),
                       static_cast<LPARAM>(option.mode));
        if (option.mode == selected) {
            selected_index = index;
        }
    }
    ::SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected_index), 0);
    theme_combo_ready_ = true;
}

void CMainDlg::InitLanguageCombo() {
    language_combo_ready_ = false;
    const HWND combo = GetDlgItem(IDC_LANGUAGE);
    ::SendMessageW(combo, WM_SETFONT, reinterpret_cast<WPARAM>(theme_.ui_font()), TRUE);
    ::SendMessageW(combo, CB_RESETCONTENT, 0, 0);

    const testsign::UiLanguage selected = testsign::CurrentUiLanguage();
    int selected_index = 0;
    for (const LanguageOption& option : kLanguageOptions) {
        const int index = static_cast<int>(::SendMessageW(combo, CB_ADDSTRING, 0,
                                                          reinterpret_cast<LPARAM>(option.label)));
        ::SendMessageW(combo, CB_SETITEMDATA, static_cast<WPARAM>(index),
                       static_cast<LPARAM>(option.language));
        if (option.language == selected) {
            selected_index = index;
        }
    }
    ::SendMessageW(combo, CB_SETCURSEL, static_cast<WPARAM>(selected_index), 0);
    language_combo_ready_ = true;
}

testsign::ThemeMode CMainDlg::SelectedThemeMode() const {
    const HWND combo = GetDlgItem(IDC_THEME);
    const int index = static_cast<int>(::SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (index < 0) {
        return testsign::ThemeMode::System;
    }
    return static_cast<testsign::ThemeMode>(
        ::SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0));
}

testsign::UiLanguage CMainDlg::SelectedLanguage() const {
    const HWND combo = GetDlgItem(IDC_LANGUAGE);
    const int index = static_cast<int>(::SendMessageW(combo, CB_GETCURSEL, 0, 0));
    if (index < 0) {
        return testsign::UiLanguage::English;
    }
    return static_cast<testsign::UiLanguage>(
        ::SendMessageW(combo, CB_GETITEMDATA, static_cast<WPARAM>(index), 0));
}

void CMainDlg::ApplyControlFonts() {
    SetFont(theme_.ui_font());

    const int labels[] = {IDC_LABEL_KEY, IDC_LABEL_SYS, IDC_LABEL_THEME, IDC_LABEL_LANG};
    for (const int id : labels) {
        ::SendMessageW(GetDlgItem(id), WM_SETFONT, reinterpret_cast<WPARAM>(theme_.ui_font()), TRUE);
    }

    const int edits[] = {IDC_KEY_PATH, IDC_SYS_PATH, IDC_STATUS};
    for (const int id : edits) {
        ::SendMessageW(GetDlgItem(id), WM_SETFONT,
                       reinterpret_cast<WPARAM>(id == IDC_STATUS ? theme_.log_font() : theme_.ui_font()),
                       TRUE);
    }

    for (const int id : kMetalButtons) {
        ::SendMessageW(GetDlgItem(id), WM_SETFONT, reinterpret_cast<WPARAM>(theme_.ui_font()), TRUE);
    }
    ::SendMessageW(GetDlgItem(IDC_THEME), WM_SETFONT, reinterpret_cast<WPARAM>(theme_.ui_font()),
                   TRUE);
    ::SendMessageW(GetDlgItem(IDC_LANGUAGE), WM_SETFONT, reinterpret_cast<WPARAM>(theme_.ui_font()),
                   TRUE);
}

void CMainDlg::ApplyMetalChrome() {
    dpi_ = testsign::QueryWindowDpi(m_hWnd);
    theme_.Create(testsign::Settings::LoadThemeMode(), dpi_);

    const int groups[] = {IDC_GROUP_KEY, IDC_GROUP_DRV, IDC_GROUP_LOG};
    for (const int id : groups) {
        ::ShowWindow(GetDlgItem(id), SW_HIDE);
    }

    const int labels[] = {IDC_LABEL_KEY, IDC_LABEL_SYS, IDC_LABEL_THEME, IDC_LABEL_LANG};
    for (const int id : labels) {
        const HWND label = GetDlgItem(id);
        ::SetWindowLongPtrW(label, GWL_EXSTYLE,
                            ::GetWindowLongPtrW(label, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
    }

    const int edits[] = {IDC_KEY_PATH, IDC_SYS_PATH, IDC_STATUS};
    for (const int id : edits) {
        const HWND edit = GetDlgItem(id);
        ::SetWindowLongPtrW(edit, GWL_EXSTYLE,
                            ::GetWindowLongPtrW(edit, GWL_EXSTYLE) & ~WS_EX_CLIENTEDGE);
        ::SetWindowPos(edit, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    ApplyControlFonts();

    button_subclasses_.clear();
    UINT_PTR subclass_id = 1;
    for (const int id : kMetalButtons) {
        const HWND button = GetDlgItem(id);
        button_subclasses_.emplace_back(button, MetalButtonProc, subclass_id, reinterpret_cast<DWORD_PTR>(this));
        ++subclass_id;
    }

    InitThemeCombo();
    InitLanguageCombo();
    ApplyUiText();
    ApplyAllComboMetrics();
    CaptureLayout();
    ApplyOsChrome();
}

void CMainDlg::DrawSectionCard(HDC hdc, int group_id, const wchar_t* title) {
    RECT rc{};
    ::GetWindowRect(GetDlgItem(group_id), &rc);
    ::MapWindowPoints(nullptr, m_hWnd, reinterpret_cast<POINT*>(&rc), 2);
    testsign::DrawMetalCard(hdc, rc, title, theme_.title_font(), theme_.palette(), theme_.dpi());
}

void CMainDlg::TrackButtonHover(HWND button) {
    if (hovered_button_ == button) {
        if (button != nullptr) {
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = button;
            ::TrackMouseEvent(&track);
        }
        return;
    }

    const HWND previous = hovered_button_;
    hovered_button_ = button;
    if (previous != nullptr) {
        ::InvalidateRect(previous, nullptr, FALSE);
    }
    if (hovered_button_ != nullptr) {
        ::InvalidateRect(hovered_button_, nullptr, FALSE);
        TRACKMOUSEEVENT track{};
        track.cbSize = sizeof(track);
        track.dwFlags = TME_LEAVE;
        track.hwndTrack = hovered_button_;
        ::TrackMouseEvent(&track);
    }
}

void CMainDlg::RequestClose() {
    if (busy_) {
        AppendStatus(testsign::Tr(testsign::Str::StatusWaitClose));
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
        AppendStatus(testsign::Tr(testsign::Str::StatusBusy));
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
            result->message = testsign::Tr(testsign::Str::StatusOpFailed);
        } catch (...) {
            result->success = false;
            result->message = testsign::Tr(testsign::Str::StatusOpFailedUnknown);
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
