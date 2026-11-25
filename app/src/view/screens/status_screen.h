/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "base_screen.h"
#include "freshness.h"
#include "status_parse.h"

namespace screen {

// How often the panel asks, and how long a successful read may go unrefreshed
// before the screen says the polling itself has stopped. The second is much
// larger than the first so an ordinary missed tick is not reported as a fault.
constexpr int kStatusPollMs = 5000;
constexpr int kStatusStaleMs = 20000;

class StatusScreen : public BaseScreen {
public:
    StatusScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~StatusScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    void render();
    void poll();

    static void poll_timer_cb(lv_timer_t* timer);

    lv_obj_t* content_{nullptr};
    lv_timer_t* poll_timer_{nullptr};
};

} // namespace screen
