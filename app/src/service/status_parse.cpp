/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "status_parse.h"

#include <nlohmann/json.hpp>

#include <array>
#include <utility>

namespace service {
namespace {

using nlohmann::json;

// Toggle key to label, in the order the Cable screen lists them so a composed
// gadget reads the same on both screens. Mirrors usbToggles in
// P4wnP1's oled/client.go.
constexpr std::array<std::pair<const char*, const char*>, 7> kUsbToggles = {{
    {"use_HID_KEYBOARD", "Keyboard"},
    {"use_HID_MOUSE",    "Mouse"},
    {"use_HID_RAW",      "Raw HID"},
    {"use_RNDIS",        "RNDIS net"},
    {"use_CDC_ECM",      "CDC ECM net"},
    {"use_SERIAL",       "Serial"},
    {"use_UMS",          "Mass storage"},
}};

// A reply we cannot read is "did not answer", never "answered with nothing".
json parse_or_null(const std::string& text) {
    return json::parse(text, nullptr, false);
}

} // namespace

bool parse_gadget_settings(const std::string& text, StatusSnapshot& out) {
    const json doc = parse_or_null(text);
    if (doc.is_discarded() || !doc.is_object()) {
        return false;
    }

    out.usb_functions.clear();
    for (const auto& [key, label] : kUsbToggles) {
        const auto it = doc.find(key);
        if (it != doc.end() && it->is_boolean() && it->get<bool>()) {
            out.usb_functions.emplace_back(label);
        }
    }
    // The service answered, so we know the gadget state. "Composed" means a
    // gadget exists, which is not the same as any function being enabled.
    out.usb_ok = true;
    out.usb_composed = !out.usb_functions.empty();
    return true;
}

bool parse_ethernet_interfaces(const std::string& text, StatusSnapshot& out) {
    const json doc = parse_or_null(text);
    if (doc.is_discarded() || !doc.is_object()) {
        return false;
    }
    const auto list = doc.find("list");
    if (list == doc.end() || !list->is_array()) {
        return false;
    }

    out.interfaces.clear();
    for (const auto& entry : *list) {
        if (!entry.is_object()) {
            continue;
        }
        Interface iface;
        iface.name = entry.value("name", "");
        iface.ip   = entry.value("ipAddress4", "");
        iface.mode = entry.value("mode", "");
        if (!iface.name.empty()) {
            out.interfaces.push_back(std::move(iface));
        }
    }
    out.net_ok = true;
    return true;
}

bool parse_wifi_state(const std::string& text, StatusSnapshot& out) {
    const json doc = parse_or_null(text);
    if (doc.is_discarded() || !doc.is_object()) {
        return false;
    }

    const std::string mode = doc.value("mode", "");
    const std::string ssid = doc.value("ssid", "");

    std::string combined = mode;
    if (!ssid.empty()) {
        if (!combined.empty()) {
            combined += ' ';
        }
        combined += ssid;
    }
    // Answering with neither a mode nor an SSID is a real answer: the radio is
    // up and doing nothing. That is "idle", not "unknown".
    out.wifi = combined.empty() ? "idle" : combined;
    out.wifi_ok = true;
    return true;
}

bool parse_trigger_actions(const std::string& text, StatusSnapshot& out) {
    const json doc = parse_or_null(text);
    if (doc.is_discarded() || !doc.is_object()) {
        return false;
    }
    const auto list = doc.find("TriggerActions");
    if (list == doc.end() || !list->is_array()) {
        return false;
    }

    out.reflexes = 0;
    out.reflexes_armed = 0;
    for (const auto& entry : *list) {
        if (!entry.is_object()) {
            continue;
        }
        ++out.reflexes;
        if (entry.value("isActive", false)) {
            ++out.reflexes_armed;
        }
    }
    out.reflexes_ok = true;
    return true;
}

bool parse_running_jobs(const std::string& text, StatusSnapshot& out) {
    const json doc = parse_or_null(text);
    if (doc.is_discarded() || !doc.is_object()) {
        return false;
    }
    const auto ids = doc.find("ids");
    if (ids == doc.end() || !ids->is_array()) {
        // An empty reply with no "ids" key is how the service reports no jobs,
        // so that counts as answered with zero rather than as a failure.
        out.jobs_ok = true;
        out.jobs_running = 0;
        return true;
    }
    out.jobs_running = static_cast<int>(ids->size());
    out.jobs_ok = true;
    return true;
}

const std::vector<std::pair<std::string, std::string>>& usb_toggle_table() {
    static const std::vector<std::pair<std::string, std::string>> table = [] {
        std::vector<std::pair<std::string, std::string>> v;
        v.reserve(kUsbToggles.size());
        for (const auto& [key, label] : kUsbToggles) {
            v.emplace_back(key, label);
        }
        return v;
    }();
    return table;
}

namespace {

// Mirrors the two network functions in the toggle table. Named here rather
// than inferred from the label, because a label is for people.
bool is_network_key(const std::string& key) {
    return key == "use_RNDIS" || key == "use_CDC_ECM";
}

} // namespace

bool carries_network(const std::vector<std::string>& enabled_keys) {
    for (const auto& key : enabled_keys) {
        if (is_network_key(key)) {
            return true;
        }
    }
    return false;
}

bool carries_network(const StatusSnapshot& snapshot) {
    // snapshot.usb_functions holds LABELS ("RNDIS net"), not keys
    // ("use_RNDIS"). Comparing it against keys silently matches nothing, so
    // the labels are mapped back through the one shared table rather than
    // spelled out a second time here.
    for (const auto& label : snapshot.usb_functions) {
        for (const auto& [key, text] : usb_toggle_table()) {
            if (label == text && is_network_key(key)) {
                return true;
            }
        }
    }
    return false;
}

std::string usb_summary(const StatusSnapshot& snapshot) {
    if (!snapshot.usb_ok) {
        return "";
    }
    if (snapshot.usb_functions.empty()) {
        return "not composed";
    }
    std::string out;
    for (const auto& label : snapshot.usb_functions) {
        if (!out.empty()) {
            out += ", ";
        }
        out += label;
    }
    return out;
}

std::string interface_ip(const StatusSnapshot& snapshot, const std::string& name) {
    for (const auto& iface : snapshot.interfaces) {
        if (iface.name == name) {
            return iface.ip;
        }
    }
    return "";
}

} // namespace service
