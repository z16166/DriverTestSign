#include "pch.h"
#include "MetalUi.h"

#include <dwmapi.h>
#include <uxtheme.h>

#pragma comment(lib, "msimg32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

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

HFONT MakeFont(const wchar_t* family, int point_size, int weight) {
    LOGFONTW log_font{};
    const HDC screen = ::GetDC(nullptr);
    log_font.lfHeight = -::MulDiv(point_size, ::GetDeviceCaps(screen, LOGPIXELSY), 72);
    ::ReleaseDC(nullptr, screen);
    log_font.lfWeight = weight;
    log_font.lfQuality = CLEARTYPE_QUALITY;
    log_font.lfCharSet = DEFAULT_CHARSET;
    wcsncpy_s(log_font.lfFaceName, family, _TRUNCATE);
    return ::CreateFontIndirectW(&log_font);
}

}  // namespace

void FillVerticalGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX vertices[2]{};
    SetVertex(vertices[0], rc.left, rc.top, top);
    SetVertex(vertices[1], rc.right, rc.bottom, bottom);
    GRADIENT_RECT band{};
    band.UpperLeft = 0;
    band.LowerRight = 1;
    ::GradientFill(hdc, vertices, 2, &band, 1, GRADIENT_FILL_RECT_V);
}

void DrawMetalBackground(HDC hdc, const RECT& rc) {
    FillVerticalGradient(hdc, rc, kMetalBgTop, kMetalBgBottom);

    UniqueGdiObject brush(::CreateSolidBrush(RGB(0, 0, 0)));
    for (LONG y = rc.top; y < rc.bottom; y += 3) {
        RECT line{rc.left, y, rc.right, y + 1};
        ::FillRect(hdc, &line, brush.as<HBRUSH>());
    }
}

void DrawMetalCard(HDC hdc, const RECT& rc, const wchar_t* title, HFONT title_font) {
    UniqueGdiObject brush(::CreateSolidBrush(kMetalPanelBottom));
    UniqueGdiObject pen(::CreatePen(PS_SOLID, 1, kMetalLineHi));
    const DcSelector select_brush(hdc, brush.get());
    const DcSelector select_pen(hdc, pen.get());
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 14, 14);

    RECT inner = rc;
    ::InflateRect(&inner, -1, -1);
    FillVerticalGradient(hdc, inner, kMetalPanelTop, kMetalPanelBottom);

    UniqueGdiObject hi(::CreatePen(PS_SOLID, 1, kMetalLineHi));
    UniqueGdiObject lo(::CreatePen(PS_SOLID, 1, kMetalLineLo));
    {
        const DcSelector select_hi(hdc, hi.get());
        ::MoveToEx(hdc, inner.left + 8, inner.top, nullptr);
        ::LineTo(hdc, inner.right - 8, inner.top);
    }
    {
        const DcSelector select_lo(hdc, lo.get());
        ::MoveToEx(hdc, inner.left + 8, inner.bottom - 1, nullptr);
        ::LineTo(hdc, inner.right - 8, inner.bottom - 1);
    }

    if (title == nullptr || title_font == nullptr) {
        return;
    }

    const DcSelector select_font(hdc, title_font);
    TEXTMETRICW metrics{};
    ::GetTextMetricsW(hdc, &metrics);
    const LONG title_top = rc.top + 8;
    const LONG title_bottom = title_top + metrics.tmHeight + 8;
    const LONG bar_left = rc.left + 16;
    RECT accent{bar_left, title_top + 3, bar_left + 3, title_bottom - 3};
    UniqueGdiObject gold(::CreateSolidBrush(kMetalGold));
    ::FillRect(hdc, &accent, gold.as<HBRUSH>());

    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, kMetalGold);
    RECT text_rc{accent.right + 10, title_top, rc.right - 14, title_bottom};
    ::DrawTextW(hdc, title, -1, &text_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void DrawMetalButton(const DRAWITEMSTRUCT& item, HFONT font, bool primary, bool hovered) {
    const bool disabled = (item.itemState & ODS_DISABLED) != 0;
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    RECT rc = item.rcItem;
    const HDC hdc = item.hDC;

    COLORREF top = kMetalBtnTop;
    COLORREF bottom = kMetalBtnBottom;
    COLORREF border = kMetalLineHi;
    if (hovered && !disabled) {
        top = kMetalBtnHoverTop;
        border = primary ? kMetalGold : RGB(168, 174, 184);
    }
    if (pressed && !disabled) {
        top = kMetalBtnBottom;
        bottom = kMetalBtnTop;
    }
    if (disabled) {
        top = RGB(42, 44, 48);
        bottom = RGB(28, 30, 34);
        border = RGB(54, 57, 63);
    }

    UniqueGdiObject clip(::CreateRectRgn(0, 0, 0, 0));
    const int had_clip = ::GetClipRgn(hdc, clip.as<HRGN>());
    ::BeginPath(hdc);
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    ::EndPath(hdc);
    ::SelectClipPath(hdc, RGN_COPY);

    RECT inner = rc;
    ::InflateRect(&inner, -1, -1);
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
        ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 10, 10);
    }

    wchar_t text[128]{};
    ::GetWindowTextW(item.hwndItem, text, 128);
    const DcSelector select_font(hdc, font);
    ::SetBkMode(hdc, TRANSPARENT);
    ::SetTextColor(hdc, disabled ? kMetalDisabled : kMetalText);
    ::DrawTextW(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}

void ApplyDarkFrame(HWND hwnd) {
    constexpr DWORD kUseImmersiveDarkMode = 20;
    constexpr DWORD kCaptionColor = 35;
    constexpr DWORD kTextColor = 36;
    constexpr DWORD kBorderColor = 34;
    const BOOL dark = TRUE;
    ::DwmSetWindowAttribute(hwnd, kUseImmersiveDarkMode, &dark, sizeof(dark));

    const COLORREF caption = kMetalBgBottom;
    const COLORREF text = kMetalText;
    const COLORREF border = RGB(48, 50, 56);
    ::DwmSetWindowAttribute(hwnd, kCaptionColor, &caption, sizeof(caption));
    ::DwmSetWindowAttribute(hwnd, kTextColor, &text, sizeof(text));
    ::DwmSetWindowAttribute(hwnd, kBorderColor, &border, sizeof(border));
}

void MetalTheme::Create() {
    font_ui_.reset(MakeFont(L"Segoe UI", 9, FW_NORMAL));
    font_title_.reset(MakeFont(L"Segoe UI Semibold", 10, FW_SEMIBOLD));
    font_log_.reset(MakeFont(L"Cascadia Mono", 9, FW_NORMAL));
    if (!font_log_) {
        font_log_.reset(MakeFont(L"Consolas", 9, FW_NORMAL));
    }
    brush_dialog_.reset(::CreateSolidBrush(kMetalBgTop));
    brush_edit_.reset(::CreateSolidBrush(kMetalEdit));
}

}  // namespace testsign
