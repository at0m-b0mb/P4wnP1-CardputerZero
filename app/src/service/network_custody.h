/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Who owns networking, and how it is always given back.
 *
 * P4wnP1's install.sh disables NetworkManager outright. On a CardputerZero
 * that breaks the device's own LaunchWizard and Settings, which depend on it
 * -- the same shape as the P4wnP1 lesson where adding auth broke the device's
 * own boot scripts. So the panel borrows networking when a mode is armed and
 * hands it back when disarmed, and never touches it at install time.
 *
 * The property that matters is not "disarm restores it". It is "it is
 * restored even if we never get to disarm": a crash, a kill, a power cut
 * mid-session, or the package being removed while armed.
 *
 * That is why custody is recorded on disk BEFORE NetworkManager is stopped
 * and cleared only AFTER it is started again. A marker that outlives the
 * process is the only thing that can tell the next run "you still owe this
 * back". These functions are pure decisions over observed state so every path
 * through them, including the ugly ones, can be tested without root.
 */

#pragma once

namespace service::netcustody {

// What we can see when we look, not what we believe.
struct Observed {
    bool marker_present = false;      // we recorded a borrow that was not returned
    bool nm_running = false;          // NetworkManager is up right now
    bool want_armed = false;          // the operator wants a network mode armed
};

enum class Step {
    WriteMarker,       // record the debt first, always before stopping anything
    StopNetworkManager,
    StartNetworkManager,
    ClearMarker,       // only once it is genuinely back
    Nothing,
};

// At most four steps, and in practice never more than two.
struct Plan {
    Step steps[4] = {Step::Nothing, Step::Nothing, Step::Nothing, Step::Nothing};
    int count = 0;

    void add(Step s) {
        if (count < 4) {
            steps[count++] = s;
        }
    }
    bool contains(Step s) const {
        for (int i = 0; i < count; ++i) {
            if (steps[i] == s) {
                return true;
            }
        }
        return false;
    }
};

// What to do, given what we see. Called on startup, on arm, on disarm and
// before removal -- the same function each time, because "what should be true
// now" does not depend on which event asked.
Plan reconcile(const Observed& observed);

// A plan is only safe if it never stops NetworkManager without having first
// recorded that we owe it back, and never clears the record while it is still
// down. Exposed so the test can assert it over every input rather than over
// the handful a human thought to write.
bool plan_is_safe(const Observed& observed, const Plan& plan);

} // namespace service::netcustody
