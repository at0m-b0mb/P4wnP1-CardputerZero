/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "base_screen.h"

namespace screen {

// How many payload rows fit the content area at once.
constexpr int kPayloadRowsVisible = 5;

// Run the payload under the cursor. Blocking only for the start: the service
// returns as soon as the job is launched, never when the script finishes.
void run_selected_payload(viewmodel::BaseViewModel& view_model);

// Stop everything that is running.
void cancel_all_payloads(viewmodel::BaseViewModel& view_model);

// How many payloads the last listing found, so the key handler knows how far
// the cursor may travel.
int payload_count();

class PayloadsScreen : public BaseScreen {
public:
    PayloadsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void render();

    static void state_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* content_{nullptr};
};

} // namespace screen
