/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Turning P4wnP1's RPC replies into what the Status screen draws.
 *
 * Pure: strings in, a snapshot out, no sockets and no LVGL, so the parsing can
 * be tested on a host against recorded replies. The transport lives next door
 * in rpc_client.
 *
 * Shapes mirror what oled/client.go reads, which is the authority:
 *
 *   GetDeployedGadgetSetting              -> {"use_HID_KEYBOARD":true, ...}
 *   GetAllDeployedEthernetInterfaceSettings
 *                                         -> {"list":[{"name","ipAddress4","mode"}]}
 *   GetWiFiState                          -> {"mode","ssid"}
 *   GetDeployedTriggerActionSet           -> {"TriggerActions":[{"isActive"}]}
 *   HIDGetRunningScriptJobs               -> {"ids":[...]}
 */

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace service {

struct Interface {
    std::string name;
    std::string ip;
    std::string mode;
};

// One reply either answered or it did not. A field that was never fetched and
// a field that came back empty are different things, and the screen says so
// differently, so each group carries its own "did this answer" flag rather
// than relying on an empty value to mean "unknown".
struct StatusSnapshot {
    bool usb_ok = false;
    bool usb_composed = false;
    std::vector<std::string> usb_functions;

    bool net_ok = false;
    std::vector<Interface> interfaces;

    bool wifi_ok = false;
    std::string wifi;  // "mode ssid", or "idle" when both are empty

    bool reflexes_ok = false;
    int reflexes = 0;
    int reflexes_armed = 0;

    bool jobs_ok = false;
    int jobs_running = 0;
};

// Each returns false when the document cannot be read as the expected shape,
// leaving the corresponding *_ok flag clear. A malformed reply is treated as
// "did not answer", never as "answered with nothing".
bool parse_gadget_settings(const std::string& json, StatusSnapshot& out);
bool parse_ethernet_interfaces(const std::string& json, StatusSnapshot& out);
bool parse_wifi_state(const std::string& json, StatusSnapshot& out);
bool parse_trigger_actions(const std::string& json, StatusSnapshot& out);
bool parse_running_jobs(const std::string& json, StatusSnapshot& out);

// The toggle keys and their labels, in the order both screens list them.
// Shared so the parser and the deploy cannot disagree about what exists.
const std::vector<std::pair<std::string, std::string>>& usb_toggle_table();

// The label shown for a composed gadget, e.g. "Keyboard, RNDIS net".
std::string usb_summary(const StatusSnapshot& snapshot);

// The address of a named interface, empty when it is not in the list.
std::string interface_ip(const StatusSnapshot& snapshot, const std::string& name);

} // namespace service
