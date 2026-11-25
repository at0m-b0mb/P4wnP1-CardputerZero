/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "base_screen.h"

#include <cstddef>

namespace screen {

// THE ENDPOINT BUDGET.
//
// The dwc2 controller exposes a small, fixed number of USB endpoints and each
// gadget function spends some of them. Compose more than the budget and the
// deploy is refused -- correctly -- with a message that names a number and not
// a thing to turn off. So this screen counts before the deploy rather than
// after, and shows which function is spending what.
//
// Costs mirror endpointCost in P4wnP1's oled/client.go, which in turn mirrors
// service/SubSysUSB.go. Two copies of a number that must agree is exactly the
// arrangement that silently stops agreeing, so this one names its source.
//
// kEndpointBudget is 7 on a Pi Zero W. The CardputerZero is a CM0 and that has
// not been confirmed on hardware, so the figure is shown as the budget the app
// is assuming rather than as a measured fact.
constexpr int kEndpointBudget = 7;

struct GadgetFunction {
    const char* key;    // the service's toggle key
    const char* label;  // what it is called on screen
    int cost;           // endpoints spent when enabled
    bool enabled;       // local selection, not yet deployed
};

// How many rows the keypad can travel over. The nav bar needs the same number,
// so it is defined once here.
constexpr int kGadgetRows = 6;

// Deploy the current selection. Blocking: configfs teardown and rebuild is
// not instant, and there is nothing useful to do while it happens.
void deploy_selected(viewmodel::BaseViewModel& view_model);

// Whether the selection exceeds the endpoint budget. Checked before the
// confirm is even offered, so the operator is stopped by the panel with a
// readable reason rather than by the service with an arithmetic one.
bool selection_over_budget(const viewmodel::BaseViewModel& view_model);

class CableScreen : public BaseScreen {
public:
    CableScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void render();

    static void state_changed(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* content_{nullptr};
};

} // namespace screen
