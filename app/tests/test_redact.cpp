/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "../src/service/redact.h"

#include <cstdio>
#include <string>

namespace {
using namespace service;
int failures = 0;
void check(bool ok, const char* what) {
    std::printf("    %-62s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) { ++failures; }
}
} // namespace

int main() {
    std::printf("Redaction\n\n  addresses\n");
    check(redact_ip("172.16.0.1") == "172.16.x.x", "the host part of an address is masked");
    check(redact_ip("192.168.1.44") == "192.168.x.x", "...on a home subnet too");
    check(redact_ip("10.0.0.1") == "10.0.x.x", "...and a ten-net");
    check(redact_ip("172.16.0.1").find("0.1") == std::string::npos,
          "...with no trace of the original host part");
    check(redact_ip("") == "", "an empty value is left alone");
    check(redact_ip("none") == "none", "the screen's own word for nothing is left alone");
    check(redact_ip("unknown") == "unknown", "...so is 'unknown'");
    check(redact_ip("172.16.0") == "172.16.0", "something that is not a dotted quad is untouched");
    check(redact_ip("1.2.3.4.5") == "1.2.3.4.5", "...five octets are not an address either");

    std::printf("\n  wi-fi\n");
    check(redact_wifi("AP HomeNetwork") == "AP <ssid>", "the SSID goes, the mode stays");
    check(redact_wifi("AP HomeNetwork").find("Home") == std::string::npos,
          "...with no trace of the name");
    check(redact_wifi("MyPrivateSSID") == "<ssid>", "a bare SSID is replaced wholesale");
    check(redact_wifi("idle") == "idle", "'idle' describes the radio, not a network");
    check(redact_wifi("unavailable") == "unavailable", "...so does 'unavailable'");
    check(redact_wifi("") == "", "...and empty stays empty");

    std::printf("\n  operator-chosen names\n");
    check(redact_name("my-office-rig", 0) == "item 1", "a loadout name is replaced");
    check(redact_name("exfil_to_mybox.js", 4) == "item 5", "...numbered so a list keeps its length");
    check(redact_name("", 0).empty(), "an empty name stays empty");

    std::printf("\n  the switch\n");
    {
        set_redaction(false);
        check(display_ip("172.16.0.1") == "172.16.0.1", "off by default: the real value is shown");
        check(display_wifi("AP HomeNetwork") == "AP HomeNetwork", "...and the real SSID");
        check(display_name("office-rig", 0) == "office-rig", "...and the real name");

        set_redaction(true);
        check(display_ip("172.16.0.1") == "172.16.x.x", "on: the address is masked");
        check(display_wifi("AP HomeNetwork") == "AP <ssid>", "...the SSID is masked");
        check(display_name("office-rig", 0) == "item 1", "...the name is masked");
        set_redaction(false);
    }

    std::printf("\n%s\n", failures == 0 ? "ALL REDACTION CHECKS PASS" : "REDACTION FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
