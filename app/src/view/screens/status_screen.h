/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "base_screen.h"
#include "status_parse.h"

namespace screen {

class StatusScreen : public BaseScreen {
public:
    StatusScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
