/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The device itself: what this build is, what it is talking to, and the two
 * actions that end the session.
 *
 * Reboot and shutdown kill the process drawing this screen, so neither can
 * report its own success. The screen says what is about to happen and then
 * stops claiming anything -- an "OK" that appears after a shutdown request
 * would be a message from a device on its way down, which is not a thing the
 * operator should be asked to interpret.
 */

#pragma once

#include "base_screen.h"

namespace screen {

// The rows that can be acted on, in the order they are drawn.
enum class SystemAction {
    Identify = 0,  // blink the LED, so you can tell which board is this one
    Reboot,
    Shutdown,
};

constexpr int kSystemActionRows = 3;

// Arm the confirm band for the row under the cursor. Identify needs no
// confirmation and runs immediately, because blinking an LED is not a thing
// anyone needs protecting from.
void activate_system_row(viewmodel::BaseViewModel& view_model);

// Carry out whatever the band is asking about.
void confirm_system_action(viewmodel::BaseViewModel& view_model);

// Screenshot tooling only, as on the Loadouts screen.
void set_pending_system_action(SystemAction action);

class SystemScreen : public BaseScreen {
public:
    SystemScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void render();

    static void state_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* content_{nullptr};
};

} // namespace screen
