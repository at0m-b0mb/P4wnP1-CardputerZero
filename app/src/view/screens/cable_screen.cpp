/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "cable_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "network_custody_runtime.h"
#include "rpc_client.h"
#include "theme.h"
#include "ui_const.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

// Mirrors the Status screen's resolution of where the service is.
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

// Held across a re-render so the result of the last deploy can be shown.
std::string g_last_error;
// What custody did alongside the deploy, shown under the result.
std::string g_custody_note;

} // namespace


void deploy_selected(viewmodel::BaseViewModel& view_model) {
    std::vector<std::string> keys;
    const int32_t mask = view_model.gadget_mask();
    for (int i = 0; i < kGadgetRows; ++i) {
        if (mask & (1 << i)) {
            keys.emplace_back(kFunctions[static_cast<std::size_t>(i)].key);
        }
    }

    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(8000);  // configfs teardown and rebuild is not instant
    const service::DeployOutcome out = client.deploy_gadget(keys);
    g_last_error = out.error;

    if (out.ok) {
        // The deploy decided whether P4wnP1 now wants the network stack, so
        // this is the moment networking is armed or handed back -- not
        // install time, and not a separate switch the operator has to
        // remember. A keyboard-only gadget gives it back.
        //
        // A custody failure does NOT fail the deploy: the gadget really is
        // deployed, and saying otherwise would be the larger lie. It is
        // reported alongside, because a device whose networking did not come
        // back is something the operator has to know about.
        const bool armed = service::carries_network(keys);
        const service::netcustody::Outcome custody =
            service::netcustody::reconcile_now(service::netcustody::system_effects(), armed);
        g_custody_note.clear();
        if (!custody.ok) {
            g_custody_note = custody.needs_root
                                 ? "Deployed, but NetworkManager needs root to pause."
                                 : "Deployed. Networking untouched: " + custody.error;
        }
        else if (custody.changed) {
            g_custody_note = armed ? "NetworkManager paused while this gadget is up."
                                   : "NetworkManager running again.";
        }
    }

    view_model.set_deploy_state(out.ok ? 3 : 4);
}

bool selection_carries_network(const viewmodel::BaseViewModel& view_model) {
    const int32_t mask = const_cast<viewmodel::BaseViewModel&>(view_model).gadget_mask();
    std::vector<std::string> keys;
    for (int i = 0; i < kGadgetRows; ++i) {
        if (mask & (1 << i)) {
            keys.emplace_back(kFunctions[static_cast<std::size_t>(i)].key);
        }
    }
    return service::carries_network(keys);
}

bool selection_over_budget(const viewmodel::BaseViewModel& view_model) {
    const int32_t mask = const_cast<viewmodel::BaseViewModel&>(view_model).gadget_mask();
    int used = 0;
    for (int i = 0; i < kGadgetRows; ++i) {
        if (mask & (1 << i)) {
            used += kFunctions[static_cast<std::size_t>(i)].cost;
        }
    }
    return used > kEndpointBudget;
}


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
    lv_subject_add_observer_obj(view_model().deploy_state_subject(), state_changed, content, this);

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

    // --- what the selection means, or what just happened -------------------
    const int32_t deploy_state = view_model().deploy_state();

    const char* message = nullptr;
    lv_color_t tone = p.text_disabled;
    switch (deploy_state) {
        case 1:
            // The consequence is named, because re-enumeration is something
            // the attached host notices and the operator may not expect.
            // Naming the second consequence only when it applies. A
            // keyboard-only gadget does not pause NetworkManager, and
            // warning about it anyway would train the operator to ignore
            // the line that matters.
            message = selection_carries_network(view_model())
                          ? "Deploy? USB re-enumerates and NetworkManager pauses."
                          : "Deploy? The host's USB link re-enumerates.";
            tone = p.brass;
            break;
        case 2: message = "Deploying..."; tone = p.text_secondary; break;
        case 3:
            message = g_custody_note.empty() ? "Deployed. The host sees the new gadget."
                                             : g_custody_note.c_str();
            tone = p.success;
            break;
        case 4:
            message = g_last_error.empty() ? "The deploy was refused." : g_last_error.c_str();
            tone = p.error;
            break;
        default:
            message = over ? "Over budget. Turn something off before deploying."
                           : "Not deployed. Nothing has changed on the host.";
            tone = over ? p.error : p.text_disabled;
            break;
    }

    if (deploy_state == 0) {
        // Idle: a quiet footer line under the rows.
        auto* foot = lv_label_create(content_);
        lv_label_set_text(foot, message);
        lv_obj_set_style_text_font(foot, face(f_note), 0);
        lv_obj_set_style_text_color(foot, tone, 0);
        lv_obj_set_width(foot, kScreenWidth - (metric::kGutter * 2));
        lv_label_set_long_mode(foot, LV_LABEL_LONG_DOT);
        lv_obj_align(foot, LV_ALIGN_BOTTOM_LEFT, metric::kGutter, -2);
        return;
    }

    // Anything else is a moment that wants the operator's attention, so it is
    // an opaque band rather than text floating over the rows it would
    // otherwise collide with.
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

    if (deploy_state == 1) {
        auto* keys = lv_label_create(band);
        lv_label_set_text(keys, "Y deploy    N cancel");
        lv_obj_set_style_text_font(keys, face(f_note), 0);
        lv_obj_set_style_text_color(keys, p.text_secondary, 0);
        lv_obj_align(keys, LV_ALIGN_TOP_LEFT, metric::kGutter, 19);
    }
}

} // namespace screen
