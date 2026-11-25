/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Replies are the shapes oled/client.go reads. If the service ever changes
 * them, these are what should fail.
 */

#include "../src/service/status_parse.h"

#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-62s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

} // namespace

int main() {
    using namespace service;

    std::printf("Status parsing\n\n  gadget settings\n");
    {
        StatusSnapshot s;
        const bool ok = parse_gadget_settings(
            R"({"use_HID_KEYBOARD":true,"use_RNDIS":true,"use_HID_MOUSE":false,"use_UMS":false})", s);
        check(ok, "a well-formed reply is accepted");
        check(s.usb_ok, "...and marks USB as answered");
        check(s.usb_composed, "...and composed, because something is enabled");
        check(s.usb_functions.size() == 2, "two functions are enabled");
        check(usb_summary(s) == "Keyboard, RNDIS net", "summary lists them in table order");
    }
    {
        StatusSnapshot s;
        check(parse_gadget_settings(R"({"use_HID_KEYBOARD":false})", s), "all-false is still an answer");
        check(s.usb_ok && !s.usb_composed, "...answered, but not composed");
        check(usb_summary(s) == "not composed", "...and says so rather than going blank");
    }
    {
        StatusSnapshot s;
        check(!parse_gadget_settings("not json at all", s), "malformed input is rejected");
        check(!s.usb_ok, "...and leaves USB unanswered, not 'answered with nothing'");
        check(usb_summary(s).empty(), "...so the summary is empty, not 'not composed'");
    }

    std::printf("\n  ethernet interfaces\n");
    {
        StatusSnapshot s;
        const bool ok = parse_ethernet_interfaces(
            R"({"list":[{"name":"usbeth","ipAddress4":"172.16.0.1","mode":"server"},
                       {"name":"wlan0","ipAddress4":"172.24.0.1","mode":"ap"}]})", s);
        check(ok, "a list of interfaces is accepted");
        check(s.net_ok && s.interfaces.size() == 2, "both interfaces are kept");
        check(interface_ip(s, "usbeth") == "172.16.0.1", "usbeth address is found by name");
        check(interface_ip(s, "wlan0") == "172.24.0.1", "wlan0 address is found by name");
        check(interface_ip(s, "bteth").empty(), "an absent interface yields no address");
    }
    {
        StatusSnapshot s;
        check(parse_ethernet_interfaces(R"({"list":[]})", s), "an empty list is a valid answer");
        check(s.net_ok && s.interfaces.empty(), "...answered, with no interfaces");
    }
    {
        StatusSnapshot s;
        check(!parse_ethernet_interfaces(R"({"wrong":"shape"})", s), "a reply with no list is rejected");
        check(!s.net_ok, "...and leaves the network unanswered");
    }
    {
        StatusSnapshot s;
        check(parse_ethernet_interfaces(R"({"list":[{"ipAddress4":"10.0.0.1"}]})", s),
              "an entry with no name is tolerated");
        check(s.interfaces.empty(), "...but dropped, since it cannot be looked up");
    }

    std::printf("\n  wifi\n");
    {
        StatusSnapshot s;
        check(parse_wifi_state(R"({"mode":"AP","ssid":"P4wnP1"})", s), "mode and ssid are accepted");
        check(s.wifi == "AP P4wnP1", "...and joined");
    }
    {
        StatusSnapshot s;
        check(parse_wifi_state(R"({"mode":"","ssid":""})", s), "an empty state is still an answer");
        check(s.wifi_ok && s.wifi == "idle", "...and reads as idle, not unknown");
    }
    {
        StatusSnapshot s;
        check(parse_wifi_state(R"({"mode":"AP"})", s), "mode without ssid is accepted");
        check(s.wifi == "AP", "...with no trailing separator");
    }

    std::printf("\n  reflexes\n");
    {
        StatusSnapshot s;
        check(parse_trigger_actions(
                  R"({"TriggerActions":[{"isActive":true},{"isActive":false},{"isActive":true}]})", s),
              "a trigger action set is accepted");
        check(s.reflexes == 3 && s.reflexes_armed == 2, "three present, two armed");
    }

    std::printf("\n  jobs\n");
    {
        StatusSnapshot s;
        check(parse_running_jobs(R"({"ids":[1,2]})", s), "running jobs are counted");
        check(s.jobs_running == 2, "...two of them");
    }
    {
        StatusSnapshot s;
        check(parse_running_jobs(R"({})", s), "an empty object is how no jobs is reported");
        check(s.jobs_ok && s.jobs_running == 0, "...answered, with zero running");
    }

    std::printf("\n%s\n", failures == 0 ? "ALL PARSE CHECKS PASS" : "PARSE FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
