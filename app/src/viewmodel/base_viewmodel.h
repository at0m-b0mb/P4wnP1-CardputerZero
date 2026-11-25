/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "subjects.h"
#include "base_model.h"

#include "lvgl.h"

namespace viewmodel {

class BaseViewModel {
public:
    BaseViewModel();
    ~BaseViewModel();

    BaseViewModel(const BaseViewModel&) = delete;
    BaseViewModel& operator=(const BaseViewModel&) = delete;

    lv_subject_t* title_subject();
    lv_subject_t* greeting_subject();
    lv_subject_t* dark_mode_subject();
    lv_subject_t* current_page_subject();
    // Which gadget functions are selected (a bitmask over the Cable screen's
    // function table) and which row the keypad is on. Selection is local: it
    // is what WOULD be deployed, not what the device is presenting.
    lv_subject_t* gadget_mask_subject();
    lv_subject_t* gadget_cursor_subject();

    // Deploying is the first thing the panel does that changes something
    // outside the device, so it is a state the screen can see rather than a
    // function call that happens silently.
    //   0 idle  1 confirming  2 deploying  3 deployed  4 refused
    lv_subject_t* deploy_state_subject();
    int32_t deploy_state() const;
    void set_deploy_state(int32_t state);
    lv_subject_t* bold_text_subject();
    lv_subject_t* info_visible_subject();
    lv_subject_t* quit_requested_subject();

    bool is_dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    model::AppPage current_page() const;
    void show_status_page();
    void show_cable_page();
    void show_payloads_page();
    void toggle_page();
    void toggle_selected_gadget(int bit_count);
    void move_gadget_cursor(int delta, int row_count);
    int32_t gadget_mask() const;
    int32_t gadget_cursor() const;
    void toggle_bold_text();
    void toggle_info();
    void request_quit();

private:
    void publish_all();

    model::BaseModel model_;
    reactive::StringSubject<32> title_subject_;
    reactive::StringSubject<32> greeting_subject_;
    reactive::BoolSubject dark_mode_subject_;
    reactive::IntSubject current_page_subject_;
    reactive::IntSubject gadget_mask_subject_;
    reactive::IntSubject gadget_cursor_subject_;
    reactive::IntSubject deploy_state_subject_;
    reactive::BoolSubject bold_text_subject_;
    reactive::BoolSubject info_visible_subject_;
    reactive::BoolSubject quit_requested_subject_;
};

} // namespace viewmodel
