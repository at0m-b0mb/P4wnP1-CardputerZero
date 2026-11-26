/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"

#include "application_config.h"
#include "asset_manager.h"
#include "help_popup.h"
#include "logger.h"
#include "linux_input.h"
#include "screenshot_service.h"
#include "screen_manager.h"
#include "cable_screen.h"
#include "payloads_screen.h"
#include "loadouts_screen.h"
#include "jobs_screen.h"
#include "system_screen.h"
#include "base_viewmodel.h"
#include "theme.h"
#include "toast.h"

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "p4wnp1_panel.conf"
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

struct DarkModePersistence {
    std::string config_path;
    bool last_dark_mode;
};

std::string writable_config_path() {
#if USE_DESKTOP
    return APP_CONFIG_FILE;
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "p4wnp1_panel" / "p4wnp1_panel.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "p4wnp1_panel" / "p4wnp1_panel.conf").string();
        }
    }
    return APP_CONFIG_FILE;
#endif
}

void persist_dark_mode_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* persistence = static_cast<DarkModePersistence*>(lv_observer_get_user_data(observer));
    if (!persistence) {
        return;
    }
    const bool dark_mode = lv_subject_get_int(subject) != 0;
    if (dark_mode == persistence->last_dark_mode) {
        return;
    }
    persistence->last_dark_mode = dark_mode;

    ApplicationConfig config;
    config.dark_mode = dark_mode;
    std::string error;
    if (save_application_config(persistence->config_path, config, error)) {
        LOG_INFO("saved config: {} (dark_mode={})",
                 persistence->config_path,
                 config.dark_mode ? "yes" : "no");
    } else {
        LOG_WARN("failed to save config {}: {}", persistence->config_path, error);
    }
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model, AssetManager& assets)
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
        platform::set_key_release_listener(key_release_listener, this);
    }

    ~ShortcutController() {
        platform::clear_key_release_listener(key_release_listener, this);
        platform::clear_global_key_listener(global_key_listener, this);
    }

private:
    bool handle_key(uint32_t key, bool long_pressed) {
        if (key == platform::kKeyPrintScreen) {
            if (!long_pressed) {
                const auto result = platform::screenshot::capture_active_screen();
                if (result.success) {
                    LOG_INFO("screenshot saved: {}", result.path);
                    toast_->show_highlighted("Saved to ",
                                             result.path,
                                             "",
                                             view::widgets::ToastTone::Success,
                                             5200,
                                             true);
                }
                else {
                    LOG_WARN("screenshot failed: {}", result.error);
                    toast_->show("Screenshot failed: " + result.error,
                                 view::widgets::ToastTone::Error,
                                 3200);
                }
            }
            return true;
        }

        if (key == platform::kKeyHelp) {
            if (!long_pressed) {
                toast_->hide();
                if (help_popup_->visible()) {
                    help_popup_->hide();
                }
                else {
                    help_popup_->show(view_model_.current_page());
                }
            }
            return true;
        }

        if (help_popup_->visible()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                help_popup_->hide();
            }
            return true;
        }

        // Loadouts screen: deploy a stored bundle, pin one to boot, delete
        // one. All three pause on the same band, which is why the band
        // remembers WHICH it is asking about.
        if (view_model_.current_page() == model::AppPage::Loadouts && !long_pressed) {
            const int rows = screen::loadout_count();
            switch (key) {
                case LV_KEY_UP:
                    view_model_.move_gadget_cursor(-1, rows);
                    return true;
                case LV_KEY_DOWN:
                    view_model_.move_gadget_cursor(1, rows);
                    return true;
                case LV_KEY_ENTER:
                    screen::arm_loadout_action(view_model_, screen::LoadoutAction::Deploy);
                    return true;
                case 'b':
                case 'B':
                    screen::arm_loadout_action(view_model_, screen::LoadoutAction::SetStartup);
                    return true;
                case 'x':
                case 'X':
                    screen::arm_loadout_action(view_model_, screen::LoadoutAction::Delete);
                    return true;
                case 'y':
                case 'Y':
                    if (view_model_.deploy_state() == 1) {
                        view_model_.set_deploy_state(2);
                        lv_refr_now(nullptr);
                        screen::confirm_loadout_action(view_model_);
                    }
                    return true;
                case 'n':
                case 'N':
                    if (view_model_.deploy_state() != 0) {
                        view_model_.set_deploy_state(0);
                    }
                    return true;
                default:
                    break;
            }
        }

        // Jobs screen: it polls itself, so the only key is "stop everything".
        if (view_model_.current_page() == model::AppPage::Jobs && !long_pressed) {
            switch (key) {
                case 'x':
                case 'X':
                    // No confirmation: stopping only ever reduces what the
                    // attached host is being told.
                    screen::cancel_all_jobs(view_model_);
                    return true;
                case 'n':
                case 'N':
                    if (view_model_.deploy_state() != 0) {
                        view_model_.set_deploy_state(0);
                    }
                    return true;
                default:
                    break;
            }
        }

        // System screen: identify, reboot, shut down.
        if (view_model_.current_page() == model::AppPage::System && !long_pressed) {
            switch (key) {
                case LV_KEY_UP:
                    view_model_.move_gadget_cursor(-1, screen::kSystemActionRows);
                    return true;
                case LV_KEY_DOWN:
                    view_model_.move_gadget_cursor(1, screen::kSystemActionRows);
                    return true;
                case LV_KEY_ENTER:
                    screen::activate_system_row(view_model_);
                    return true;
                case 'y':
                case 'Y':
                    if (view_model_.deploy_state() == 1) {
                        view_model_.set_deploy_state(2);
                        lv_refr_now(nullptr);
                        screen::confirm_system_action(view_model_);
                    }
                    return true;
                case 'n':
                case 'N':
                    if (view_model_.deploy_state() != 0) {
                        view_model_.set_deploy_state(0);
                    }
                    return true;
                default:
                    break;
            }
        }

        // Payloads screen: move the cursor, run under confirmation, stop all.
        if (view_model_.current_page() == model::AppPage::Payloads && !long_pressed) {
            const int rows = screen::payload_count();
            switch (key) {
                case LV_KEY_UP:
                    view_model_.move_gadget_cursor(-1, rows);
                    return true;
                case LV_KEY_DOWN:
                    view_model_.move_gadget_cursor(1, rows);
                    return true;
                case LV_KEY_ENTER:
                    if (view_model_.deploy_state() == 0 && rows > 0) {
                        view_model_.set_deploy_state(1);
                    }
                    return true;
                case 'y':
                case 'Y':
                    if (view_model_.deploy_state() == 1) {
                        view_model_.set_deploy_state(2);
                        lv_refr_now(nullptr);
                        screen::run_selected_payload(view_model_);
                    }
                    return true;
                case 'n':
                case 'N':
                    if (view_model_.deploy_state() != 0) {
                        view_model_.set_deploy_state(0);
                    }
                    return true;
                case 'x':
                case 'X':
                    // Stop needs no confirmation: it only ever reduces what
                    // the attached host is being told.
                    screen::cancel_all_payloads(view_model_);
                    return true;
                default:
                    break;
            }
        }

        // Cable screen: move the cursor and toggle a function. Selection is
        // local only -- nothing here reaches the USB controller until a deploy
        // is confirmed, which is what the footer on that screen says.
        if (view_model_.current_page() == model::AppPage::Cable && !long_pressed) {
            switch (key) {
                case LV_KEY_UP:
                    view_model_.move_gadget_cursor(-1, screen::kGadgetRows);
                    return true;
                case LV_KEY_DOWN:
                    view_model_.move_gadget_cursor(1, screen::kGadgetRows);
                    return true;
                case LV_KEY_LEFT:
                    view_model_.move_gadget_cursor(-3, screen::kGadgetRows);
                    return true;
                case LV_KEY_RIGHT:
                    view_model_.move_gadget_cursor(3, screen::kGadgetRows);
                    return true;
                case ' ':
                    view_model_.toggle_selected_gadget(screen::kGadgetRows);
                    return true;
                case LV_KEY_ENTER:
                    // Refuse an over-budget composition here, with a reason,
                    // rather than letting the service refuse it afterwards
                    // with an arithmetic message and no indication of what to
                    // turn off. The footer is already saying which it is.
                    if (view_model_.deploy_state() == 0 &&
                        !screen::selection_over_budget(view_model_)) {
                        view_model_.set_deploy_state(1);  // ask first
                    }
                    return true;
                case 'y':
                case 'Y':
                    if (view_model_.deploy_state() == 1) {
                        view_model_.set_deploy_state(2);
                        lv_refr_now(nullptr);  // show "Deploying..." before we block
                        screen::deploy_selected(view_model_);
                    }
                    return true;
                case 'n':
                case 'N':
                    if (view_model_.deploy_state() != 0) {
                        view_model_.set_deploy_state(0);
                    }
                    return true;
                default:
                    break;
            }
        }

        // On the Cable screen, Esc first dismisses a confirm or a result; only
        // once the screen is idle does it mean "go back".
        if (view_model_.current_page() != model::AppPage::Status &&
            key == LV_KEY_ESC && !long_pressed && view_model_.deploy_state() != 0) {
            view_model_.set_deploy_state(0);
            return true;
        }

        const bool is_escape = key == LV_KEY_ESC;
        const bool is_home_exit_key =
            key == '4' &&
            ((!long_pressed && view_model_.current_page() == model::AppPage::Status) ||
             (long_pressed && exit_key_pressed_ == key));
        if (!is_escape && !is_home_exit_key) {
            return false;
        }

        if (long_pressed) {
            if (exit_key_pressed_ == key && exit_armed_) {
                toast_->hide();
                view_model_.request_quit();
            }
        }
        else {
            exit_key_pressed_ = key;
            exit_armed_ = view_model_.current_page() == model::AppPage::Status;
            if (exit_armed_) {
                toast_->show_persistent_highlighted("Hold ",
                                                    "ESC",
                                                    " to exit",
                                                    view::widgets::ToastTone::Warning);
            }
        }
        return true;
    }

    void handle_release(uint32_t key) {
        if (key != exit_key_pressed_) {
            return;
        }

        if (exit_armed_) {
            toast_->hide();
        } else if (key == LV_KEY_ESC &&
                   view_model_.current_page() == model::AppPage::Cable) {
            view_model_.show_status_page();
        }
        exit_key_pressed_ = 0;
        exit_armed_ = false;
    }

public:
    // Feed a key through exactly the path a real keypress takes. Used by
    // P4WNP1_KEYS so the screenshot tooling exercises the key handler rather
    // than setting the state it would have produced -- a capture that bypasses
    // the handler proves the drawing and nothing about the wiring.
    bool inject_key(uint32_t key) {
        return handle_key(key, false);
    }

private:
    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    static void key_release_listener(uint32_t key, const char*, void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        if (controller) {
            controller->handle_release(key);
        }
    }

    viewmodel::BaseViewModel& view_model_;
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
    uint32_t exit_key_pressed_{0};
    bool exit_armed_{false};
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("p4wnp1");

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    const std::string user_config_path = writable_config_path();
    std::string loaded_config_path = APP_CONFIG_FILE;
    if (user_config_path != APP_CONFIG_FILE) {
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        }
    }

    ApplicationConfig config;
    std::string config_error;
    if (load_application_config(loaded_config_path, config, config_error)) {
        view_model.set_dark_mode(config.dark_mode);
        LOG_INFO("loaded config: {} (dark_mode={})",
                 loaded_config_path,
                 config.dark_mode ? "yes" : "no");

        // P4WNP1_PAGE=status|cable|payloads|loadouts|jobs|system: open
        // straight to a page. Used with P4WNP1_SHOT to capture each screen
        // without driving the keypad.
        if (const char* page = std::getenv("P4WNP1_PAGE")) {
            const std::string want(page);
            if (want == "cable") {
                view_model.show_cable_page();
            }
            else if (want == "payloads") {
                view_model.show_payloads_page();
            }
            else if (want == "loadouts") {
                view_model.show_loadouts_page();
            }
            else if (want == "jobs") {
                view_model.show_jobs_page();
            }
            else if (want == "system") {
                view_model.show_system_page();
            }
            else {
                view_model.show_status_page();
            }
            LOG_INFO("opening page: {}", page);
        }

        // P4WNP1_ACTION picks WHICH question an armed confirm band is asking,
        // since several actions share one band. Screenshot tooling only.
        if (const char* action = std::getenv("P4WNP1_ACTION")) {
            const std::string want(action);
            if (want == "deploy") {
                screen::set_pending_loadout_action(screen::LoadoutAction::Deploy);
            }
            else if (want == "startup") {
                screen::set_pending_loadout_action(screen::LoadoutAction::SetStartup);
            }
            else if (want == "delete") {
                screen::set_pending_loadout_action(screen::LoadoutAction::Delete);
            }
            else if (want == "reboot") {
                screen::set_pending_system_action(screen::SystemAction::Reboot);
            }
            else if (want == "shutdown") {
                screen::set_pending_system_action(screen::SystemAction::Shutdown);
            }
        }

        // P4WNP1_DEPLOY_STATE: open on a given Cable state. Screenshot
        // tooling, alongside P4WNP1_PAGE -- every state the operator can see
        // should be capturable without reproducing it by hand.
        if (const char* state = std::getenv("P4WNP1_DEPLOY_STATE")) {
            view_model.set_deploy_state(std::atoi(state));
        }
    } else {
        LOG_WARN("failed to load config {}: {}; using defaults", loaded_config_path, config_error);
    }

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           view_model.is_dark_mode(),
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    screen_manager.start();
    ShortcutController shortcuts(view_model, assets);

    DarkModePersistence dark_mode_persistence{user_config_path, view_model.is_dark_mode()};
    auto* dark_mode_observer = lv_subject_add_observer(view_model.dark_mode_subject(),
                                                        persist_dark_mode_observer,
                                                        &dark_mode_persistence);

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));

    // P4WNP1_KEYS=down,down,enter,y : replay keystrokes through the real
    // handler once the first screen has settled, then carry on to the shot.
    // This is the only way the key wiring gets exercised off-device.
    std::vector<uint32_t> replay_keys;
    if (const char* keys = std::getenv("P4WNP1_KEYS")) {
        const std::string spec(keys);
        std::size_t at = 0;
        while (at <= spec.size()) {
            std::size_t comma = spec.find(',', at);
            if (comma == std::string::npos) {
                comma = spec.size();
            }
            std::string name = spec.substr(at, comma - at);
            // trim
            while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) {
                name.erase(name.begin());
            }
            while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) {
                name.pop_back();
            }
            if (!name.empty()) {
                uint32_t code = 0;
                if (name == "up")          code = LV_KEY_UP;
                else if (name == "down")   code = LV_KEY_DOWN;
                else if (name == "left")   code = LV_KEY_LEFT;
                else if (name == "right")  code = LV_KEY_RIGHT;
                else if (name == "enter")  code = LV_KEY_ENTER;
                else if (name == "esc")    code = LV_KEY_ESC;
                else if (name == "space")  code = ' ';
                else if (name.size() == 1) code = static_cast<uint32_t>(name[0]);
                if (code != 0) {
                    replay_keys.push_back(code);
                }
                else {
                    LOG_WARN("P4WNP1_KEYS: ignoring unknown key '{}'", name);
                }
            }
            if (comma == spec.size()) {
                break;
            }
            at = comma + 1;
        }
        LOG_INFO("key replay: {} keys queued", replay_keys.size());
    }
    std::size_t replayed = 0;
    int replay_at_ms = 450;  // after the first screen has fetched and drawn

    // P4WNP1_SHOT=<ms>: let the UI settle, capture one frame, exit. This is how
    // the store screenshots in app-builder.json get made, and how a screen can
    // be reviewed by someone who does not have the device in front of them.
    int shot_after_ms = 0;
    if (const char* shot = std::getenv("P4WNP1_SHOT")) {
        shot_after_ms = std::atoi(shot);
        if (shot_after_ms <= 0) {
            shot_after_ms = 700;
        }
        LOG_INFO("screenshot mode: capturing after {} ms", shot_after_ms);
    }
    int elapsed_ms = 0;

    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        lv_timer_handler();
        lv_delay_ms(5);

        elapsed_ms += 5;

        if (replayed < replay_keys.size() && elapsed_ms >= replay_at_ms) {
            const uint32_t key = replay_keys[replayed++];
            shortcuts.inject_key(key);
            lv_refr_now(nullptr);
            // Space the keys out so each one's render, and any RPC it makes,
            // completes before the next arrives.
            replay_at_ms = elapsed_ms + 160;
        }

        if (shot_after_ms > 0) {
            if (elapsed_ms >= shot_after_ms && replayed >= replay_keys.size()) {
                const auto result = platform::screenshot::capture_active_screen();
                if (result.success) {
                    LOG_INFO("screenshot saved: {}", result.path);
                }
                else {
                    LOG_WARN("screenshot failed: {}", result.error);
                }
                break;
            }
        }
    }

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }
    if (dark_mode_observer) {
        lv_observer_remove(dark_mode_observer);
    }

    return 0;
}

} // namespace app
