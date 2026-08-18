#include "pch.h"
#include "MetalUi.h"

#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "advapi32.lib")

namespace testsign {
namespace {

void SetVertex(TRIVERTEX& vertex, LONG x, LONG y, COLORREF color) {
    vertex.x = x;
    vertex.y = y;
    vertex.Red = static_cast<COLOR16>(GetRValue(color) << 8);
    vertex.Green = static_cast<COLOR16>(GetGValue(color) << 8);
    vertex.Blue = static_cast<COLOR16>(GetBValue(color) << 8);
    vertex.Alpha = 0;
}

class DcSelector {
public:
    DcSelector(HDC hdc, HGDIOBJ object) : hdc_(hdc), previous_(::SelectObject(hdc, object)) {}
    ~DcSelector() { ::SelectObject(hdc_, previous_); }

    DcSelector(const DcSelector&) = delete;
    DcSelector& operator=(const DcSelector&) = delete;

private:
    HDC hdc_;
    HGDIOBJ previous_;
};

HFONT MakeFont(const wchar_t* family, int point_size, int weight, UINT dpi) {
    LOGFONTW log_font{};
    log_font.lfHeight = -::MulDiv(point_size, static_cast<int>(dpi == 0 ? 96 : dpi), 72);
    log_font.lfWeight = weight;
    log_font.lfQuality = CLEARTYPE_QUALITY;
    log_font.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(log_font.lfFaceName, family, _TRUNCATE);
    return ::CreateFontIndirectW(&log_font);
}

}  // namespace

UINT QueryWindowDpi(HWND hwnd) {
    if (hwnd != nullptr) {
        const UINT dpi = ::GetDpiForWindow(hwnd);
        if (dpi != 0) {
            return dpi;
        }
    }
    const UINT system_dpi = ::GetDpiForSystem();
    return system_dpi != 0 ? system_dpi : 96;
}

int ScalePx(int px96, UINT dpi) {
    return ::MulDiv(px96, static_cast<int>(dpi == 0 ? 96 : dpi), 96);
}

bool QueryAppsUseLightTheme() {
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = ::RegGetValueW(
        HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (status != ERROR_SUCCESS) {
        return true;
    }
    return value != 0;
}

bool ResolveUsesLight(ThemeMode mode) {
    switch (mode) {
    case ThemeMode::Light:
        return true;
    case ThemeMode::Dark:
        return false;
    case ThemeMode::System:
        return QueryAppsUseLightTheme();
    }
    return QueryAppsUseLightTheme();
}

MetalPalette MakeMetalPalette(bool light) {
    if (light) {
        return MetalPalette{
            .bg_top = RGB(232, 235, 240),
            .bg_bottom = RGB(210, 214, 221),
            .panel_top = RGB(250, 251, 253),
            .panel_bottom = RGB(226, 230, 236),
            .line_hi = RGB(255, 255, 255),
            .line_lo = RGB(168, 174, 184),
            .edit = RGB(255, 255, 255),
            .text = RGB(32, 36, 42),
            .muted = RGB(90, 96, 106),
            .gold = RGB(160, 116, 52),
            .btn_top = RGB(246, 248, 251),
            .btn_bottom = RGB(208, 214, 222),
            .btn_hover_top = RGB(255, 255, 255),
            .btn_hover_border = RGB(120, 128, 140),
            .disabled = RGB(148, 154, 162),
            .btn_disabled_top = RGB(230, 233, 238),
            .btn_disabled_bottom = RGB(214, 218, 224),
            .btn_disabled_border = RGB(196, 202, 210),
            .caption_border = RGB(188, 194, 202),
            .grain = RGB(198, 202, 208),
            .dark = false,
        };
    }

    return MetalPalette{
        .bg_top = RGB(24, 26, 30),
        .bg_bottom = RGB(14, 15, 18),
        .panel_top = RGB(48, 52, 59),
        .panel_bottom = RGB(30, 32, 37),
        .line_hi = RGB(92, 98, 108),
        .line_lo = RGB(8, 9, 11),
        .edit = RGB(16, 17, 20),
        .text = RGB(226, 228, 232),
        .muted = RGB(154, 158, 166),
        .gold = RGB(201, 164, 104),
        .btn_top = RGB(68, 73, 82),
        .btn_bottom = RGB(36, 39, 45),
        .btn_hover_top = RGB(86, 92, 102),
        .btn_hover_border = RGB(168, 174, 184),
        .disabled = RGB(108, 112, 118),
        .btn_disabled_top = RGB(42, 44, 48),
        .btn_disabled_bottom = RGB(28, 30, 34),
        .btn_disabled_border = RGB(54, 57, 63),
        .caption_border = RGB(48, 50, 56),
        .grain = RGB(0, 0, 0),
        .dark = true,
    };
}

void FillVerticalGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX vertices[2]{};
    SetVertex(vertices[0], rc.left, rc.top, top);
    SetVertex(vertices[1], rc.right, rc.bottom, bottom);
    GRADIENT_RECT band{};
    band.UpperLeft = 0;
    band.LowerRight = 1;
    ::GradientFill(hdc, vertices, 2, &band, 1, GRADIENT_FILL_RECT_V);
}

void DrawMetalBackground(HDC hdc, const RECT& rc, const MetalPalette& palette, UINT dpi) {
    FillVerticalGradient(hdc, rc, palette.bg_top, palette.bg_bottom);

    UniqueGdiObject brush(::CreateSolidBrush(palette.grain));
    const int step = (std::max)(2, ScalePx(3, dpi));
    for (LONG y = rc.top; y < rc.bottom; y += step) {
        RECT line{rc.left, y, rc.right, y + 1};
        ::FillRect(hdc, &line, brush.as<HBRUSH>());
    }
}

void DrawMetalCard(HDC hdc, const RECT& rc, const wchar_t* title, HFONT title_font,
                   const MetalPalette& palette, UINT dpi) {
    const int radius = ScalePx(14, dpi);
    const int inset = (std::max)(1, ScalePx(1, dpi));
    const int highlight = ScalePx(8, dpi);
    UniqueGdiObject brush(::CreateSolidBrush(palette.panel_bottom));
    UniqueGdiObject pen(::CreatePen(PS_SOLID, 1, palette.line_hi));
    const DcSelector select_brush(hdc, brush.get());
    const DcSelector select_pen(hdc, pen.get());
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);

    RECT inner = rc;
    ::InflateRect(&inner, -inset, -inset);
    FillVerticalGradient(hdc, inner, palette.panel_top, palette.panel_bottom);

    UniqueGdiObject hi(::CreatePen(PS_SOLID, 1, palette.line_hi));
    UniqueGdiObject lo(::CreatePen(PS_SOLID, 1, palette.line_lo));
    {
        const DcSelector select_hi(hdc, hi.get());
        ::MoveToEx(hdc, inner.left + highlight, inner.top, nullptr);
        ::LineTo(hdc, inner.right - highlight, inner.top);
    }
    {
        const DcSelector select_lo(hdc, lo.get());
        ::MoveToEx(hdc, inner.left + highlight, inner.bottom - inset, nullptr);
        ::LineTo(hdc, inner.right - highlight, inner.bottom - inset);
    }

    if (title == nullptr || title_font == nullptr) {
        return;
    }

    const DcSelector select_font(hdc, title_font);
    TEXTMETRICW metrics{};
    ::GetTextMetricsW(hdc, &metrics);
    const LONG title_pad = ScalePx(8, dpi);
    const LONG title_top = rc.top + title_pad;
    const LONG title_bottom = title_top + metrics.tmHeight + title_pad;
    const LONG bar_left = rc.left + ScalePx(16, dpi);
    const LONG accent_width = (std::max)(2, ScalePx(3, dpi));
    RECT accent{bar_left, title_top + ScalePx(3, dpi), bar_left + accent_width,
                title_bottom - ScalePx(3, dpi)};
    UniqueGdiObject gold(::CreateSolidBrush(palette.gold));
    ::FillRect(hdc, &accent, gold.as<HBRUSH>());

    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, palette.gold);
    RECT text_rc{accent.right + ScalePx(10, dpi), title_top, rc.right - ScalePx(14, dpi),
                 title_bottom};
    ::DrawTextW(hdc, title, -1, &text_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void DrawMetalButton(const DRAWITEMSTRUCT& item, HFONT font, bool primary, bool hovered,
                     const MetalPalette& palette, UINT dpi) {
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    RECT rc = item.rcItem;
    const HDC hdc = item.hDC;
    const int radius = ScalePx(10, dpi);
    const int inset = (std::max)(1, ScalePx(1, dpi));

    COLORREF top = palette.btn_top;
    COLORREF bottom = palette.btn_bottom;
    COLORREF border = palette.line_hi;
    if (hovered && !disabled) {
        top = palette.btn_hover_top;
        border = primary ? palette.gold : palette.btn_hover_border;
    }
    if (pressed && !disabled) {
        top = palette.btn_bottom;
        bottom = palette.btn_top;
    }
    if (disabled) {
        top = palette.btn_disabled_top;
        bottom = palette.btn_disabled_bottom;
        border = palette.btn_disabled_border;
    }

    UniqueGdiObject clip(::CreateRectRgn(0, 0, 0, 0));
    const int had_clip = ::GetClipRgn(hdc, clip.as<HRGN>());
    ::BeginPath(hdc);
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    ::EndPath(hdc);
    ::SelectClipPath(hdc, RGN_COPY);

    RECT inner = rc;
    ::InflateRect(&inner, -inset, -inset);
    FillVerticalGradient(hdc, inner, top, bottom);

    if (had_clip == 1) {
        ::SelectClipRgn(hdc, clip.as<HRGN>());
    } else {
        ::SelectClipRgn(hdc, nullptr);
    }

    UniqueGdiObject pen(::CreatePen(PS_SOLID, 1, border));
    {
        const DcSelector select_brush(hdc, ::GetStockObject(NULL_BRUSH));
        const DcSelector select_pen(hdc, pen.get());
        ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    }

    wchar_t text[128]{};
    ::GetWindowTextW(item.hwndItem, text, 128);
    const DcSelector select_font(hdc, font);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, disabled ? palette.disabled : palette.text);
    ::DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void DrawMetalComboItem(const DRAWITEMSTRUCT& item, HFONT font, const MetalPalette& palette,
                        UINT dpi) {
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    const COLORREF background = selected ? palette.panel_top : palette.edit;
    UniqueGdiObject brush(::CreateSolidBrush(background));
    ::FillRect(item.hDC, &item.rcItem, brush.as<HBRUSH>());

    if (item.itemID == static_cast<UINT>(-1)) {
        return;
    }

    wchar_t text[64]{};
    ::SendMessageW(item.hwndItem, CB_GETLBTEXT, item.itemID, reinterpret_cast<LPARAM>(text));
    const DcSelector select_font(item.hDC, font);
    ::SetBkMode(item.hDC, TRANSPARENT);
    ::SetTextColor(item.hDC, selected ? palette.gold : palette.text);
    const int pad = ScalePx(8, dpi);
    RECT text_rc = item.rcItem;
    text_rc.left += pad;
    text_rc.right -= pad;
    ::DrawTextW(item.hDC, text, -1, &text_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void ApplyWindowFrame(HWND hwnd, const MetalPalette& palette) {
    constexpr DWORD kUseImmersiveDarkModeOld = 19;
    constexpr DWORD kUseImmersiveDarkMode = 20;
    constexpr DWORD kCaptionColor = 35;
    constexpr DWORD kTextColor = 36;
    constexpr DWORD kBorderColor = 34;
    const BOOL dark = palette.dark ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, kUseImmersiveDarkModeOld, &dark, sizeof(dark));
    ::DwmSetWindowAttribute(hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark));

    const COLORREF caption = palette.bg_bottom;
    const COLORREF text = palette.text;
    const COLORREF border = palette.caption_border;
    ::DwmSetWindowAttribute(hwnd, kCaptionColor, &caption, sizeof(caption));
    ::DwmSetWindowAttribute(hwnd, kTextColor, &text, sizeof(text));
    ::DwmSetWindowAttribute(hwnd, kBorderColor, &border, sizeof(border));
}

void MetalTheme::RecreateBrushes() {
    brush_dialog_.reset(::CreateSolidBrush(palette_.bg_top));
    brush_edit_.reset(::CreateSolidBrush(palette_.edit));
}

bool MetalTheme::ApplyResolvedPalette(bool force) {
    const MetalPalette next = MakeMetalPalette(ResolveUsesLight(preference_));
    if (!force && brush_dialog_ && next.dark == palette_.dark) {
        return false;
    }
    palette_ = next;
    RecreateBrushes();
    return true;
}

void MetalTheme::RecreateFonts(UINT dpi) {
    dpi_ = dpi == 0 ? 96 : dpi;
    font_ui_.reset(MakeFont(L"Segoe UI", 9, FW_NORMAL, dpi_));
    font_title_.reset(MakeFont(L"Segoe UI Semibold", 10, FW_SEMIBOLD, dpi_));
    font_log_.reset(MakeFont(L"Cascadia Mono", 9, FW_NORMAL, dpi_));
    if (!font_log_) {
        font_log_.reset(MakeFont(L"Consolas", 9, FW_NORMAL, dpi_));
    }
}

void MetalTheme::Create(ThemeMode preference, UINT dpi) {
    preference_ = preference;
    RecreateFonts(dpi);
    ApplyResolvedPalette(true);
}

bool MetalTheme::SetPreference(ThemeMode preference) {
    preference_ = preference;
    return ApplyResolvedPalette(false);
}

bool MetalTheme::Refresh() {
    return ApplyResolvedPalette(false);
}

}  // namespace testsign
