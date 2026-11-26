/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "loadouts_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "redact.h"
#include "rpc_client.h"
#include "theme.h"
#include "ui_const.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace screen {
namespace {

std::string api_base() {
    if (const char* v = std::getenv("P4WNP1_API")) {
        return v;
    }
    return "http://127.0.0.1:8000";
}

std::string token_path() {
    if (const char* v = std::getenv("P4WNP1_TOKEN")) {
        return v;
    }
    return "/run/p4wnp1/local.token";
}

// Listed once per screen build. A render happens on every cursor move and the
// shelf does not change under the operator's thumb.
std::vector<std::string> g_loadouts;
bool g_listed_ok = false;        // did the shelf ANSWER, as opposed to being empty
std::string g_startup;           // which loadout the service applies at boot
bool g_startup_known = false;
std::string g_message;
LoadoutAction g_pending = LoadoutAction::None;

constexpr int kRowPitch = 14;

void refresh(service::RpcClient& client) {
    const service::Listing shelf = client.list_loadouts();
    g_listed_ok = shelf.ok;
    g_loadouts = shelf.items;

    const service::Listing boot = client.startup_loadout();
    g_startup_known = boot.ok;
    g_startup = boot.items.empty() ? std::string() : boot.items.front();
}

const std::string& selected_name(viewmodel::BaseViewModel& view_model) {
    static const std::string none;
    const int32_t cursor = view_model.gadget_cursor();
    if (cursor < 0 || cursor >= static_cast<int32_t>(g_loadouts.size())) {
        return none;
    }
    return g_loadouts[static_cast<std::size_t>(cursor)];
}

} // namespace

int loadout_count() {
    return static_cast<int>(g_loadouts.size());
}

void set_pending_loadout_action(LoadoutAction action) {
    g_pending = action;
}

LoadoutAction pending_loadout_action() {
    return g_pending;
}

void arm_loadout_action(viewmodel::BaseViewModel& view_model, LoadoutAction action) {
    if (g_loadouts.empty() || view_model.deploy_state() != 0) {
        return;
    }
    g_pending = action;
    view_model.set_deploy_state(1);
}

void confirm_loadout_action(viewmodel::BaseViewModel& view_model) {
    const std::string name = selected_name(view_model);
    if (name.empty()) {
        g_message = "nothing selected";
        g_pending = LoadoutAction::None;
        view_model.set_deploy_state(4);
        return;
    }

    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(6000);  // a loadout touches several subsystems

    service::ActionOutcome out;
    switch (g_pending) {
        case LoadoutAction::Deploy:
            out = client.deploy_loadout(name);
            if (out.ok) {
                g_message = "Applied " + name + ". USB and networking changed.";
            }
            break;
        case LoadoutAction::SetStartup:
            out = client.set_startup_loadout(name);
            if (out.ok) {
                g_message = name + " will be applied at boot.";
                // Re-read rather than assume: the screen should show what the
                // service says is set, not what we asked it to set.
                const service::Listing boot = client.startup_loadout();
                g_startup_known = boot.ok;
                g_startup = boot.items.empty() ? std::string() : boot.items.front();
            }
            break;
        case LoadoutAction::Delete:
            out = client.delete_loadout(name);
            if (out.ok) {
                g_message = "Deleted " + name + ".";
                refresh(client);
                // The shelf is shorter now, so a cursor left where it was
                // would point past the end or, worse, at a different loadout.
                const int count = static_cast<int>(g_loadouts.size());
                if (view_model.gadget_cursor() >= count) {
                    view_model.set_gadget_cursor(count > 0 ? count - 1 : 0);
                }
            }
            break;
        case LoadoutAction::None:
        default:
            g_message = "nothing to confirm";
            view_model.set_deploy_state(4);
            return;
    }

    g_pending = LoadoutAction::None;
    if (!out.ok) {
        g_message = out.error;
    }
    view_model.set_deploy_state(out.ok ? 3 : 4);
}

LoadoutsScreen::LoadoutsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void LoadoutsScreen::state_changed(lv_observer_t* observer, lv_subject_t*) {
    auto* self = static_cast<LoadoutsScreen*>(lv_observer_get_user_data(observer));
    if (self) {
        self->render();
    }
}

void LoadoutsScreen::build_content(lv_obj_t* content) {
    content_ = content;

    service::RpcClient client(api_base(), token_path());
    client.set_timeout_ms(1500);
    refresh(client);

    lv_subject_add_observer_obj(view_model().gadget_cursor_subject(), state_changed, content, this);
    lv_subject_add_observer_obj(view_model().deploy_state_subject(), state_changed, content, this);

    render();
}

void LoadoutsScreen::render() {
    using namespace view;

    if (!content_ || !lv_obj_is_valid(content_)) {
        return;
    }
    lv_obj_clean(content_);

    const bool dark = lv_subject_get_int(view_model().dark_mode_subject()) != 0;
    const ThemePalette p = palette(dark);
    const int32_t cursor = view_model().gadget_cursor();
    const int32_t state = view_model().deploy_state();

    auto* f_label = assets().load_standard_font(metric::kTextLabel, app::StandardFontWeight::Regular);
    auto* f_value = assets().load_standard_font(metric::kTextValue, app::StandardFontWeight::Bold);
    auto* f_note  = assets().load_standard_font(metric::kTextNote, app::StandardFontWeight::Regular);
    const auto face = [](lv_font_t* f) -> const lv_font_t* {
        return f ? f : &lv_font_montserrat_12;
    };

    // --- nothing to show, and the two reasons are not the same -------------
    if (g_loadouts.empty()) {
        const bool answered = g_listed_ok;
        auto* none = lv_label_create(content_);
        lv_label_set_text(none, answered ? "No loadouts stored" : "No service");
        lv_obj_set_style_text_font(none, face(f_value), 0);
        lv_obj_set_style_text_color(none, answered ? p.text_secondary : p.error, 0);
        lv_obj_align(none, LV_ALIGN_TOP_LEFT, metric::kGutter, 6);

        auto* why = lv_label_create(content_);
        lv_label_set_text(why,
                          answered
                              ? "The service answered and has none saved. Save one from the web UI."
                              : "The service did not answer, so what is stored is unknown.");
        lv_obj_set_style_text_font(why, face(f_note), 0);
        lv_obj_set_style_text_color(why, p.text_disabled, 0);
        lv_obj_set_width(why, kScreenWidth - (metric::kGutter * 2));
        lv_label_set_long_mode(why, LV_LABEL_LONG_WRAP);
        lv_obj_align(why, LV_ALIGN_TOP_LEFT, metric::kGutter, 25);
        return;
    }

    // --- header: how many, and which one boots -----------------------------
    char head[48];
    std::snprintf(head, sizeof(head), "%d loadouts", static_cast<int>(g_loadouts.size()));
    auto* caption = lv_label_create(content_);
    lv_label_set_text(caption, head);
    lv_obj_set_style_text_font(caption, face(f_label), 0);
    lv_obj_set_style_text_color(caption, p.text_secondary, 0);
    lv_obj_align(caption, LV_ALIGN_TOP_LEFT, metric::kGutter, 3);

    {
        // An unknown startup loadout is said as unknown. "none" would be a
        // claim about the device that we did not actually read.
        const char* boot_text = !g_startup_known ? "boot: unknown"
                              : g_startup.empty() ? "boot: none"
                                                  : nullptr;
        std::string owned;
        if (!boot_text) {
            // The startup loadout is named by the operator as well.
            owned = "boot: " + (service::redaction_enabled() ? std::string("<loadout>")
                                                             : g_startup);
            boot_text = owned.c_str();
        }
        auto* b = lv_label_create(content_);
        lv_label_set_text(b, boot_text);
        lv_obj_set_style_text_font(b, face(f_note), 0);
        lv_obj_set_style_text_color(b, g_startup_known && !g_startup.empty() ? p.brass
                                                                            : p.text_disabled, 0);
        lv_obj_align(b, LV_ALIGN_TOP_RIGHT, -metric::kGutter, 4);
    }

    auto* rule = lv_obj_create(content_);
    lv_obj_remove_style_all(rule);
    lv_obj_set_size(rule, kScreenWidth - (metric::kGutter * 2), metric::kHairline);
    lv_obj_set_style_bg_color(rule, p.border, 0);
    lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
    lv_obj_align(rule, LV_ALIGN_TOP_LEFT, metric::kGutter, 17);

    // --- the shelf, scrolled to keep the cursor in view --------------------
    const int count = static_cast<int>(g_loadouts.size());
    int first = 0;
    if (count > kLoadoutRowsVisible) {
        first = cursor - (kLoadoutRowsVisible / 2);
        if (first < 0) {
            first = 0;
        }
        if (first > count - kLoadoutRowsVisible) {
            first = count - kLoadoutRowsVisible;
        }
    }

    for (int row = 0; row < kLoadoutRowsVisible && first + row < count; ++row) {
        const int index = first + row;
        const bool here = index == cursor;
        const int y = 22 + row * kRowPitch;
        const std::string& name = g_loadouts[static_cast<std::size_t>(index)];
        const bool boots = g_startup_known && !g_startup.empty() && name == g_startup;

        if (here) {
            auto* hl = lv_obj_create(content_);
            lv_obj_remove_style_all(hl);
            lv_obj_set_size(hl, kScreenWidth - (metric::kGutter * 2) + 6, kRowPitch - 1);
            lv_obj_set_style_radius(hl, 2, 0);
            lv_obj_set_style_bg_color(hl, p.button, 0);
            lv_obj_set_style_bg_opa(hl, LV_OPA_COVER, 0);
            lv_obj_align(hl, LV_ALIGN_TOP_LEFT, metric::kGutter - 3, y - 1);
        }

        auto* label = lv_label_create(content_);
        lv_label_set_text(label, service::display_name(name, index).c_str());
        lv_obj_set_style_text_font(label, here ? face(f_value) : face(f_label), 0);
        lv_obj_set_style_text_color(label, here ? p.text : p.text_secondary, 0);
        lv_obj_set_width(label, kScreenWidth - (metric::kGutter * 2) - 46);
        lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, metric::kGutter, y);

        if (boots) {
            // A word, not a symbol: the bundled face has no reliable glyph
            // for a pin or a star, and a tofu box says nothing at all.
            auto* tag = lv_label_create(content_);
            lv_label_set_text(tag, "boot");
            lv_obj_set_style_text_font(tag, face(f_note), 0);
            lv_obj_set_style_text_color(tag, p.brass, 0);
            lv_obj_align(tag, LV_ALIGN_TOP_RIGHT, -metric::kGutter, y + 1);
        }
    }

    // --- the band ----------------------------------------------------------
    if (state == 0) {
        return;
    }

    std::string prompt;
    const char* keys = "Y yes    N cancel";
    const char* message = nullptr;
    lv_color_t tone = p.text_disabled;

    switch (state) {
        case 1: {
            const std::string& name = selected_name(view_model());
            switch (g_pending) {
                case LoadoutAction::Deploy:
                    // Named, not implied. This re-enumerates USB and can move
                    // the device off the network it is reachable on.
                    prompt = "Apply " + name + "? USB and networking change.";
                    tone = p.brass;
                    break;
                case LoadoutAction::SetStartup:
                    prompt = "Apply " + name + " at every boot?";
                    tone = p.brass;
                    break;
                case LoadoutAction::Delete:
                    prompt = "Delete " + name + "? This cannot be undone.";
                    tone = p.error;
                    keys = "Y delete    N cancel";
                    break;
                default:
                    prompt = "Confirm?";
                    break;
            }
            message = prompt.c_str();
            break;
        }
        case 2: message = "Working..."; tone = p.text_secondary; break;
        case 3: message = g_message.c_str(); tone = p.success; break;
        case 4:
        default:
            message = g_message.empty() ? "It would not do that." : g_message.c_str();
            tone = p.error;
            break;
    }

    constexpr int kBandHeight = 34;
    auto* band = lv_obj_create(content_);
    lv_obj_remove_style_all(band);
    lv_obj_set_size(band, kScreenWidth, kBandHeight);
    lv_obj_set_style_bg_color(band, p.surface, 0);
    lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(band, p.border, 0);
    lv_obj_set_style_border_width(band, metric::kHairline, 0);
    lv_obj_set_style_border_side(band, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(band, 0, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(band, LV_ALIGN_BOTTOM_MID, 0, 0);

    auto* text = lv_label_create(band);
    lv_label_set_text(text, message);
    lv_obj_set_style_text_font(text, face(f_note), 0);
    lv_obj_set_style_text_color(text, tone, 0);
    lv_obj_set_width(text, kScreenWidth - (metric::kGutter * 2));
    lv_label_set_long_mode(text, LV_LABEL_LONG_DOT);
    // An explicit single-line height is what makes LONG_DOT actually clip.
    // Without it the label is content-sized, a long message wraps to a second
    // line, and that line prints straight through the key hints below -- which
    // is how "Apply airgap? ..." came out overlapping "Y yes  N cancel".
    lv_obj_set_height(text, metric::kTextNote + 2);
    lv_obj_align(text, LV_ALIGN_TOP_LEFT, metric::kGutter, 5);

    if (state == 1) {
        auto* k = lv_label_create(band);
        lv_label_set_text(k, keys);
        lv_obj_set_style_text_font(k, face(f_note), 0);
        lv_obj_set_style_text_color(k, p.text_secondary, 0);
        lv_obj_align(k, LV_ALIGN_TOP_LEFT, metric::kGutter, 19);
    }
}

} // namespace screen
