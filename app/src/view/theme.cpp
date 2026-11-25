/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "theme.h"

#include "ui_const.h"

namespace view {

ThemePalette palette(bool dark_mode) {
    const auto c = [dark_mode](const color::Tone& tone) {
        return lv_color_hex(color::pick(tone, dark_mode));
    };

    return ThemePalette{
        c(color::kBody),
        c(color::kCard),
        c(color::kBar),
        c(color::kButton),
        c(color::kBorder),
        c(color::kTextPrimary),
        c(color::kTextSecondary),
        c(color::kTextDisabled),
        c(color::kBrass),
        c(color::kShine),
        c(color::kSuccess),
        c(color::kWarning),
        c(color::kError),
    };
}

void apply_lvgl_theme(lv_display_t* display, bool dark_mode, const lv_font_t* font) {
    if (!display) {
        return;
    }

    const auto colors = palette(dark_mode);
    // Brass is the primary and the secondary both: one accent, used sparingly,
    // rather than the framework's blue-and-green pair.
    lv_theme_t* theme = lv_theme_default_init(display, colors.brass, colors.brass, dark_mode, font);
    lv_display_set_theme(display, theme);
}

} // namespace view
