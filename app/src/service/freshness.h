/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * How old is what the screen is showing, and may it still be believed?
 *
 * A panel that reads the service once and then leaves the answer on screen is
 * quietly lying: "Service up" stays up long after the service has gone, and
 * every figure under it keeps its confident colour. The fix is not only to
 * poll — it is to say which of four situations we are in, because they call
 * for different words:
 *
 *   NeverReached  nothing has ever answered; there is nothing to show
 *   Live          the last read succeeded and is recent
 *   Stale         the last read succeeded but was a while ago, so the poll
 *                 itself may be wedged
 *   Lost          it answered before and does not now; the figures on screen
 *                 are last-known, not current, and must be labelled so
 *
 * Pure, so every transition can be tested without a clock or a socket.
 */

#pragma once

#include <cstdint>

namespace service {

enum class Liveness {
    NeverReached,
    Live,
    Stale,
    Lost,
};

struct Freshness {
    Liveness state = Liveness::NeverReached;
    int age_ms = 0;  // since the last SUCCESSFUL read; 0 when there has not been one
};

// stale_after_ms should be comfortably longer than the poll interval, so an
// ordinary gap between polls is not reported as a problem.
Freshness assess(bool ever_reached,
                 std::int64_t last_ok_ms,
                 bool last_attempt_ok,
                 std::int64_t now_ms,
                 int stale_after_ms);

// What the headline should say. Never "up" unless the most recent attempt
// actually succeeded.
const char* headline(Liveness state);

} // namespace service
