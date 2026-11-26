/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

enum class AppPage {
    Status   = 0,
    Cable    = 1,
    Payloads = 2,
    Loadouts = 3,
    Jobs     = 4,
    System   = 5,
};

// Pages in the order the page key walks them, and how many there are. Kept
// here so the navbar, the help popup and the screen manager cannot disagree
// about what exists.
constexpr int kAppPageCount = 6;

class BaseModel {
public:
    const char* app_title() const;
    const char* greeting() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();

private:
    bool dark_mode_ = false;
    AppPage current_page_ = AppPage::Status;
};

} // namespace model
