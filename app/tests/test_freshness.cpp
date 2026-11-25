/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "../src/service/freshness.h"

#include <cstdio>
#include <cstring>

namespace {

using namespace service;

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-62s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

constexpr int STALE = 15000;  // a poll runs far more often than this

} // namespace

int main() {
    std::printf("Freshness\n\n  nothing has answered yet\n");
    {
        const Freshness f = assess(false, 0, false, 100000, STALE);
        check(f.state == Liveness::NeverReached, "never reached");
        check(f.age_ms == 0, "...and has no age, because there is no reading");
        check(std::strcmp(headline(f.state), "No service") == 0, "...headline says no service");
    }

    std::printf("\n  a healthy poll\n");
    {
        const Freshness f = assess(true, 99000, true, 100000, STALE);
        check(f.state == Liveness::Live, "a recent successful read is live");
        check(f.age_ms == 1000, "...one second old");
        check(std::strcmp(headline(f.state), "Service up") == 0, "...headline says up");
    }

    std::printf("\n  the service goes away\n");
    {
        // The case the whole module exists for: we had it, the latest attempt
        // failed, and the figures on screen are now last-known.
        const Freshness f = assess(true, 99000, false, 100000, STALE);
        check(f.state == Liveness::Lost, "a failed attempt after success is lost");
        check(std::strcmp(headline(f.state), "Service up") != 0,
              "...and the headline STOPS saying up");
        check(std::strcmp(headline(f.state), "Lost the service") == 0, "...it says lost");
        check(f.age_ms == 1000, "...and reports how old the figures are");
    }
    {
        // Still lost however long ago it was, and still carrying an age.
        const Freshness f = assess(true, 10000, false, 600000, STALE);
        check(f.state == Liveness::Lost, "still lost ten minutes later");
        check(f.age_ms == 590000, "...with the age grown accordingly");
    }

    std::printf("\n  the poll itself stops running\n");
    {
        // Last attempt succeeded, but that was ages ago, so nothing is
        // refreshing. Distinct from losing the service: the service may be
        // perfectly fine and it is the panel that has stopped asking.
        const Freshness f = assess(true, 10000, true, 100000, STALE);
        check(f.state == Liveness::Stale, "an old successful read is stale, not lost");
        check(std::strcmp(headline(f.state), "Not refreshing") == 0,
              "...and says the panel stopped asking, not that the service died");
    }
    {
        const Freshness f = assess(true, 100000 - STALE, true, 100000, STALE);
        check(f.state == Liveness::Stale, "exactly at the threshold counts as stale");
    }
    {
        const Freshness f = assess(true, 100000 - STALE + 1, true, 100000, STALE);
        check(f.state == Liveness::Live, "a millisecond inside it is still live");
    }

    std::printf("\n  a clock that misbehaves\n");
    {
        // Monotonic clocks are supposed to be monotonic. If one is not, a
        // negative age must not come out looking like the freshest reading
        // we have ever had.
        const Freshness f = assess(true, 200000, true, 100000, STALE);
        check(f.age_ms == 0, "a backwards clock clamps the age to zero");
        check(f.state == Liveness::Live, "...and does not invent staleness either");
    }

    std::printf("\n%s\n", failures == 0 ? "ALL FRESHNESS CHECKS PASS" : "FRESHNESS FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
