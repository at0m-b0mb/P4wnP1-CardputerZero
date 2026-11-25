/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "status_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"
#include "rpc_client.h"
#include "ui_const.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace screen {
namespace {

// Where the service is. Overridable so the panel can be pointed at a stand-in
// while there is no device, which is also how the screenshots are made.
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
    // The service's own local credential, the same file its CLI reads, so the
    // panel holds no credential of its own.
    return "/run/p4wnp1/local.token";
}

// Ask the service. Every field it does not answer stays at its "unknown"
// default and the screen says so, rather than showing a plausible device.
service::StatusSnapshot current_status() {
    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(1200);
    return client.fetch_status();
}

// Six rows plus a header have to live inside 110px, so the connected view
// uses a tighter pitch than the default row height.
constexpr int kConnectedRowPitch = 14;

std::string endpoint_label() {
    std::string base = api_base();
    const std::string scheme = "http://";
    if (base.rfind(scheme, 0) == 0) {
        base.erase(0, scheme.size());
    }
    return base;
}

} // namespace

StatusScreen::StatusScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void StatusScreen::build_content(lv_obj_t* content) {
    using namespace view;

    const service::StatusSnapshot status = current_status();
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
    const bool any_answer = status.usb_ok || status.net_ok || status.wifi_ok || status.jobs_ok;

    if (!any_answer) {
        text_at("No service", face(f_head), p.error, 6);
        text_at("Nothing here is measured.", face(f_note), p.text_secondary, 25);

        rule_at(46);

        // What it is waiting for, so the empty state is actionable rather than
        // just blank. Brass used once, on the one thing worth looking at.
        text_at("Waiting for a P4wnP1 service at", face(f_note), p.text_disabled, 56);
        text_at(endpoint_label().c_str(), face(f_value), p.brass, 72);
        return;
    }

    // Connected, so the statement is implied by there being figures at all.
    // It shrinks to one line and gives its space to the rows -- six of them do
    // not fit under a headline and a rule on a 110px content area.
    text_at("Service up", face(f_note), p.success, 2);
    {
        auto* where = lv_label_create(content);
        lv_label_set_text(where, endpoint_label().c_str());
        lv_obj_set_style_text_font(where, face(f_note), 0);
        lv_obj_set_style_text_color(where, p.text_disabled, 0);
        lv_obj_align(where, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 2);
    }
    rule_at(16);

    // --- the facts ---------------------------------------------------------
    int y = 22;
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
        y += kConnectedRowPitch;
    };

    char buf[64];
    const std::string usb = service::usb_summary(status);
    const std::string usbeth = service::interface_ip(status, "usbeth");
    const std::string wlan = service::interface_ip(status, "wlan0");

    row("USB", status.usb_ok ? usb.c_str() : "unknown",
        status.usb_composed ? p.text : p.text_disabled);
    row("usbeth", usbeth.empty() ? "none" : usbeth.c_str(),
        usbeth.empty() ? p.text_disabled : p.text);
    row("wlan0", wlan.empty() ? "none" : wlan.c_str(),
        wlan.empty() ? p.text_disabled : p.text);
    row("Wi-Fi", status.wifi_ok ? status.wifi.c_str() : "unavailable",
        status.wifi_ok ? p.text : p.text_disabled);

    if (status.reflexes_ok) {
        std::snprintf(buf, sizeof(buf), "%d of %d armed", status.reflexes_armed, status.reflexes);
        row("Reflexes", buf, status.reflexes_armed > 0 ? p.text : p.text_disabled);
    }
    std::snprintf(buf, sizeof(buf), "%d running", status.jobs_running);
    row("Jobs", buf, status.jobs_running > 0 ? p.text : p.text_disabled);
}

} // namespace screen
