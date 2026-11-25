/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "status_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "ui_const.h"

#include <cstdio>

namespace screen {
namespace {

// Nothing has asked the service anything yet, so every field stays at its
// "unknown" default. The screen renders that honestly rather than inventing a
// device to look at.
StatusSnapshot current_status() {
    return StatusSnapshot{};
}

constexpr const char* kServiceEndpoint = "127.0.0.1:8000";

} // namespace

StatusScreen::StatusScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void StatusScreen::build_content(lv_obj_t* content) {
    using namespace view;

    const StatusSnapshot status = current_status();
    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);

    auto* f_head  = assets().load_standard_font(metric::kTextTitle, app::StandardFontWeight::Bold);
    auto* f_label = assets().load_standard_font(metric::kTextLabel, app::StandardFontWeight::Regular);
    auto* f_value = assets().load_standard_font(metric::kTextValue, app::StandardFontWeight::Bold);
    auto* f_note  = assets().load_standard_font(metric::kTextNote, app::StandardFontWeight::Regular);

    const auto face = [](lv_font_t* f) -> const lv_font_t* {
        return f ? f : &lv_font_montserrat_12;
    };

    const auto text_at = [&](const char* s, const lv_font_t* font, lv_color_t colour, int y) {
        auto* l = lv_label_create(content);
        lv_label_set_text(l, s);
        lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_text_color(l, colour, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, metric::kGutter, y);
        return l;
    };

    const auto rule_at = [&](int y) {
        auto* r = lv_obj_create(content);
        lv_obj_remove_style_all(r);
        lv_obj_set_size(r, kScreenWidth - (metric::kGutter * 2), metric::kHairline);
        lv_obj_set_style_bg_color(r, p.border, 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_align(r, LV_ALIGN_TOP_LEFT, metric::kGutter, y);
    };

    // --- the statement -----------------------------------------------------
    // The page leads with one fact, set large and in the colour of its own
    // severity, instead of a row of equally weighted fields.
    if (!status.link_up) {
        text_at("No service", face(f_head), p.error, 6);
        text_at("Nothing here is measured.", face(f_note), p.text_secondary, 25);

        rule_at(46);

        // What it is waiting for, so the empty state is actionable rather than
        // just blank. Brass used once, on the one thing worth looking at.
        text_at("Waiting for a P4wnP1 service at", face(f_note), p.text_disabled, 56);
        text_at(kServiceEndpoint, face(f_value), p.brass, 72);
        return;
    }

    text_at("Service up", face(f_head), p.success, 6);
    text_at("Figures below are what the service reports.", face(f_note), p.text_secondary, 25);
    rule_at(46);

    // --- the facts ---------------------------------------------------------
    int y = 54;
    const auto row = [&](const char* name, const char* value, lv_color_t colour) {
        auto* k = lv_label_create(content);
        lv_label_set_text(k, name);
        lv_obj_set_style_text_font(k, face(f_label), 0);
        lv_obj_set_style_text_color(k, p.text_secondary, 0);
        lv_obj_align(k, LV_ALIGN_TOP_LEFT, metric::kGutter, y);

        auto* v = lv_label_create(content);
        lv_label_set_text(v, value);
        lv_obj_set_style_text_font(v, face(f_value), 0);
        lv_obj_set_style_text_color(v, colour, 0);
        lv_obj_align(v, LV_ALIGN_TOP_LEFT, metric::kGutter + 84, y);
        y += metric::kRowHeight;
    };

    char buf[64];
    row("USB", status.usb_composed ? status.usb_functions : "not composed",
        status.usb_composed ? p.text : p.text_disabled);
    row("usbeth", status.usbeth_addr[0] ? status.usbeth_addr : "none",
        status.usbeth_addr[0] ? p.text : p.text_disabled);
    row("wlan0", status.wlan_addr[0] ? status.wlan_addr : "none",
        status.wlan_addr[0] ? p.text : p.text_disabled);

    std::snprintf(buf, sizeof(buf), "%d running", status.jobs_running);
    row("Jobs", buf, status.jobs_running > 0 ? p.text : p.text_disabled);
}

} // namespace screen
