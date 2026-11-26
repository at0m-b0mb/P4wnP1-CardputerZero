/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {

const char* BaseModel::app_title() const {
    return "P4wnP1";
}

const char* BaseModel::greeting() const {
    return "Hello World";
}

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void BaseModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

AppPage BaseModel::current_page() const {
    return current_page_;
}


void BaseModel::set_current_page(AppPage page) {
    current_page_ = page;
}

void BaseModel::toggle_page() {
    // Six pages, walked in a ring. Written as arithmetic over the enum rather
    // than a switch so that adding a page cannot leave one unreachable --
    // which a forgotten switch arm does silently.
    const int next = (static_cast<int>(current_page_) + 1) % kAppPageCount;
    current_page_ = static_cast<AppPage>(next);
}

} // namespace model
