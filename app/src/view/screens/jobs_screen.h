/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * What is running right now.
 *
 * A job list is the one screen that MUST poll: jobs end by themselves, so a
 * list fetched once is wrong within seconds and wrong in the dangerous
 * direction -- it shows something typing into a host that has already
 * finished, or hides one that is still going.
 *
 * The service gives ids and nothing else. There is no RPC that reports which
 * file a job was started from, so each row's label costs a second call and is
 * only fetched for rows that are actually on screen.
 */

#pragma once

#include "base_screen.h"

#include <string>
#include <vector>

namespace screen {

constexpr int kJobRowsVisible = 5;

// Faster than Status: this is the screen you watch while something runs.
constexpr int kJobsPollMs = 2000;
constexpr int kJobsStaleMs = 8000;

// Stop everything. Not confirmed: it only ever reduces what the attached host
// is being told, and the screen you are on is the one asking for it.
void cancel_all_jobs(viewmodel::BaseViewModel& view_model);

// How many jobs the last poll found.
int job_count();

class JobsScreen : public BaseScreen {
public:
    JobsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~JobsScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void render();
    void poll();

    static void poll_timer_cb(lv_timer_t* timer);
    static void state_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* content_{nullptr};
    lv_timer_t* poll_timer_{nullptr};
};

} // namespace screen
