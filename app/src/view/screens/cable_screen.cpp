/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "cable_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "ui_const.h"

#include <array>
#include <cstdio>

namespace screen {
namespace {

// The subset of the USB composition worth exposing on a 320x170 panel, with
// the endpoint cost of each. Order fixes the bit positions in the selection
// mask, so rows may be relabelled but not reordered without migrating it.
constexpr std::array<GadgetFunction, kGadgetRows> kFunctions = {{
    {"use_HID_KEYBOARD", "Keyboard",     1, false},
    {"use_RNDIS",        "RNDIS net",    2, false},
    {"use_HID_MOUSE",    "Mouse",        1, false},
    {"use_CDC_ECM",      "CDC ECM net",  2, false},
    {"use_HID_RAW",      "Raw HID",      1, false},
    {"use_UMS",          "Mass storage", 2, false},
}};

constexpr int kColumnWidth = 150;
constexpr int kRowPitch    = 15;

} // namespace

CableScreen::CableScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void CableScreen::state_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* self = static_cast<CableScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->render();
    }
}

void CableScreen::build_content(lv_obj_t* content) {
    content_ = content;

    // Selection and cursor both live in the view model, so they survive this
    // screen being torn down and rebuilt when the page changes.
    lv_subject_add_observer_obj(view_model().gadget_mask_subject(), state_changed, content, this);
    lv_subject_add_observer_obj(view_model().gadget_cursor_subject(), state_changed, content, this);

    render();
}

void CableScreen::render() {
    using namespace view;

    if (!content_ || !lv_obj_is_valid(content_)) {
        return;
    }
    lv_obj_clean(content_);

    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);
    const int32_t mask = view_model().gadget_mask();
    const int32_t cursor = view_model().gadget_cursor();

    int used = 0;
    for (int i = 0; i < kGadgetRows; ++i) {
        if (mask & (1 << i)) {
            used += kFunctions[static_cast<std::size_t>(i)].cost;
        }
    }
    const bool over = used > kEndpointBudget;

    auto* f_label = assets().load_standard_font(metric::kTextLabel, app::StandardFontWeight::Regular);
    auto* f_value = assets().load_standard_font(metric::kTextValue, app::StandardFontWeight::Bold);
    auto* f_note  = assets().load_standard_font(metric::kTextNote, app::StandardFontWeight::Regular);
    const auto face = [](lv_font_t* f) -> const lv_font_t* {
        return f ? f : &lv_font_montserrat_12;
    };

    // --- the meter ---------------------------------------------------------
    auto* caption = lv_label_create(content_);
    lv_label_set_text(caption, "USB endpoints");
    lv_obj_set_style_text_font(caption, face(f_label), 0);
    lv_obj_set_style_text_color(caption, p.text_secondary, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, metric::kGutter, 6);

    char tally[24];
    std::snprintf(tally, sizeof(tally), "%d of %d", used, kEndpointBudget);
    auto* count = lv_label_create(content_);
    lv_label_set_text(count, tally);
    lv_obj_set_style_text_font(count, face(f_value), 0);
    lv_obj_set_style_text_color(count, over ? p.error : p.text, 0);
    lv_obj_align(count, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 6);

    // One segment per endpoint. A spent endpoint is filled and a free one is
    // an outline, so the shape carries the reading and colour is not doing the
    // work alone.
    constexpr int kSegGap = 3;
    const int track = kScreenWidth - (metric::kGutter * 2);
    const int seg_w = (track - (kSegGap * (kEndpointBudget - 1))) / kEndpointBudget;
    for (int i = 0; i < kEndpointBudget; ++i) {
        auto* seg = lv_obj_create(content_);
        lv_obj_remove_style_all(seg);
        lv_obj_set_size(seg, seg_w, 7);
        lv_obj_set_style_radius(seg, 1, 0);
        lv_obj_align(seg, LV_ALIGN_TOP_LEFT, metric::kGutter + i * (seg_w + kSegGap), 24);
        if (i < used) {
            lv_obj_set_style_bg_color(seg, over ? p.error : p.shine, 0);
            lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
        }
        else {
            lv_obj_set_style_border_color(seg, p.border, 0);
            lv_obj_set_style_border_width(seg, metric::kHairline, 0);
            lv_obj_set_style_bg_opa(seg, LV_OPA_TRANSP, 0);
        }
    }

    auto* rule = lv_obj_create(content_);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, track, metric::kHairline);
    lv_obj_set_style_bg_color(rule, p.border, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_LEFT, metric::kGutter, 38);

    // --- the functions, two columns ----------------------------------------
    for (int i = 0; i < kGadgetRows; ++i) {
        const auto& fn = kFunctions[static_cast<std::size_t>(i)];
        const bool on = (mask & (1 << i)) != 0;
        const bool here = (i == cursor);
        const int col = i / 3;
        const int row = i % 3;
        const int x = metric::kGutter + col * kColumnWidth;
        const int y = 46 + row * kRowPitch;

        // The cursor is a filled bar behind the row rather than a different
        // text colour, so it stays legible whatever state the row is in.
        if (here) {
            auto* hl = lv_obj_create(content_);
            lv_obj_remove_style_all(hl);
            lv_obj_set_size(hl, kColumnWidth - 6, kRowPitch - 1);
            lv_obj_set_style_radius(hl, 2, 0);
            lv_obj_set_style_bg_color(hl, p.button, 0);
            lv_obj_set_style_bg_opa(hl, LV_OPA_COVER, 0);
            lv_obj_align(hl, LV_ALIGN_TOP_LEFT, x - 3, y - 1);
        }

        auto* mark = lv_label_create(content_);
        lv_label_set_text(mark, on ? "[x]" : "[ ]");
        lv_obj_set_style_text_font(mark, face(f_label), 0);
        lv_obj_set_style_text_color(mark, on ? p.brass : p.text_disabled, 0);
        lv_obj_align(mark, LV_ALIGN_TOP_LEFT, x, y);

        auto* name = lv_label_create(content_);
        lv_label_set_text(name, fn.label);
        lv_obj_set_style_text_font(name, face(f_label), 0);
        lv_obj_set_style_text_color(name, on ? p.text : p.text_secondary, 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, x + 24, y);

        char cost[8];
        std::snprintf(cost, sizeof(cost), "%d", fn.cost);
        auto* c = lv_label_create(content_);
        lv_label_set_text(c, cost);
        lv_obj_set_style_text_font(c, face(f_note), 0);
        lv_obj_set_style_text_color(c, p.text_disabled, 0);
        lv_obj_align(c, LV_ALIGN_TOP_LEFT, x + kColumnWidth - 22, y + 1);
    }

    // --- what the selection means ------------------------------------------
    auto* foot = lv_label_create(content_);
    lv_label_set_text(foot, over ? "Over budget. Turn something off before deploying."
                                 : "Not deployed. Nothing has changed on the host.");
    lv_obj_set_style_text_font(foot, face(f_note), 0);
    lv_obj_set_style_text_color(foot, over ? p.error : p.text_disabled, 0);
    lv_obj_align(foot, LV_ALIGN_BOTTOM_LEFT, metric::kGutter, -4);
}

} // namespace screen
