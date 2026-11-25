/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "../src/model/gadget_budget.h"

#include <cstdio>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-58s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

} // namespace

int main() {
    using namespace model::gadget;

    std::printf("Endpoint budget\n\n  costs\n");
    check(endpoints_used(0) == 0, "nothing selected costs nothing");
    check(endpoints_used(0b000001) == 1, "keyboard alone costs 1");
    check(endpoints_used(0b000010) == 2, "RNDIS alone costs 2");
    check(endpoints_used(0b000011) == 3, "keyboard + RNDIS, the default, costs 3");
    check(endpoints_used(0b111111) == 9, "everything at once costs 9");

    std::printf("\n  the ceiling\n");
    check(!over_budget(0b000011), "the default selection fits");
    // The case P4wnP1's own comment calls out: mass storage on a device that
    // already has both network functions and a keyboard.
    constexpr std::int32_t kboard_rndis_ecm_ums = 0b101011;
    check(endpoints_used(kboard_rndis_ecm_ums) == 7, "keyboard + both nets + storage is exactly 7");
    check(!over_budget(kboard_rndis_ecm_ums), "...and that still fits, at the limit");
    check(over_budget(0b101111), "...adding raw HID to it does not");
    check(endpoints_used(0b101111) == 8, "...and the overflow is reported as 8");

    std::printf("\n  cursor\n");
    check(move_cursor(0, 1, kRowCount) == 1, "down moves one row");
    check(move_cursor(5, 1, kRowCount) == 0, "down from the last row wraps to the first");
    check(move_cursor(0, -1, kRowCount) == 5, "up from the first row wraps to the last");
    check(move_cursor(0, 3, kRowCount) == 3, "right jumps a column");
    check(move_cursor(3, 3, kRowCount) == 0, "right from the second column wraps back");
    check(move_cursor(0, 0, 0) == 0, "an empty list cannot move the cursor");
    check(move_cursor(2, 100, kRowCount) >= 0 && move_cursor(2, 100, kRowCount) < kRowCount,
          "a large delta still lands inside the list");
    check(move_cursor(2, -100, kRowCount) >= 0 && move_cursor(2, -100, kRowCount) < kRowCount,
          "a large negative delta still lands inside the list");

    std::printf("\n  toggle\n");
    check(toggle(0, 0, kRowCount) == 0b000001, "toggling an off row turns it on");
    check(toggle(0b000001, 0, kRowCount) == 0, "toggling an on row turns it off");
    check(toggle(0b000011, 2, kRowCount) == 0b000111, "toggling leaves other rows alone");
    check(toggle(0b000011, -1, kRowCount) == 0b000011, "a negative index changes nothing");
    check(toggle(0b000011, kRowCount, kRowCount) == 0b000011, "an index past the end changes nothing");

    std::printf("\n%s\n", failures == 0 ? "ALL BUDGET CHECKS PASS" : "BUDGET FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
