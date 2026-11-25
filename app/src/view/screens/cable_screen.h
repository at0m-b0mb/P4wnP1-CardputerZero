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
