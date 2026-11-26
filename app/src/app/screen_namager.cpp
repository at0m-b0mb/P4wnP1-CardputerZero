/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "screen_manager.h"

#include "status_screen.h"
#include "cable_screen.h"
#include "payloads_screen.h"
#include "loadouts_screen.h"
#include "jobs_screen.h"
#include "system_screen.h"

namespace app {
namespace {

void flush_requested_page_async(void* user_data) {
    auto* manager = static_cast<ScreenManager*>(user_data);
    if (manager) {
        manager->flush_requested_page();
    }
}

void current_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
    if (!manager) {
        return;
    }

    manager->request_page(static_cast<model::AppPage>(lv_subject_get_int(subject)));
}

} // namespace

ScreenManager::ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

ScreenManager::~ScreenManager() {
    lv_async_call_cancel(flush_requested_page_async, this);

    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
}

void ScreenManager::start() {
    if (page_observer_) {
        return;
    }

    page_observer_ = lv_subject_add_observer_with_target(view_model_.current_page_subject(),
                                                         current_page_observer,
                                                         this,
                                                         nullptr);
}

void ScreenManager::show_status_screen() {
    load_screen(std::make_unique<screen::StatusScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Status;
    has_loaded_page_ = true;
}

void ScreenManager::show_cable_screen() {
    load_screen(std::make_unique<screen::CableScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Cable;
    has_loaded_page_ = true;
}

void ScreenManager::show_payloads_screen() {
    load_screen(std::make_unique<screen::PayloadsScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Payloads;
    has_loaded_page_ = true;
}

void ScreenManager::show_loadouts_screen() {
    load_screen(std::make_unique<screen::LoadoutsScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Loadouts;
    has_loaded_page_ = true;
}

void ScreenManager::show_jobs_screen() {
    load_screen(std::make_unique<screen::JobsScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Jobs;
    has_loaded_page_ = true;
}

void ScreenManager::show_system_screen() {
    load_screen(std::make_unique<screen::SystemScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::System;
    has_loaded_page_ = true;
}

void ScreenManager::request_page(model::AppPage page) {
    requested_page_ = page;

    if (page_switch_scheduled_) {
        return;
    }

    page_switch_scheduled_ = lv_async_call(flush_requested_page_async, this) == LV_RESULT_OK;
}

void ScreenManager::flush_requested_page() {
    page_switch_scheduled_ = false;

    if (has_loaded_page_ && requested_page_ == loaded_page_) {
        return;
    }

    switch (requested_page_) {
        case model::AppPage::Cable:    show_cable_screen();    return;
        case model::AppPage::Payloads: show_payloads_screen(); return;
        case model::AppPage::Loadouts: show_loadouts_screen(); return;
        case model::AppPage::Jobs:     show_jobs_screen();     return;
        case model::AppPage::System:   show_system_screen();   return;
        case model::AppPage::Status:
        default:                       show_status_screen();   return;
    }
}

lv_obj_t* ScreenManager::current_screen() const {
    return current_screen_ ? current_screen_->root() : nullptr;
}

void ScreenManager::load_screen(std::unique_ptr<screen::BaseScreen> next_screen) {
    if (!next_screen || !next_screen->root()) {
        return;
    }

    lv_screen_load(next_screen->root());
    current_screen_ = std::move(next_screen);
}

} // namespace app
