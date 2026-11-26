/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"
#include "network_custody_runtime.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

// `p4wnp1_panel --netcheck` reports what the networking custody would do and
// changes nothing.
//
// This exists for the first run on a real CardputerZero. Custody stops a
// system service, and the honest way to meet unfamiliar hardware with a
// feature like that is to have it say what it intends before it is allowed
// to do it. It also answers, in one command and without a screen, the two
// questions that decide whether the feature works on the device at all: is
// systemd reachable, and are we allowed to drive it.
int netcheck(bool armed) {
    using namespace service::netcustody;

    Effects fx = system_effects();
    fx.dry_run = true;

    const Reading reading = observe(fx, armed);
    std::printf("P4wnP1 panel -- networking custody dry run\n\n");
    std::printf("  marker path          %s\n", fx.marker_path.c_str());
    std::printf("  borrow recorded      %s\n", reading.observed.marker_present ? "yes" : "no");
    if (reading.nm_known) {
        std::printf("  NetworkManager       %s\n",
                    reading.observed.nm_running ? "running" : "not running");
    }
    else {
        std::printf("  NetworkManager       UNKNOWN (%s)\n", reading.why.c_str());
    }
    std::printf("  treating as armed    %s\n\n", armed ? "yes" : "no");

    const Outcome out = reconcile_now(fx, armed);
    if (!out.ok) {
        std::printf("  would refuse: %s\n", out.error.c_str());
        return 1;
    }
    if (out.did.empty()) {
        std::printf("  nothing to do.\n");
        return 0;
    }
    for (const auto& step : out.did) {
        std::printf("  %s\n", step.c_str());
    }
    std::printf("\n  nothing was changed. Drop --netcheck to act.\n");
    return 0;
}

// `--restore-networking` is the recovery hatch, and what the package's postrm
// calls. It gives networking back if we owe it, and does nothing otherwise.
int restore_networking() {
    using namespace service::netcustody;
    const Outcome out = reconcile_now(system_effects(), /*want_armed=*/false);
    if (!out.ok) {
        std::fprintf(stderr, "could not restore networking: %s\n", out.error.c_str());
        return 1;
    }
    for (const auto& step : out.did) {
        std::printf("%s\n", step.c_str());
    }
    if (!out.changed) {
        std::printf("nothing was owed; NetworkManager left as it was\n");
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--netcheck") {
            return netcheck(/*armed=*/false);
        }
        if (arg == "--netcheck-armed") {
            return netcheck(/*armed=*/true);
        }
        if (arg == "--restore-networking") {
            return restore_networking();
        }
        if (arg == "--version") {
#ifdef P4WNP1_PANEL_VERSION
            std::printf("p4wnp1_panel %s\n", P4WNP1_PANEL_VERSION);
#else
            std::printf("p4wnp1_panel (version unknown)\n");
#endif
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            std::printf(
                "p4wnp1_panel -- on-device panel for P4wnP1 A.L.O.A.\n\n"
                "  (no arguments)          run the panel\n"
                "  --netcheck              say what networking custody would do; change nothing\n"
                "  --netcheck-armed        the same, as if a network gadget were deployed\n"
                "  --restore-networking    hand NetworkManager back if we owe it\n"
                "  --version               print the version\n");
            return 0;
        }
        std::fprintf(stderr, "unknown option: %s (try --help)\n", argv[i]);
        return 2;
    }

    app::Application application;
    return application.run();
}
