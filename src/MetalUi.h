#pragma once

#include "pch.h"
#include "Raii.h"

namespace testsign {

inline constexpr COLORREF kMetalBgTop = RGB(24, 26, 30);
inline constexpr COLORREF kMetalBgBottom = RGB(14, 15, 18);
inline constexpr COLORREF kMetalPanelTop = RGB(48, 52, 59);
inline constexpr COLORREF kMetalPanelBottom = RGB(30, 32, 37);
inline constexpr COLORREF kMetalLineHi = RGB(92, 98, 108);
inline constexpr COLORREF kMetalLineLo = RGB(8, 9, 11);
inline constexpr COLORREF kMetalEdit = RGB(16, 17, 20);
inline constexpr COLORREF kMetalText = RGB(226, 228, 232);
inline constexpr COLORREF kMetalMuted = RGB(154, 158, 166);
inline constexpr COLORREF kMetalGold = RGB(201, 164, 104);
inline constexpr COLORREF kMetalBtnTop = RGB(68, 73, 82);
inline constexpr COLORREF kMetalBtnBottom = RGB(36, 39, 45);
inline constexpr COLORREF kMetalBtnHoverTop = RGB(86, 92, 102);
inline constexpr COLORREF kMetalDisabled = RGB(108, 112, 118);

void FillVerticalGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom);
void DrawMetalBackground(HDC hdc, const RECT& rc);
void DrawMetalCard(HDC hdc, const RECT& rc, const wchar_t* title, HFONT title_font);
void DrawMetalButton(const DRAWITEMSTRUCT& item, HFONT font, bool primary, bool hovered);
void ApplyDarkFrame(HWND hwnd);

class MetalTheme {
public:
    void Create();

    [[nodiscard]] HFONT ui_font() const noexcept { return font_ui_.as<HFONT>(); }
    [[nodiscard]] HFONT title_font() const noexcept { return font_title_.as<HFONT>(); }
    [[nodiscard]] HFONT log_font() const noexcept { return font_log_.as<HFONT>(); }
    [[nodiscard]] HBRUSH dialog_brush() const noexcept { return brush_dialog_.as<HBRUSH>(); }
    [[nodiscard]] HBRUSH edit_brush() const noexcept { return brush_edit_.as<HBRUSH>(); }

private:
    UniqueGdiObject font_ui_;
    UniqueGdiObject font_title_;
    UniqueGdiObject font_log_;
    UniqueGdiObject brush_dialog_;
    UniqueGdiObject brush_edit_;
};

}  // namespace testsign
