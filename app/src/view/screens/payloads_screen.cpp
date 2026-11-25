/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "payloads_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "rpc_client.h"
#include "theme.h"
#include "ui_const.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

// Listed once per screen build rather than per render: a render happens on
// every cursor move and the list does not change under the operator's thumb.
std::vector<std::string> g_payloads;
std::string g_message;
int g_running_jobs = 0;

constexpr int kRowPitch = 14;

} // namespace

int payload_count() {
    return static_cast<int>(g_payloads.size());
}

void run_selected_payload(viewmodel::BaseViewModel& view_model) {
    const int32_t cursor = view_model.gadget_cursor();
    if (cursor < 0 || cursor >= static_cast<int32_t>(g_payloads.size())) {
        g_message = "nothing selected";
        view_model.set_deploy_state(4);
        return;
    }

    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(6000);  // covers the start, never the script
    const service::RunOutcome out = client.run_payload(g_payloads[static_cast<std::size_t>(cursor)]);

    if (out.ok) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "Started. Job %d is typing into the host.", out.job_id);
        g_message = buf;
        view_model.set_deploy_state(3);
    }
    else {
        g_message = out.error;
        view_model.set_deploy_state(4);
    }
}

void cancel_all_payloads(viewmodel::BaseViewModel& view_model) {
    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(4000);
    // One call, not two: asking twice would both double the work and risk
    // the message and the state disagreeing about what happened.
    const bool stopped = client.cancel_all_jobs();
    g_message = stopped ? "Stopped. Nothing is typing now."
                        : "Could not stop the running jobs.";
    view_model.set_deploy_state(stopped ? 3 : 4);
}

PayloadsScreen::PayloadsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void PayloadsScreen::state_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* self = static_cast<PayloadsScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->render();
    }
}

void PayloadsScreen::build_content(lv_obj_t* content) {
    content_ = content;

    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(1500);
    g_payloads = client.list_payloads();
    const service::StatusSnapshot s = client.fetch_status();
    g_running_jobs = s.jobs_running;

    // Reuse the Cable screen's cursor: only one list is ever on screen, and a
    // second cursor subject would be a second thing to keep in step.
    lv_subject_add_observer_obj(view_model().gadget_cursor_subject(), state_changed, content, this);
    lv_subject_add_observer_obj(view_model().deploy_state_subject(), state_changed, content, this);

    render();
}

void PayloadsScreen::render() {
    using namespace view;

    if (!content_ || !lv_obj_is_valid(content_)) {
        return;
    }
    lv_obj_clean(content_);

    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);
    const int32_t cursor = view_model().gadget_cursor();
    const int32_t state = view_model().deploy_state();

    auto* f_label = assets().load_standard_font(metric::kTextLabel, app::StandardFontWeight::Regular);
    auto* f_value = assets().load_standard_font(metric::kTextValue, app::StandardFontWeight::Bold);
    auto* f_note  = assets().load_standard_font(metric::kTextNote, app::StandardFontWeight::Regular);
    const auto face = [](lv_font_t* f) -> const lv_font_t* {
        return f ? f : &lv_font_montserrat_12;
    };

    if (g_payloads.empty()) {
        auto* none = lv_label_create(content_);
        lv_label_set_text(none, "No payloads");
        lv_obj_set_style_text_font(none, face(f_value), 0);
        lv_obj_set_style_text_color(none, p.text_secondary, 0);
        lv_obj_align(none, LV_ALIGN_TOP_LEFT, metric::kGutter, 6);

        auto* why = lv_label_create(content_);
        lv_label_set_text(why, "Either the service is not answering, or it has none stored.");
        lv_obj_set_style_text_font(why, face(f_note), 0);
        lv_obj_set_style_text_color(why, p.text_disabled, 0);
        lv_obj_set_width(why, kScreenWidth - (metric::kGutter * 2));
        lv_label_set_long_mode(why, LV_LABEL_LONG_WRAP);
        lv_obj_align(why, LV_ALIGN_TOP_LEFT, metric::kGutter, 25);
        return;
    }

    // --- header: how many, and what is already running ---------------------
    char head[48];
    std::snprintf(head, sizeof(head), "%d payloads", static_cast<int>(g_payloads.size()));
    auto* caption = lv_label_create(content_);
    lv_label_set_text(caption, head);
    lv_obj_set_style_text_font(caption, face(f_label), 0);
    lv_obj_set_style_text_color(caption, p.text_secondary, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, metric::kGutter, 3);

    if (g_running_jobs > 0) {
        char jobs[32];
        std::snprintf(jobs, sizeof(jobs), "%d running", g_running_jobs);
        auto* j = lv_label_create(content_);
        lv_label_set_text(j, jobs);
        lv_obj_set_style_text_font(j, face(f_value), 0);
        lv_obj_set_style_text_color(j, p.brass, 0);
        lv_obj_align(j, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 3);
    }

    auto* rule = lv_obj_create(content_);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, kScreenWidth - (metric::kGutter * 2), metric::kHairline);
    lv_obj_set_style_bg_color(rule, p.border, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_LEFT, metric::kGutter, 17);

    // --- the list, scrolled to keep the cursor in view ---------------------
    const int count = static_cast<int>(g_payloads.size());
    int first = 0;
    if (count > kPayloadRowsVisible) {
        first = cursor - (kPayloadRowsVisible / 2);
        if (first < 0) {
            first = 0;
        }
        if (first > count - kPayloadRowsVisible) {
            first = count - kPayloadRowsVisible;
        }
    }

    for (int row = 0; row < kPayloadRowsVisible && first + row < count; ++row) {
        const int index = first + row;
        const bool here = index == cursor;
        const int y = 22 + row * kRowPitch;

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
        lv_label_set_text(name, g_payloads[static_cast<std::size_t>(index)].c_str());
        lv_obj_set_style_text_font(name, here ? face(f_value) : face(f_label), 0);
        lv_obj_set_style_text_color(name, here ? p.text : p.text_secondary, 0);
        lv_obj_set_width(name, kScreenWidth - (metric::kGutter * 2) - 10);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, metric::kGutter, y);
    }

    // --- the band: confirm, or what just happened --------------------------
    if (state == 0) {
        return;
    }

    const char* message = nullptr;
    lv_color_t tone = p.text_disabled;
    switch (state) {
        case 1:
            // Named, not implied: this types into whatever is plugged in.
            message = "Run it? This types into the attached host.";
            tone = p.brass;
            break;
        case 2: message = "Starting..."; tone = p.text_secondary; break;
        case 3: message = g_message.c_str(); tone = p.success; break;
        case 4:
        default: message = g_message.empty() ? "It would not run." : g_message.c_str();
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
    lv_obj_align(text, LV_ALIGN_TOP_LEFT, metric::kGutter, 5);

    if (state == 1) {
        auto* keys = lv_label_create(band);
        lv_label_set_text(keys, "Y run    N cancel");
        lv_obj_set_style_text_font(keys, face(f_note), 0);
        lv_obj_set_style_text_color(keys, p.text_secondary, 0);
        lv_obj_align(keys, LV_ALIGN_TOP_LEFT, metric::kGutter, 19);
    }
}

} // namespace screen
