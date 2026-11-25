/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "base_screen.h"

namespace screen {

// What the device is presenting over USB, and how to reach it.
//
// Every field here is something the P4wnP1 service reports about itself. Until
// the RPC client lands, link_up is false and the screen says so rather than
// showing a plausible-looking device state that nothing measured.
struct StatusSnapshot {
    bool link_up = false;        // the service answered
    bool usb_composed = false;   // a gadget is deployed in configfs
    const char* usb_functions = "";
    const char* usbeth_addr = "";
    const char* wlan_addr = "";
    const char* ap_ssid = "";
    int jobs_running = 0;
    int endpoints_used = 0;
    int endpoints_total = 0;
};

class StatusScreen : public BaseScreen {
public:
    StatusScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
