/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The endpoint arithmetic, with no LVGL and no view model attached, so it can
 * be tested on a host without a display. Everything the Cable screen decides
 * about cost and selection goes through here.
 */

#pragma once

#include <cstdint>

namespace model::gadget {

// Endpoint cost per function, indexed by row. Order fixes the bit positions in
// the selection mask, so rows may be relabelled but not reordered without
// migrating saved selections.
//
// Mirrors endpointCost in P4wnP1's oled/client.go, which mirrors
// service/SubSysUSB.go. Two copies of a number that must agree is exactly the
// arrangement that silently stops agreeing, so this one names its source and
// the test below pins the totals.
inline constexpr int kRowCount = 6;
inline constexpr int kCost[kRowCount] = {
    1, // HID keyboard
    2, // RNDIS net
    1, // HID mouse
    2, // CDC ECM net
    1, // Raw HID
    2, // Mass storage
};

// 7 on a Pi Zero W. The CardputerZero is a CM0 and that has not been confirmed
// on hardware, so this is the budget the app assumes, not a measured fact.
inline constexpr int kBudget = 7;

// Endpoints spent by a selection.
constexpr int endpoints_used(std::int32_t mask) {
    int total = 0;
    for (int i = 0; i < kRowCount; ++i) {
        if (mask & (1 << i)) {
            total += kCost[i];
        }
    }
    return total;
}

constexpr bool over_budget(std::int32_t mask) {
    return endpoints_used(mask) > kBudget;
}

// Move the cursor, wrapping at both ends. A delta larger than the row count is
// reduced first, so a column jump on a short list cannot escape the range.
constexpr int move_cursor(int current, int delta, int count) {
    if (count <= 0) {
        return 0;
    }
    int next = (current + delta) % count;
    if (next < 0) {
        next += count;
    }
    return next;
}

// Flip one row's bit. An index outside the list leaves the mask untouched
// rather than shifting by an out-of-range amount, which would be undefined.
constexpr std::int32_t toggle(std::int32_t mask, int index, int count) {
    if (index < 0 || index >= count || index >= 31) {
        return mask;
    }
    return mask ^ (1 << index);
}

} // namespace model::gadget
