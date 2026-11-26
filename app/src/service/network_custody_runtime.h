/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Carrying out what network_custody decides.
 *
 * network_custody.{h,cpp} is a pure decision: given what we observe, what
 * should happen. This file is the half that touches the machine -- reading
 * whether NetworkManager is up, stopping and starting it, and recording the
 * debt on disk. Every command goes through an injectable Runner so the whole
 * thing can be exercised on a host that has neither systemd nor
 * NetworkManager, and so the macOS simulator can never pretend to have
 * managed a service it cannot see.
 *
 * STOP, NOT DISABLE. P4wnP1's own install.sh runs
 *
 *     systemctl disable NetworkManager
 *
 * which is persistent: it survives a reboot, and the CardputerZero's
 * LaunchWizard and Settings stay broken until someone re-enables it by hand.
 * This panel only ever STOPS the service, which is runtime state. A reboot
 * starts it again by itself, because it is still enabled.
 *
 * That is also why the marker lives under /run: a stop does not survive a
 * reboot, so neither should the record of it. /run is cleared on boot, which
 * is exactly the lifetime of the debt. Putting the marker somewhere
 * persistent would mean a power cut left a record of a stop that the boot had
 * already undone, and the next run would "restore" something nobody stopped.
 */

#pragma once

#include "network_custody.h"

#include <functional>
#include <string>
#include <vector>

namespace service::netcustody {

struct CommandResult {
    bool ran = false;       // false when the command could not be executed at all
    int exit_code = -1;
    std::string output;     // stdout and stderr together, trimmed
};

using Runner = std::function<CommandResult(const std::vector<std::string>&)>;

struct Effects {
    Runner run;
    std::string marker_path;
    bool dry_run = false;   // report what would happen; change nothing
};

// The real machine. On anything that is not Linux the runner refuses rather
// than failing obscurely, because a simulator reporting "stopped
// NetworkManager" would be a lie about a service that does not exist there.
Effects system_effects();

// Where the debt is recorded. Under /run, for the reason in the header above.
const char* default_marker_path();

const char* step_name(Step step);

struct Outcome {
    bool ok = false;
    bool changed = false;            // did anything actually happen
    std::vector<std::string> did;    // in order, for the log and the screen
    std::string error;               // empty when ok
    bool needs_root = false;         // the common, nameable failure
    bool unsupported = false;        // no systemd here at all
};

// What a look at the machine told us.
//
// `nm_known` is the difference between "NetworkManager is not running" and
// "I could not find out", and it has to be carried separately because the
// pure Observed has nowhere to put it. Collapsing the two is not a cosmetic
// loss: a failed probe that reads as "not running" produces a plan that
// writes the borrow marker and skips the stop, so the panel records that it
// took networking while taking nothing at all.
struct Reading {
    Observed observed;
    bool nm_known = false;
    std::string why;   // why not, when nm_known is false
};

// Read the machine. `want_armed` is the caller's intent, not an observation.
Reading observe(const Effects& fx, bool want_armed);

// Carry out a plan. Steps run in order and stop at the first failure, because
// the ordering is the safety property: a half-applied plan that stopped at
// the wrong point is still safe, one that carried on past a failure is not.
Outcome apply(const Effects& fx, const Plan& plan);

// Observe, decide, check the decision, then act.
//
// The plan_is_safe() guard is re-checked here even though reconcile() is the
// only thing that produces plans. It costs nothing, and the one failure mode
// worth refusing outright is a plan that stops NetworkManager without having
// recorded the debt first -- the exact state that leaves a device with no
// networking and nothing on disk saying why.
Outcome reconcile_now(const Effects& fx, bool want_armed);

} // namespace service::netcustody
