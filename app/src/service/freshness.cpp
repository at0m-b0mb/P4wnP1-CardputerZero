/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "freshness.h"

namespace service {

Freshness assess(bool ever_reached,
                 std::int64_t last_ok_ms,
                 bool last_attempt_ok,
                 std::int64_t now_ms,
                 int stale_after_ms) {
    Freshness out;

    if (!ever_reached) {
        // Nothing has ever answered. There is no age, because there is no
        // reading to be old.
        out.state = Liveness::NeverReached;
        out.age_ms = 0;
        return out;
    }

    // A clock that went backwards must not produce a negative age that then
    // reads as "very fresh".
    const std::int64_t delta = now_ms - last_ok_ms;
    out.age_ms = delta < 0 ? 0 : static_cast<int>(delta);

    if (!last_attempt_ok) {
        // It answered before and does not now. Whatever is on screen is
        // last-known, and the headline must stop saying "up".
        out.state = Liveness::Lost;
        return out;
    }

    // The last attempt succeeded, so being old here means the poll itself is
    // not running, not that the service went away.
    out.state = out.age_ms >= stale_after_ms ? Liveness::Stale : Liveness::Live;
    return out;
}

const char* headline(Liveness state) {
    switch (state) {
        case Liveness::Live:
            return "Service up";
        case Liveness::Lost:
            return "Lost the service";
        case Liveness::Stale:
            return "Not refreshing";
        case Liveness::NeverReached:
        default:
            return "No service";
    }
}

} // namespace service
