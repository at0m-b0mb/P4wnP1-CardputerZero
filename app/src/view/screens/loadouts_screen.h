/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Stored loadouts -- what P4wnP1 calls master templates.
 *
 * One loadout is a saved bundle of USB, network, Bluetooth and reflex
 * settings. Deploying one changes several subsystems at once, which is why
 * this screen asks before it does and names what will happen rather than
 * saying "apply?".
 */

#pragma once

#include "base_screen.h"

namespace screen {

// How many loadout rows fit the content area at once.
constexpr int kLoadoutRowsVisible = 5;

// Which action the confirm band is currently asking about. Deploying,
// pinning to boot and deleting all pause on the same band, and a band that
// did not know which it was would eventually confirm the wrong one.
enum class LoadoutAction {
    None,
    Deploy,
    SetStartup,
    Delete,
};

// Ask before acting. These only arm the band; nothing reaches the service
// until the operator answers it.
void arm_loadout_action(viewmodel::BaseViewModel& view_model, LoadoutAction action);

// Carry out whatever the band is asking about.
void confirm_loadout_action(viewmodel::BaseViewModel& view_model);

// Set what the band is asking about without going through a key press. Only
// the screenshot tooling uses this: every state an operator can reach should
// be capturable without reproducing it by hand.
void set_pending_loadout_action(LoadoutAction action);

// What is pending, so the key handler can tell an armed band from an idle one.
LoadoutAction pending_loadout_action();

// How many loadouts the last listing found, so the cursor knows its range.
int loadout_count();

class LoadoutsScreen : public BaseScreen {
public:
    LoadoutsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void render();

    static void state_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* content_{nullptr};
};

} // namespace screen
