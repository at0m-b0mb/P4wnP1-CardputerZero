/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Making a screenshot safe to publish.
 *
 * The Status screen shows what the device is actually on: wlan0's address and
 * the Wi-Fi SSID. In the simulator those are a fake service's inventions. On
 * real hardware they are the operator's own network -- and the CardputerZero
 * store REQUIRES screenshots, so the one moment this codebase most wants a
 * picture of the panel is the one moment the panel is displaying something
 * nobody should publish.
 *
 * So redaction is a display-time transform, switched on by P4WNP1_REDACT=1.
 * It never touches the snapshot: the panel still knows the real values, still
 * acts on them, and still logs them locally. Only the drawing changes.
 *
 * Shape is preserved rather than blanked. "172.16.x.x" still tells the reader
 * it is an RFC1918 address on the usb subnet, which is the part that makes a
 * screenshot useful as documentation; the host part is what identifies a
 * network. Blanking the whole field would make the screenshots useless and
 * tempt someone into publishing the unredacted ones instead.
 */

#pragma once

#include <string>

namespace service {

// Is redaction on? Read from P4WNP1_REDACT once, so a screenshot run cannot
// half-apply it.
bool redaction_enabled();

// Overridable for tests, and for a capture script that wants it on without
// touching the environment.
void set_redaction(bool on);

// "172.16.0.1" -> "172.16.x.x". Leaves anything that is not a dotted quad
// alone, including the empty string and words like "none", because those are
// the screen's own vocabulary rather than data about the device.
std::string redact_ip(const std::string& value);

// "AP HomeNetwork" -> "AP <ssid>". The mode is kept because it says what the
// radio is doing; the name is the identifying part.
//
// Known non-identifying states ("idle", "unavailable", "") pass through: they
// describe the subsystem, not a network.
std::string redact_wifi(const std::string& value);

// Anything an operator may have named themselves -- a loadout, a payload, a
// job label. Replaced wholesale with a stable placeholder plus its index, so
// a list of five still reads as a list of five.
std::string redact_name(const std::string& value, int index);

// Applies redact_ip only when redaction is on. The screens call these, so a
// screen cannot forget the check.
std::string display_ip(const std::string& value);
std::string display_wifi(const std::string& value);
std::string display_name(const std::string& value, int index);

} // namespace service
