/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_viewmodel.h"

namespace viewmodel {
namespace {

int page_to_int(model::AppPage page) {
    return static_cast<int>(page);
}

} // namespace

BaseViewModel::BaseViewModel()
    : title_subject_(model_.app_title()),
      greeting_subject_(model_.greeting()),
      dark_mode_subject_(model_.dark_mode()),
      current_page_subject_(page_to_int(model_.current_page())),
      gadget_mask_subject_(0b000011),
      gadget_cursor_subject_(0),
      bold_text_subject_(false),
      info_visible_subject_(false),
      quit_requested_subject_(false) {}

BaseViewModel::~BaseViewModel() = default;

lv_subject_t* BaseViewModel::title_subject() {
    return title_subject_.native();
}

lv_subject_t* BaseViewModel::greeting_subject() {
    return greeting_subject_.native();
}

lv_subject_t* BaseViewModel::dark_mode_subject() {
    return dark_mode_subject_.native();
}

lv_subject_t* BaseViewModel::current_page_subject() {
    return current_page_subject_.native();
}

lv_subject_t* BaseViewModel::gadget_mask_subject() {
    return gadget_mask_subject_.native();
}

lv_subject_t* BaseViewModel::gadget_cursor_subject() {
    return gadget_cursor_subject_.native();
}

lv_subject_t* BaseViewModel::bold_text_subject() {
    return bold_text_subject_.native();
}

lv_subject_t* BaseViewModel::info_visible_subject() {
    return info_visible_subject_.native();
}

lv_subject_t* BaseViewModel::quit_requested_subject() {
    return quit_requested_subject_.native();
}

bool BaseViewModel::is_dark_mode() const {
    return model_.dark_mode();
}

void BaseViewModel::set_dark_mode(bool enabled) {
    model_.set_dark_mode(enabled);
    publish_all();
}

void BaseViewModel::toggle_dark_mode() {
    model_.toggle_dark_mode();
    publish_all();
}

model::AppPage BaseViewModel::current_page() const {
    return model_.current_page();
}

void BaseViewModel::show_status_page() {
    model_.set_current_page(model::AppPage::Status);
    publish_all();
}

void BaseViewModel::show_cable_page() {
    model_.set_current_page(model::AppPage::Cable);
    publish_all();
}

void BaseViewModel::toggle_page() {
    model_.toggle_page();
    publish_all();
}

void BaseViewModel::toggle_selected_gadget(int bit_count) {
    const int32_t cursor = gadget_cursor_subject_.value();
    if (cursor < 0 || cursor >= bit_count) {
        return;
    }
    gadget_mask_subject_.set(gadget_mask_subject_.value() ^ (1 << cursor));
}

int32_t BaseViewModel::gadget_mask() const {
    return gadget_mask_subject_.value();
}

int32_t BaseViewModel::gadget_cursor() const {
    return gadget_cursor_subject_.value();
}

void BaseViewModel::move_gadget_cursor(int delta, int row_count) {
    if (row_count <= 0) {
        return;
    }
    int32_t next = (gadget_cursor_subject_.value() + delta) % row_count;
    if (next < 0) {
        next += row_count;
    }
    gadget_cursor_subject_.set(next);
}

void BaseViewModel::toggle_bold_text() {
    bold_text_subject_.toggle();
}

void BaseViewModel::toggle_info() {
    info_visible_subject_.toggle();
}

void BaseViewModel::request_quit() {
    quit_requested_subject_.set(true);
}

void BaseViewModel::publish_all() {
    title_subject_.set(model_.app_title());
    greeting_subject_.set(model_.greeting());
    dark_mode_subject_.set(model_.dark_mode());
    current_page_subject_.set(page_to_int(model_.current_page()));
}

} // namespace viewmodel
