#pragma once

#include "pch.h"
#include "Raii.h"
#include "Settings.h"

namespace testsign {

struct MetalPalette {
    COLORREF bg_top{};
    COLORREF bg_bottom{};
    COLORREF panel_top{};
    COLORREF panel_bottom{};
    COLORREF line_hi{};
    COLORREF line_lo{};
    COLORREF edit{};
    COLORREF text{};
    COLORREF muted{};
    COLORREF gold{};
    COLORREF btn_top{};
    COLORREF btn_bottom{};
    COLORREF btn_hover_top{};
    COLORREF btn_hover_border{};
    COLORREF disabled{};
    COLORREF btn_disabled_top{};
    COLORREF btn_disabled_bottom{};
    COLORREF btn_disabled_border{};
    COLORREF caption_border{};
    COLORREF grain{};
    bool dark{true};
};

[[nodiscard]] UINT QueryWindowDpi(HWND hwnd);
[[nodiscard]] int ScalePx(int px96, UINT dpi);
[[nodiscard]] bool QueryAppsUseLightTheme();
[[nodiscard]] bool ResolveUsesLight(ThemeMode mode);
[[nodiscard]] MetalPalette MakeMetalPalette(bool light);

void FillVerticalGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom);
void DrawMetalBackground(HDC hdc, const RECT& rc, const MetalPalette& palette, UINT dpi);
void DrawMetalCard(HDC hdc, const RECT& rc, const wchar_t* title, HFONT title_font,
                   const MetalPalette& palette, UINT dpi);
void DrawMetalButton(const DRAWITEMSTRUCT& item, HFONT font, bool primary, bool hovered,
                     const MetalPalette& palette, UINT dpi);
void DrawMetalComboItem(const DRAWITEMSTRUCT& item, HFONT font, const MetalPalette& palette,
                        UINT dpi);
void ApplyWindowFrame(HWND hwnd, const MetalPalette& palette);

class MetalTheme {
public:
    void Create(ThemeMode preference, UINT dpi);
    void RecreateFonts(UINT dpi);
    bool SetPreference(ThemeMode preference);
    bool Refresh();

    [[nodiscard]] ThemeMode preference() const noexcept { return preference_; }
    [[nodiscard]] const MetalPalette& palette() const noexcept { return palette_; }
    [[nodiscard]] UINT dpi() const noexcept { return dpi_; }
    [[nodiscard]] HFONT ui_font() const noexcept { return font_ui_.as<HFONT>(); }
    [[nodiscard]] HFONT title_font() const noexcept { return font_title_.as<HFONT>(); }
    [[nodiscard]] HFONT log_font() const noexcept { return font_log_.as<HFONT>(); }
    [[nodiscard]] HBRUSH dialog_brush() const noexcept { return brush_dialog_.as<HBRUSH>(); }
    [[nodiscard]] HBRUSH edit_brush() const noexcept { return brush_edit_.as<HBRUSH>(); }

private:
    bool ApplyResolvedPalette(bool force);
    void RecreateBrushes();

    ThemeMode preference_{ThemeMode::System};
    UINT dpi_{96};
    MetalPalette palette_{};
    UniqueGdiObject font_ui_;
    UniqueGdiObject font_title_;
    UniqueGdiObject font_log_;
    UniqueGdiObject brush_dialog_;
    UniqueGdiObject brush_edit_;
};

}  // namespace testsign
