/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "jobs_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "freshness.h"
#include "rpc_client.h"
#include "theme.h"
#include "ui_const.h"

#include <chrono>
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

std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

struct Row {
    int id = 0;
    std::string label;
};

// Survives screen rebuilds, so paging away and back does not blank the list.
std::vector<Row> g_rows;
bool g_ever_reached = false;
bool g_last_attempt_ok = false;
std::int64_t g_last_ok_ms = 0;
std::string g_message;

constexpr int kRowPitch = 14;

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

int job_count() {
    return static_cast<int>(g_rows.size());
}

void cancel_all_jobs(viewmodel::BaseViewModel& view_model) {
    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(4000);
    const bool stopped = client.cancel_all_jobs();
    g_message = stopped ? "Stopped. Nothing is typing now."
                        : "Could not stop the running jobs.";
    view_model.set_deploy_state(stopped ? 3 : 4);
}

JobsScreen::JobsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

JobsScreen::~JobsScreen() {
    // The timer holds a this pointer. Left alive it fires against a destroyed
    // screen the next time the page changes.
    if (poll_timer_) {
        lv_timer_delete(poll_timer_);
        poll_timer_ = nullptr;
    }
}

void JobsScreen::poll_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<JobsScreen*>(lv_timer_get_user_data(timer));
    if (self) {
        self->poll();
    }
}

void JobsScreen::state_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* self = static_cast<JobsScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->render();
    }
}

void JobsScreen::poll() {
    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(1200);

    const service::JobIds ids = client.running_job_ids();
    g_last_attempt_ok = ids.ok;

    if (ids.ok) {
        std::vector<Row> rows;
        rows.reserve(ids.ids.size());
        for (std::size_t i = 0; i < ids.ids.size(); ++i) {
            Row row;
            row.id = ids.ids[i];
            // One extra call per row, so only for rows that will be drawn.
            // A list of forty jobs must not cost forty round trips to render
            // five lines.
            if (i < static_cast<std::size_t>(kJobRowsVisible)) {
                row.label = client.job_label(row.id);
            }
            rows.push_back(row);
        }
        g_rows = std::move(rows);
        g_ever_reached = true;
        g_last_ok_ms = now_ms();
    }

    render();
}

void JobsScreen::build_content(lv_obj_t* content) {
    content_ = content;

    lv_subject_add_observer_obj(view_model().deploy_state_subject(), state_changed, content, this);

    poll();
    poll_timer_ = lv_timer_create(poll_timer_cb, kJobsPollMs, this);
}

void JobsScreen::render() {
    using namespace view;

    if (!content_ || !lv_obj_is_valid(content_)) {
        return;
    }
    lv_obj_clean(content_);

    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);
    const int32_t state = view_model().deploy_state();
    const service::Freshness fresh = service::assess(
        g_ever_reached, g_last_ok_ms, g_last_attempt_ok, now_ms(), kJobsStaleMs);

    auto* f_head  = assets().load_standard_font(metric::kTextTitle, app::StandardFontWeight::Bold);
    auto* f_label = assets().load_standard_font(metric::kTextLabel, app::StandardFontWeight::Regular);
    auto* f_value = assets().load_standard_font(metric::kTextValue, app::StandardFontWeight::Bold);
    auto* f_note  = assets().load_standard_font(metric::kTextNote, app::StandardFontWeight::Regular);
    const auto face = [](lv_font_t* f) -> const lv_font_t* {
        return f ? f : &lv_font_montserrat_12;
    };

    // --- never got an answer ----------------------------------------------
    if (fresh.state == service::Liveness::NeverReached) {
        auto* none = lv_label_create(content_);
        lv_label_set_text(none, "No service");
        lv_obj_set_style_text_font(none, face(f_head), 0);
        lv_obj_set_style_text_color(none, p.error, 0);
        lv_obj_align(none, LV_ALIGN_TOP_LEFT, metric::kGutter, 6);

        auto* why = lv_label_create(content_);
        lv_label_set_text(why, "Whether anything is running cannot be known from here.");
        lv_obj_set_style_text_font(why, face(f_note), 0);
        lv_obj_set_style_text_color(why, p.text_disabled, 0);
        lv_obj_set_width(why, kScreenWidth - (metric::kGutter * 2));
        lv_label_set_long_mode(why, LV_LABEL_LONG_WRAP);
        lv_obj_align(why, LV_ALIGN_TOP_LEFT, metric::kGutter, 27);
        return;
    }

    const bool live = fresh.state == service::Liveness::Live;

    // --- header ------------------------------------------------------------
    char head[48];
    if (g_rows.empty()) {
        std::snprintf(head, sizeof(head), "Nothing running");
    }
    else {
        std::snprintf(head, sizeof(head), "%d running", static_cast<int>(g_rows.size()));
    }
    auto* caption = lv_label_create(content_);
    lv_label_set_text(caption, head);
    lv_obj_set_style_text_font(caption, face(f_label), 0);
    lv_obj_set_style_text_color(caption,
                                g_rows.empty() ? p.text_secondary
                                               : (live ? p.brass : p.text_disabled), 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, metric::kGutter, 3);

    {
        // The age matters more here than anywhere: this is the screen you
        // read to decide whether a payload has finished.
        char stamp[40];
        if (live) {
            std::snprintf(stamp, sizeof(stamp), "live");
        }
        else {
            char age[24];
            age_text(age, sizeof(age), fresh.age_ms);
            std::snprintf(stamp, sizeof(stamp), "%s, last read %s",
                          service::headline(fresh.state), age);
        }
        auto* s = lv_label_create(content_);
        lv_label_set_text(s, stamp);
        lv_obj_set_style_text_font(s, face(f_note), 0);
        lv_obj_set_style_text_color(
            s, live ? p.success
                    : (fresh.state == service::Liveness::Lost ? p.error : p.warning), 0);
        lv_obj_align(s, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 4);
    }

    auto* rule = lv_obj_create(content_);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, kScreenWidth - (metric::kGutter * 2), metric::kHairline);
    lv_obj_set_style_bg_color(rule, p.border, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_LEFT, metric::kGutter, 17);

    // --- nothing running is a fine answer, said plainly --------------------
    if (g_rows.empty()) {
        auto* quiet = lv_label_create(content_);
        lv_label_set_text(quiet, live
                                     ? "No payload is typing into the attached host."
                                     : "Nothing was running when this was last read.");
        lv_obj_set_style_text_font(quiet, face(f_note), 0);
        lv_obj_set_style_text_color(quiet, p.text_disabled, 0);
        lv_obj_set_width(quiet, kScreenWidth - (metric::kGutter * 2));
        lv_label_set_long_mode(quiet, LV_LABEL_LONG_WRAP);
        lv_obj_align(quiet, LV_ALIGN_TOP_LEFT, metric::kGutter, 26);
        return;
    }

    // --- the rows ----------------------------------------------------------
    const lv_color_t id_colour = live ? p.text : p.text_disabled;
    const int count = static_cast<int>(g_rows.size());

    for (int row = 0; row < kJobRowsVisible && row < count; ++row) {
        const Row& job = g_rows[static_cast<std::size_t>(row)];
        const int y = 22 + row * kRowPitch;

        char id_text[16];
        std::snprintf(id_text, sizeof(id_text), "job %d", job.id);
        auto* id_label = lv_label_create(content_);
        lv_label_set_text(id_label, id_text);
        lv_obj_set_style_text_font(id_label, face(f_value), 0);
        lv_obj_set_style_text_color(id_label, id_colour, 0);
        lv_obj_align(id_label, LV_ALIGN_TOP_LEFT, metric::kGutter, y);

        // No invented name. Where the source gave nothing usable the row
        // stays the id alone and says so, rather than showing a filename the
        // service never told us.
        auto* name = lv_label_create(content_);
        lv_label_set_text(name, job.label.empty() ? "source has no title" : job.label.c_str());
        lv_obj_set_style_text_font(name, face(f_label), 0);
        lv_obj_set_style_text_color(name, job.label.empty() ? p.text_disabled
                                                            : p.text_secondary, 0);
        lv_obj_set_width(name, kScreenWidth - (metric::kGutter * 2) - 56);
        lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, metric::kGutter + 52, y);
    }

    if (count > kJobRowsVisible) {
        char more[32];
        std::snprintf(more, sizeof(more), "+%d more", count - kJobRowsVisible);
        auto* m = lv_label_create(content_);
        lv_label_set_text(m, more);
        lv_obj_set_style_text_font(m, face(f_note), 0);
        lv_obj_set_style_text_color(m, p.text_disabled, 0);
        lv_obj_align(m, LV_ALIGN_TOP_LEFT, metric::kGutter, 22 + kJobRowsVisible * kRowPitch);
    }

    // --- the band ----------------------------------------------------------
    if (state == 0) {
        return;
    }

    const char* message = nullptr;
    lv_color_t tone = p.text_disabled;
    switch (state) {
        case 2: message = "Stopping..."; tone = p.text_secondary; break;
        case 3: message = g_message.c_str(); tone = p.success; break;
        case 4:
        default:
            message = g_message.empty() ? "It would not stop." : g_message.c_str();
            tone = p.error;
            break;
    }

    constexpr int kBandHeight = 24;
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
}

} // namespace screen
