/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Palette and type scale. Colours derived from the house style: warm paper with
 * gold in light, true black in dark.
 */



#pragma once

#include <cstdint>

namespace view {

// some font constants
constexpr const char* ICON_SIGN_OUT           = "\uE42A";
constexpr const char* ICON_TEXT_BOLD          = "\uE5BE";
constexpr const char* ICON_MOON               = "\uE330";
constexpr const char* ICON_SUN                = "\uE474";
constexpr const char* ICON_SQUARE_ARROW_LEFT  = "\uE074";
constexpr const char* ICON_SQUARE_ARROW_RIGHT = "\uE076";
constexpr const char* ICON_MINUS              = "\uE32A";
constexpr const char* ICON_PLUS               = "\uE3D4";
constexpr const char* ICON_INFO               = "\uE2CE";
constexpr const char* ICON_WIFI_NONE          = "\uE4F0";
constexpr const char* ICON_WIFI_LOW           = "\uE4EC";
constexpr const char* ICON_WIFI_MEDIUM        = "\uE4EE";
constexpr const char* ICON_WIFI_HIGH          = "\uE4EA";
constexpr const char* ICON_ETHERNET           = "\uEDDE";
constexpr const char* ICON_BAT_FULL           = "\uE7C4";
constexpr const char* ICON_BAT_HIGH           = "\uE7C2";
constexpr const char* ICON_BAT_MEDIUM         = "\uE7C0";
constexpr const char* ICON_BAT_LOW            = "\uE7BE";
constexpr const char* ICON_BAT_EMPTY          = "\uE7C6";
constexpr const char* ICON_BAT_CHARGING       = "\uE0BC";

namespace color {

// Every colour is declared as a light/dark pair at the point of definition, so a
// dark value cannot be forgotten when a light one is added.
struct Tone {
    uint32_t light;
    uint32_t dark;
};

// --- ground -------------------------------------------------------------
// Light is warm paper, not flat white: #FFFFFF is reserved for cards so they
// lift off the page. Dark is true black, and every grey above it is neutral --
// nothing here may read as blue.
constexpr Tone kBody   {0xF3F1EC, 0x000000};
constexpr Tone kCard   {0xFFFFFF, 0x0E0E0E};
constexpr Tone kBar    {0xEDEAE3, 0x121212};
constexpr Tone kButton {0xE8E4DB, 0x1A1A1A};
constexpr Tone kBorder {0xD9D3C7, 0x2A2A2A};

// --- text ---------------------------------------------------------------
// Secondary text on true black is #C2BBB1 rather than a mid grey: a mid grey
// clears AA on paper and still reads washed out on black.
constexpr Tone kTextPrimary  {0x1A1713, 0xEDE8E0};
constexpr Tone kTextSecondary{0x5C564B, 0xC2BBB1};
constexpr Tone kTextDisabled {0x8C8578, 0x6E6A64};

// --- the one accent -----------------------------------------------------
// Two golds, because one cannot do every job: brass stays legible as small
// text, shine is bright enough to carry a mark and is never set behind words.
constexpr Tone kBrass {0x80631D, 0xD7B45A};
// On warm paper the two golds necessarily converge: a gold bright enough to
// read as "shine" cannot clear 3:1 against #F3F1EC. So shine earns its name
// only on black, and on paper it is a deeper mark. That is a property of the
// ground, not a compromise in the token.
constexpr Tone kShine {0x9D7E1E, 0xF0CE6A};

// --- state --------------------------------------------------------------
// Muted and slightly earthy, so they sit beside brass instead of fighting it.
constexpr Tone kSuccess {0x2E6F4E, 0x6FBF8F};
constexpr Tone kWarning {0x8C5D16, 0xE0B457};
constexpr Tone kError   {0x9B2C2C, 0xE08585};

constexpr uint32_t pick(const Tone& tone, bool dark) { return dark ? tone.dark : tone.light; }

} // namespace color

namespace metric {

// 4pt rhythm. Nothing in a layout should be a number that is not from here.
constexpr int kStep       = 4;
constexpr int kGutter     = 10;
constexpr int kRowHeight  = 16;
constexpr int kHairline   = 1;

// Type scale. The house style's display serif does not survive 12px on a
// 320x170 panel -- the same reason a Didone's hairlines break up on black --
// so identity is carried by weight and the brass accent instead, and the mix
// that does survive is sans for prose, mono for measured values.
constexpr int kTextLabel  = 12;
constexpr int kTextValue  = 12;
constexpr int kTextNote   = 11;
constexpr int kTextTitle  = 14;

} // namespace metric

} // namespace view
