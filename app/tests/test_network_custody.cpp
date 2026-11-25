/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Every input combination, not a handful of chosen ones. There are only eight
 * of them, so there is no excuse for testing seven.
 */

#include "../src/service/network_custody.h"

#include <cstdio>

namespace {

using namespace service::netcustody;

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-64s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

const char* name(Step s) {
    switch (s) {
        case Step::WriteMarker: return "write-marker";
        case Step::StopNetworkManager: return "stop-nm";
        case Step::StartNetworkManager: return "start-nm";
        case Step::ClearMarker: return "clear-marker";
        case Step::Nothing: default: return "-";
    }
}

} // namespace

int main() {
    std::printf("Network custody\n\n  the exhaustive table\n");
    std::printf("    marker  nm_up  want_armed | plan\n");
    for (int bits = 0; bits < 8; ++bits) {
        Observed o;
        o.marker_present = bits & 4;
        o.nm_running     = bits & 2;
        o.want_armed     = bits & 1;

        const Plan plan = reconcile(o);
        std::printf("      %d       %d        %d       | ",
                    o.marker_present, o.nm_running, o.want_armed);
        if (plan.count == 0) {
            std::printf("nothing");
        }
        for (int i = 0; i < plan.count; ++i) {
            std::printf("%s%s", i ? ", " : "", name(plan.steps[i]));
        }
        std::printf("\n");
    }

    std::printf("\n  the safety property, over every input\n");
    {
        bool all_safe = true;
        for (int bits = 0; bits < 8; ++bits) {
            Observed o;
            o.marker_present = bits & 4;
            o.nm_running     = bits & 2;
            o.want_armed     = bits & 1;
            if (!plan_is_safe(o, reconcile(o))) {
                std::printf("      unsafe plan for marker=%d nm=%d armed=%d\n",
                            o.marker_present, o.nm_running, o.want_armed);
                all_safe = false;
            }
        }
        check(all_safe, "no input produces a plan that drops networking on the floor");
    }

    std::printf("\n  the cases worth naming\n");
    {
        Observed o; o.want_armed = true; o.nm_running = true;
        const Plan p = reconcile(o);
        check(p.count == 2 && p.steps[0] == Step::WriteMarker && p.steps[1] == Step::StopNetworkManager,
              "arming records the debt BEFORE stopping, never after");
    }
    {
        // The crash: we borrowed, then died. Next startup is not armed.
        Observed o; o.marker_present = true; o.nm_running = false; o.want_armed = false;
        const Plan p = reconcile(o);
        check(p.contains(Step::StartNetworkManager) && p.contains(Step::ClearMarker),
              "a crash while borrowed is repaired on the next run");
    }
    {
        // Removal while armed is the same shape as a crash: not armed any more.
        Observed o; o.marker_present = true; o.nm_running = false; o.want_armed = false;
        check(reconcile(o).contains(Step::StartNetworkManager),
              "uninstalling while armed still hands networking back");
    }
    {
        // Someone else stopped NetworkManager. Not ours to restore.
        Observed o; o.marker_present = false; o.nm_running = false; o.want_armed = false;
        check(reconcile(o).count == 0,
              "networking we never borrowed is left exactly as we found it");
    }
    {
        // Already armed and already stopped: arming again must be a no-op, not
        // a second marker or a second stop.
        Observed o; o.marker_present = true; o.nm_running = false; o.want_armed = true;
        check(reconcile(o).count == 0, "arming twice changes nothing the second time");
    }
    {
        // Disarmed, nothing owed, NetworkManager already up: nothing to do.
        Observed o; o.marker_present = false; o.nm_running = true; o.want_armed = false;
        check(reconcile(o).count == 0, "the idle steady state asks for no work");
    }
    {
        // Marker present and NM somehow already up: clear the debt, do not
        // start what is running.
        Observed o; o.marker_present = true; o.nm_running = true; o.want_armed = false;
        const Plan p = reconcile(o);
        check(!p.contains(Step::StartNetworkManager) && p.contains(Step::ClearMarker),
              "a debt against a running NetworkManager is just cleared");
    }

    std::printf("\n%s\n", failures == 0 ? "ALL CUSTODY CHECKS PASS" : "CUSTODY FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
