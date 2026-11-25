/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * WCAG contrast for every text pairing in both themes. Checked rather than
 * eyeballed, because a pairing can clear the ratio and still look wrong -- and
 * because the eye is worst at exactly the comparison that matters, small text
 * on a ground it has been staring at.
 */

#include "../src/view/ui_const.h"

#include <cstdio>
#include <cmath>
#include <string>
#include <vector>

namespace {

double channel(double srgb) {
    const double c = srgb / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

double luminance(uint32_t rgb) {
    const double r = channel(static_cast<double>((rgb >> 16) & 0xFF));
    const double g = channel(static_cast<double>((rgb >> 8) & 0xFF));
    const double b = channel(static_cast<double>(rgb & 0xFF));
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
}

double contrast(uint32_t a, uint32_t b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    const double hi = la > lb ? la : lb;
    const double lo = la > lb ? lb : la;
    return (hi + 0.05) / (lo + 0.05);
}

struct Check {
    const char* what;
    view::color::Tone fg;
    view::color::Tone bg;
    double floor;   // 4.5 normal text, 3.0 large/bold text and UI marks
};

int failures = 0;

void run(bool dark, const std::vector<Check>& checks) {
    std::printf("\n  %s\n", dark ? "DARK (true black)" : "LIGHT (warm paper)");
    for (const auto& c : checks) {
        const uint32_t fg = view::color::pick(c.fg, dark);
        const uint32_t bg = view::color::pick(c.bg, dark);
        const double ratio = contrast(fg, bg);
        const bool ok = ratio >= c.floor;
        if (!ok) ++failures;
        std::printf("    %-34s #%06X on #%06X  %5.2f:1  (>= %.1f)  %s\n",
                    c.what, fg, bg, ratio, c.floor, ok ? "pass" : "FAIL");
    }
}

} // namespace

int main() {
    using namespace view::color;

    const std::vector<Check> checks = {
        // Body text has to clear AA everywhere it is set.
        {"primary text on paper/black",   kTextPrimary,   kBody,   4.5},
        {"primary text on card",          kTextPrimary,   kCard,   4.5},
        {"primary text on bar",           kTextPrimary,   kBar,    4.5},
        {"primary text on button",        kTextPrimary,   kButton, 4.5},

        // Secondary carries real content (labels, units), so it is held to the
        // same floor rather than treated as decoration.
        {"secondary text on paper/black", kTextSecondary, kBody,   4.5},
        {"secondary text on card",        kTextSecondary, kCard,   4.5},
        {"secondary text on bar",         kTextSecondary, kBar,    4.5},

        // Brass exists to be legible as small text; that is the whole reason
        // there are two golds.
        {"brass on paper/black",          kBrass,         kBody,   4.5},
        {"brass on card",                 kBrass,         kCard,   4.5},

        // State colours are set as words, not just swatches.
        {"success on paper/black",        kSuccess,       kBody,   4.5},
        {"success on card",               kSuccess,       kCard,   4.5},
        {"warning on paper/black",        kWarning,       kBody,   4.5},
        {"warning on card",               kWarning,       kCard,   4.5},
        {"error on paper/black",          kError,         kBody,   4.5},
        {"error on card",                 kError,         kCard,   4.5},

        // Shine never sits behind words; it only has to be visible as a mark.
        {"shine as a mark on paper/black", kShine,        kBody,   3.0},
        {"shine as a mark on card",        kShine,        kCard,   3.0},
    };

    std::printf("Contrast, WCAG 2.1 relative luminance\n");
    run(false, checks);
    run(true, checks);

    // Disabled text is exempt from AA, but report it so a drift into
    // invisibility is at least visible here.
    std::printf("\n  Reported, not enforced (disabled text is exempt):\n");
    for (bool dark : {false, true}) {
        const double r = contrast(pick(kTextDisabled, dark), pick(kBody, dark));
        std::printf("    disabled text on %-16s %5.2f:1\n", dark ? "black" : "paper", r);
    }

    std::printf("\n%s\n", failures == 0 ? "ALL CONTRAST CHECKS PASS"
                                        : "CONTRAST FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
