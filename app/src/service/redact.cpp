/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "redact.h"

#include <cctype>
#include <cstdlib>
#include <string>

namespace service {
namespace {

int g_forced = -1;  // -1 unset, 0 off, 1 on

// Words the screens use to mean "there is nothing here". They describe the
// subsystem rather than the device's network, so redacting them would hide
// information while protecting nothing.
bool is_vocabulary(const std::string& v) {
    return v.empty() || v == "none" || v == "unknown" || v == "idle" ||
           v == "unavailable" || v == "not deployed";
}

bool is_dotted_quad(const std::string& v, std::size_t dots[3]) {
    int found = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const char c = v[i];
        if (c == '.') {
            if (found == 3) {
                return false;
            }
            dots[found++] = i;
        }
        else if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return found == 3 && !v.empty() && v.back() != '.' && v.front() != '.';
}

} // namespace

void set_redaction(bool on) {
    g_forced = on ? 1 : 0;
}

bool redaction_enabled() {
    if (g_forced >= 0) {
        return g_forced == 1;
    }
    const char* v = std::getenv("P4WNP1_REDACT");
    // Read once and cached, so a long screenshot run cannot half-apply it.
    g_forced = (v && *v && std::string(v) != "0") ? 1 : 0;
    return g_forced == 1;
}

std::string redact_ip(const std::string& value) {
    std::size_t dots[3] = {0, 0, 0};
    if (!is_dotted_quad(value, dots)) {
        return value;  // "none", a hostname, anything that is not an address
    }
    // Keep the network part, mask the host part: enough to show which subnet
    // without identifying the machine.
    return value.substr(0, dots[1]) + ".x.x";
}

std::string redact_wifi(const std::string& value) {
    if (is_vocabulary(value)) {
        return value;
    }
    // The snapshot stores "mode ssid" with a single space, and the mode is
    // worth keeping. A value with no space is an SSID on its own.
    const std::size_t space = value.find(' ');
    if (space == std::string::npos) {
        return "<ssid>";
    }
    return value.substr(0, space) + " <ssid>";
}

std::string redact_name(const std::string& value, int index) {
    if (value.empty()) {
        return value;
    }
    return "item " + std::to_string(index + 1);
}

std::string display_ip(const std::string& value) {
    return redaction_enabled() ? redact_ip(value) : value;
}

std::string display_wifi(const std::string& value) {
    return redaction_enabled() ? redact_wifi(value) : value;
}

std::string display_name(const std::string& value, int index) {
    return redaction_enabled() ? redact_name(value, index) : value;
}

} // namespace service
