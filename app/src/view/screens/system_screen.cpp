/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "rpc_client.h"
#include "theme.h"
#include "ui_const.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifndef P4WNP1_PANEL_VERSION
#define P4WNP1_PANEL_VERSION "unknown"
#endif

namespace screen {
namespace {

std::string api_base() {
    if (const char* v = std::getenv("P4WNP1_API")) {
        return v;
    }
    return "http://127.0.0.1:8000";
}

std::string token_path() {
    if (const char* v = std::getenv("P4WNP1_TOKEN")) {
        return v;
    }
    return "/run/p4wnp1/local.token";
}

std::string endpoint_label() {
    std::string base = api_base();
    const std::string scheme = "http://";
    if (base.rfind(scheme, 0) == 0) {
        base.erase(0, scheme.size());
    }
    return base;
}

bool g_service_up = false;
bool g_checked = false;
std::string g_message;
SystemAction g_pending = SystemAction::Identify;

// True once a reboot or shutdown has been asked for. From that moment the
// screen stops reporting state: anything it drew would be about a device that
// is going away, and the panel has no way to tell "it worked" from "the
// connection dropped because it worked".
bool g_going_down = false;

constexpr int kRowPitch = 13;

struct RowSpec {
    const char* name;
    const char* detail;
};

const RowSpec kRows[kSystemActionRows] = {
    {"Identify",  "blink the device LED"},
    {"Reboot",    "ends this session"},
    {"Shut down", "ends this session"},
};

} // namespace

void set_pending_system_action(SystemAction action) {
    g_pending = action;
}

void activate_system_row(viewmodel::BaseViewModel& view_model) {
    if (g_going_down || view_model.deploy_state() != 0) {
        return;
    }
    int32_t row = view_model.gadget_cursor();
    if (row < 0 || row >= kSystemActionRows) {
        row = 0;
    }
    g_pending = static_cast<SystemAction>(row);

    if (g_pending == SystemAction::Identify) {
        // Nothing to protect anyone from, so it just happens.
        service::RpcClient client(api_base(), token_path());
        client.set_timeout_ms(3000);
        const service::ActionOutcome out = client.set_led_blink(4);
        g_message = out.ok ? "Blinking the LED on this device."
                           : out.error;
        view_model.set_deploy_state(out.ok ? 3 : 4);
        return;
    }

    view_model.set_deploy_state(1);
}

void confirm_system_action(viewmodel::BaseViewModel& view_model) {
    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(4000);

    const bool rebooting = g_pending == SystemAction::Reboot;

    // Said BEFORE the call, because after it there may be nothing left to
    // report with. The band is already on screen when the device goes.
    g_going_down = true;
    g_message = rebooting ? "Rebooting. This panel is going away with it."
                          : "Shutting down. Wait for the LED before unplugging.";
    view_model.set_deploy_state(3);
    lv_refr_now(nullptr);

    const service::ActionOutcome out = rebooting ? client.reboot() : client.shutdown();

    if (!out.ok) {
        // It refused, or nothing answered -- so the device is NOT going down
        // and the screen must take back what it just said.
        g_going_down = false;
        g_message = out.error;
        view_model.set_deploy_state(4);
    }
}

SystemScreen::SystemScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void SystemScreen::state_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* self = static_cast<SystemScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->render();
    }
}

void SystemScreen::build_content(lv_obj_t* content) {
    content_ = content;

    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(1200);
    g_service_up = client.reachable();
    g_checked = true;

    lv_subject_add_observer_obj(view_model().gadget_cursor_subject(), state_changed, content, this);
    lv_subject_add_observer_obj(view_model().deploy_state_subject(), state_changed, content, this);

    render();
}

void SystemScreen::render() {
    using namespace view;

    if (!content_ || !lv_obj_is_valid(content_)) {
        return;
    }
    lv_obj_clean(content_);

    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);
    const int32_t state = view_model().deploy_state();
    int32_t cursor = view_model().gadget_cursor();
    if (cursor < 0 || cursor >= kSystemActionRows) {
        cursor = 0;
    }

    auto* f_label = assets().load_standard_font(metric::kTextLabel, app::StandardFontWeight::Regular);
    auto* f_value = assets().load_standard_font(metric::kTextValue, app::StandardFontWeight::Bold);
    auto* f_note  = assets().load_standard_font(metric::kTextNote, app::StandardFontWeight::Regular);
    const auto face = [](lv_font_t* f) -> const lv_font_t* {
        return f ? f : &lv_font_montserrat_12;
    };

    const auto text_at = [&](const char* s, const lv_font_t* font, lv_color_t colour, int y) {
        auto* l = lv_label_create(content_);
        lv_label_set_text(l, s);
        lv_obj_set_style_text_font(l, font, 0);
        lv_obj_set_style_text_color(l, colour, 0);
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, metric::kGutter, y);
        return l;
    };

    // --- what this build is ------------------------------------------------
    text_at("P4wnP1 panel " P4WNP1_PANEL_VERSION, face(f_value), p.text, 0);

    {
        const char* verdict = !g_checked   ? "service unchecked"
                            : g_service_up ? "service up"
                                           : "no service";
        auto* v = lv_label_create(content_);
        lv_label_set_text(v, verdict);
        lv_obj_set_style_text_font(v, face(f_note), 0);
        lv_obj_set_style_text_color(
            v, !g_checked ? p.text_disabled : (g_service_up ? p.success : p.error), 0);
        lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 4);
    }

    text_at(endpoint_label().c_str(), face(f_note), p.text_disabled, 15);

    auto* rule = lv_obj_create(content_);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, kScreenWidth - (metric::kGutter * 2), metric::kHairline);
    lv_obj_set_style_bg_color(rule, p.border, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_LEFT, metric::kGutter, 31);

    // --- the three rows ----------------------------------------------------
    for (int row = 0; row < kSystemActionRows; ++row) {
        const bool here = row == cursor;
        // Spaced by the real LINE HEIGHT (~14px), not the 11px nominal font
        // size -- measuring by the nominal size is what put the rule through
        // the bottom of "127.0.0.1:8099".
        //
        // 34, 47, 60: the last row ends at 74 and the confirm band's top
        // edge is at 76 (content 110 tall, band 34). The band cannot shrink
        // below 34 -- it holds a message line AND a key-hint line, and at 30
        // it cut "Y reboot  N cancel" in half -- so the rows move instead.
        const int y = 34 + row * kRowPitch;
        const bool grave = row != static_cast<int>(SystemAction::Identify);

        if (here) {
            auto* hl = lv_obj_create(content_);
            lv_obj_remove_style_all(hl);
            lv_obj_set_size(hl, kScreenWidth - (metric::kGutter * 2) + 6, kRowPitch - 1);
            lv_obj_set_style_radius(hl, 2, 0);
            lv_obj_set_style_bg_color(hl, p.button, 0);
            lv_obj_set_style_bg_opa(hl, LV_OPA_COVER, 0);
            lv_obj_align(hl, LV_ALIGN_TOP_LEFT, metric::kGutter - 3, y - 1);
        }

        auto* name = lv_label_create(content_);
        lv_label_set_text(name, kRows[row].name);
        lv_obj_set_style_text_font(name, here ? face(f_value) : face(f_label), 0);
        lv_obj_set_style_text_color(
            name, here ? (grave ? p.error : p.text) : p.text_secondary, 0);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, metric::kGutter, y);

        auto* detail = lv_label_create(content_);
        lv_label_set_text(detail, kRows[row].detail);
        lv_obj_set_style_text_font(detail, face(f_note), 0);
        lv_obj_set_style_text_color(detail, p.text_disabled, 0);
        lv_obj_align(detail, LV_ALIGN_TOP_LEFT, metric::kGutter + 88, y + 1);
    }

    // --- the standing caveat, not buried in a help page --------------------
    if (state == 0) {
        // One line, at a fixed y below the rows. Bottom-aligned and wrapping,
        // this ran its second line straight through the "Shut down" row: the
        // content area is 110px and the rows already reach 82.
        auto* note = lv_label_create(content_);
        lv_label_set_text(note, "Networking is borrowed, not taken.");
        lv_obj_set_style_text_font(note, face(f_note), 0);
        lv_obj_set_style_text_color(note, p.text_disabled, 0);
        lv_obj_set_width(note, kScreenWidth - (metric::kGutter * 2));
        lv_label_set_long_mode(note, LV_LABEL_LONG_DOT);
        lv_obj_align(note, LV_ALIGN_TOP_LEFT, metric::kGutter, 80);
        return;
    }

    // --- the band ----------------------------------------------------------
    const char* message = nullptr;
    lv_color_t tone = p.text_disabled;
    switch (state) {
        case 1:
            message = g_pending == SystemAction::Reboot
                          ? "Reboot? The panel goes down with it."
                          : "Shut down? It will not come back by itself.";
            tone = p.error;
            break;
        case 2: message = "Asking..."; tone = p.text_secondary; break;
        case 3: message = g_message.c_str(); tone = g_going_down ? p.warning : p.success; break;
        case 4:
        default:
            message = g_message.empty() ? "It would not do that." : g_message.c_str();
            tone = p.error;
            break;
    }

    constexpr int kBandHeight = 34;
    auto* band = lv_obj_create(content_);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, kScreenWidth, kBandHeight);
    lv_obj_set_style_bg_color(band, p.surface, 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(band, p.border, 0);
    lv_obj_set_style_border_width(band, metric::kHairline, 0);
    lv_obj_set_style_border_side(band, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(band, 0, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(band, LV_ALIGN_BOTTOM_MID, 0, 0);

    auto* text = lv_label_create(band);
    lv_label_set_text(text, message);
    lv_obj_set_style_text_font(text, face(f_note), 0);
    lv_obj_set_style_text_color(text, tone, 0);
    lv_obj_set_width(text, kScreenWidth - (metric::kGutter * 2));
    lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
    // An explicit single-line height is what makes LONG_DOT actually clip.
    // Without it the label is content-sized, a long message wraps to a second
    // line, and that line prints straight through the key hints below -- which
    // is how "Apply airgap? ..." came out overlapping "Y yes  N cancel".
    lv_obj_set_height(text, metric::kTextNote + 2);
    lv_obj_align(text, LV_ALIGN_TOP_LEFT, metric::kGutter, 5);

    if (state == 1) {
        auto* k = lv_label_create(band);
        lv_label_set_text(k, g_pending == SystemAction::Reboot ? "Y reboot    N cancel"
                                                               : "Y shut down    N cancel");
        lv_obj_set_style_text_font(k, face(f_note), 0);
        lv_obj_set_style_text_color(k, p.text_secondary, 0);
        lv_obj_align(k, LV_ALIGN_TOP_LEFT, metric::kGutter, 19);
    }
}

} // namespace screen
