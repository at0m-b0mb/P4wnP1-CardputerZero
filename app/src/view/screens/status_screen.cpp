/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "status_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "rpc_client.h"
#include "theme.h"
#include "ui_const.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

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
    // The service's own local credential, the same file its CLI reads, so the
    // panel holds no credential of its own.
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

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Last known reading, kept across screen rebuilds so switching pages does not
// throw away what we were told.
service::StatusSnapshot g_snapshot;
bool g_ever_reached = false;
bool g_last_attempt_ok = false;
std::int64_t g_last_ok_ms = 0;

void age_text(char* buf, std::size_t n, int age_ms) {
    const int s = age_ms / 1000;
    if (s < 60) {
        std::snprintf(buf, n, "%ds ago", s);
    }
    else {
        std::snprintf(buf, n, "%dm ago", s / 60);
    }
}

} // namespace

StatusScreen::StatusScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

StatusScreen::~StatusScreen() {
    // The screen outlives nothing: if the timer did, it would fire against a
    // destroyed object the next time the page changed.
    if (poll_timer_) {
        lv_timer_delete(poll_timer_);
        poll_timer_ = nullptr;
    }
}

void StatusScreen::poll_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<StatusScreen*>(lv_timer_get_user_data(timer));
    if (self) {
        self->poll();
    }
}

void StatusScreen::poll() {
    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(1200);
    const service::StatusSnapshot fresh = client.fetch_status();

    // "Answered" means at least one of the five reads came back. A device
    // whose Wi-Fi subsystem is unavailable still counts as reachable.
    const bool answered = fresh.usb_ok || fresh.net_ok || fresh.wifi_ok || fresh.jobs_ok;
    g_last_attempt_ok = answered;
    if (answered) {
        g_snapshot = fresh;
        g_ever_reached = true;
        g_last_ok_ms = now_ms();
    }
    render();
}

void StatusScreen::build_content(lv_obj_t* content) {
    content_ = content;
    poll();
    poll_timer_ = lv_timer_create(poll_timer_cb, kStatusPollMs, this);
}

void StatusScreen::render() {
    using namespace view;

    if (!content_ || !lv_obj_is_valid(content_)) {
        return;
    }
    lv_obj_clean(content_);

    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);
    const service::Freshness fresh = service::assess(
        g_ever_reached, g_last_ok_ms, g_last_attempt_ok, now_ms(), kStatusStaleMs);

    auto* f_head  = assets().load_standard_font(metric::kTextTitle, app::StandardFontWeight::Bold);
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

    const auto rule_at = [&](int y) {
        auto* r = lv_obj_create(content_);
        lv_obj_remove_style_all(r);
        lv_obj_set_size(r, kScreenWidth - (metric::kGutter * 2), metric::kHairline);
        lv_obj_set_style_bg_color(r, p.border, 0);
        lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
        lv_obj_align(r, LV_ALIGN_TOP_LEFT, metric::kGutter, y);
    };

    // --- nothing has ever answered ----------------------------------------
    if (fresh.state == service::Liveness::NeverReached) {
        text_at(service::headline(fresh.state), face(f_head), p.error, 6);
        text_at("Nothing here is measured.", face(f_note), p.text_secondary, 25);
        rule_at(46);
        text_at("Waiting for a P4wnP1 service at", face(f_note), p.text_disabled, 56);
        text_at(endpoint_label().c_str(), face(f_value), p.brass, 72);
        return;
    }

    // --- there is a reading, fresh or not ---------------------------------
    const bool live = fresh.state == service::Liveness::Live;
    const lv_color_t head_colour = live ? p.success
                                        : (fresh.state == service::Liveness::Lost ? p.error
                                                                                  : p.warning);
    text_at(service::headline(fresh.state), face(f_note), head_colour, 2);

    // The age is the whole point: a figure with no timestamp invites the
    // reader to assume it is current.
    char stamp[32];
    if (live) {
        std::snprintf(stamp, sizeof(stamp), "%s", endpoint_label().c_str());
    }
    else {
        char age[24];
        age_text(age, sizeof(age), fresh.age_ms);
        std::snprintf(stamp, sizeof(stamp), "last read %s", age);
    }
    {
        auto* where = lv_label_create(content_);
        lv_label_set_text(where, stamp);
        lv_obj_set_style_text_font(where, face(f_note), 0);
        lv_obj_set_style_text_color(where, live ? p.text_disabled : head_colour, 0);
        lv_obj_align(where, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 2);
    }
    rule_at(16);

    // When the reading is not current, every figure under it is last-known.
    // Dimming them is the difference between reporting and asserting.
    const lv_color_t value_colour = live ? p.text : p.text_disabled;
    const lv_color_t dim_colour = p.text_disabled;

    int y = 22;
    const auto row = [&](const char* name, const char* value, bool emphasised) {
        auto* k = lv_label_create(content_);
        lv_label_set_text(k, name);
        lv_obj_set_style_text_font(k, face(f_label), 0);
        lv_obj_set_style_text_color(k, p.text_secondary, 0);
        lv_obj_align(k, LV_ALIGN_TOP_LEFT, metric::kGutter, y);

        auto* v = lv_label_create(content_);
        lv_label_set_text(v, value);
        lv_obj_set_style_text_font(v, face(f_value), 0);
        lv_obj_set_style_text_color(v, emphasised ? value_colour : dim_colour, 0);
        lv_obj_align(v, LV_ALIGN_TOP_LEFT, metric::kGutter + 84, y);
        y += 14;
    };

    char buf[64];
    const std::string usb = service::usb_summary(g_snapshot);
    const std::string usbeth = service::interface_ip(g_snapshot, "usbeth");
    const std::string wlan = service::interface_ip(g_snapshot, "wlan0");

    row("USB", g_snapshot.usb_ok ? usb.c_str() : "unknown", g_snapshot.usb_composed);
    row("usbeth", usbeth.empty() ? "none" : usbeth.c_str(), !usbeth.empty());
    row("wlan0", wlan.empty() ? "none" : wlan.c_str(), !wlan.empty());
    row("Wi-Fi", g_snapshot.wifi_ok ? g_snapshot.wifi.c_str() : "unavailable", g_snapshot.wifi_ok);

    if (g_snapshot.reflexes_ok) {
        std::snprintf(buf, sizeof(buf), "%d of %d armed",
                      g_snapshot.reflexes_armed, g_snapshot.reflexes);
        row("Reflexes", buf, g_snapshot.reflexes_armed > 0);
    }
    std::snprintf(buf, sizeof(buf), "%d running", g_snapshot.jobs_running);
    row("Jobs", buf, g_snapshot.jobs_running > 0);
}

} // namespace screen
