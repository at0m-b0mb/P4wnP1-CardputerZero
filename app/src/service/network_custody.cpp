/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "network_custody.h"

namespace service::netcustody {

Plan reconcile(const Observed& o) {
    Plan plan;

    if (o.want_armed) {
        // Borrowing. Record the debt first: if we die between the marker and
        // the stop, the next run restores something that was never stopped,
        // which is harmless. Dying in the other order leaves NetworkManager
        // down with nothing on disk saying we did it, which is not.
        if (!o.marker_present) {
            plan.add(Step::WriteMarker);
        }
        if (o.nm_running) {
            plan.add(Step::StopNetworkManager);
        }
        return plan;
    }

    // Not armed. If we owe networking back, give it back. This is the same
    // path for a clean disarm, for a crash recovered on the next startup, and
    // for the package being removed -- none of them is a special case.
    if (o.marker_present) {
        if (!o.nm_running) {
            plan.add(Step::StartNetworkManager);
        }
        plan.add(Step::ClearMarker);
        return plan;
    }

    // Nothing owed. Notably we do NOT start NetworkManager here: if it is
    // down and we never borrowed it, someone else stopped it on purpose and
    // it is not ours to switch back on.
    return plan;
}

bool plan_is_safe(const Observed& o, const Plan& plan) {
    // 1. Never stop NetworkManager unless the debt is already recorded, either
    //    because it was recorded before or because this plan records it first.
    if (plan.contains(Step::StopNetworkManager)) {
        if (!o.marker_present) {
            bool recorded_first = false;
            for (int i = 0; i < plan.count; ++i) {
                if (plan.steps[i] == Step::WriteMarker) {
                    recorded_first = true;
                }
                if (plan.steps[i] == Step::StopNetworkManager) {
                    if (!recorded_first) {
                        return false;
                    }
                    break;
                }
            }
            if (!recorded_first) {
                return false;
            }
        }
    }

    // 2. Never clear the record while NetworkManager is still down: that is
    //    precisely the state where the next run has to know it owes a restore.
    if (plan.contains(Step::ClearMarker)) {
        const bool will_be_up = o.nm_running || plan.contains(Step::StartNetworkManager);
        if (!will_be_up) {
            return false;
        }
        // And never clear it before starting, for the same reason.
        for (int i = 0; i < plan.count; ++i) {
            if (plan.steps[i] == Step::ClearMarker) {
                if (!o.nm_running) {
                    return false;  // a start must have come earlier; it did not
                }
                break;
            }
            if (plan.steps[i] == Step::StartNetworkManager) {
                break;
            }
        }
    }

    // 3. Never both stop and start in one plan: that is a contradiction and
    //    means the inputs were misread.
    if (plan.contains(Step::StopNetworkManager) && plan.contains(Step::StartNetworkManager)) {
        return false;
    }

    return true;
}

} // namespace service::netcustody
