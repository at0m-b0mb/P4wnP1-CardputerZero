#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 at0m-b0mb
# SPDX-License-Identifier: GPL-3.0-only
"""
Check app-builder.json against the CardputerZero store's published policy
before czdev uploads anything.

The rules are from AppBuilder/docs/APP_BUILDER_JSON.md. czdev pre-checks some
of this and the store enforces the rest server-side; running it here means a
rejection costs a second rather than a round trip.

Note the icon rule in particular: the Template scaffolds a 100x100 icon and
the store requires 128-512, so a freshly scaffolded app fails this until the
icon is replaced.
"""
import json
import pathlib
import struct
import sys
from pathlib import Path

APP = Path(__file__).resolve().parent / "app"

CATEGORIES = {
    "System Tools", "Development", "Hardware & IoT", "Security", "Radio & Comms",
    "AI", "Creative & Office", "Media", "Games", "Emulators", "Education",
    "Lifestyle", "Other",
}
PERMISSIONS = {
    "camera", "microphone", "imu", "network", "additional_hardware",
    "background_service", "external_display",
}
PLACEHOLDERS = {"TODO", "TEMP", "M5Stack", "Your Name", "TODO-your-github-login"}

failures = []


def check(ok, what):
    print(f"  {'pass' if ok else 'FAIL'}  {what}")
    if not ok:
        failures.append(what)


def png_size(path):
    with open(path, "rb") as f:
        return struct.unpack(">II", f.read(32)[16:24])


def main():
    manifest = json.loads((APP / "app-builder.json").read_text())
    store = manifest.get("store", {})
    print("app-builder.json against the store policy\n")

    title = manifest.get("app_name", "")
    check(bool(title) and title not in PLACEHOLDERS and "Template" not in title,
          f"store title: {title!r}")

    summary = store.get("summary", "")
    check(0 < len(summary) <= 80, f"summary is {len(summary)} chars (limit 80)")

    cats = store.get("categories", [])
    check(1 <= len(cats) <= 2 and set(cats) <= CATEGORIES, f"categories {cats}")

    shots = store.get("screenshots", [])
    check(1 <= len(shots) <= 6, f"{len(shots)} screenshots (1-6)")
    for shot in shots:
        p = APP / shot
        if not p.exists():
            check(False, f"{shot} is missing")
            continue
        check(png_size(p) == (320, 170), f"{shot} is {png_size(p)[0]}x{png_size(p)[1]}, needs 320x170")

    icon = store.get("icon", "")
    p = APP / icon
    if not p.exists():
        check(False, f"icon {icon} is missing")
    else:
        w, h = png_size(p)
        check(w == h and 128 <= w <= 512, f"icon is {w}x{h}, needs a 128-512 square")

    check(bool(store.get("license")) and not store["license"].startswith("TODO"),
          f"licence {store.get('license')!r}")
    check("CardputerZero/Template" not in store.get("source_repo", ""),
          "source_repo does not point at the Template")

    author = store.get("author", {})
    bad_author = PLACEHOLDERS | {"", None}
    check(author.get("github") not in bad_author and
          author.get("display_name") not in bad_author,
          f"author {author.get('display_name')!r}")

    code = store.get("share_code", "")
    check(len(code) == 4 and code.isalnum() and code not in PLACEHOLDERS,
          f"share_code {code!r}")

    check(set(store.get("permissions", {})) == PERMISSIONS,
          "exactly the seven permission booleans")

    # The manifest version and the build version are written in two places
    # and nothing made them agree, so they drifted: the manifest said 0.1.0
    # while the package was 0.2.1. The store would have taken the manifest's
    # word for it and published a version number that matched no artifact.
    import re
    cmake = pathlib.Path(__file__).parent / "app" / "CMakeLists.txt"
    build_version = None
    if cmake.exists():
        m = re.search(r"project\(p4wnp1_panel\s+VERSION\s+([0-9.]+)", cmake.read_text())
        if m:
            build_version = m.group(1)
    check(build_version is not None, "the build version can be read from CMakeLists.txt")
    check(manifest.get("version") == build_version,
          f"manifest version {manifest.get('version')!r} matches the build {build_version!r}")

    # Every screenshot the manifest promises has to exist, or the listing
    # ships with holes in it.
    shots = store.get("screenshots", [])
    check(len(shots) >= 1, f"{len(shots)} screenshots listed")
    for rel in shots:
        path = pathlib.Path(__file__).parent / "app" / rel
        check(path.exists(), f"screenshot present: {rel}")

    blob = json.dumps(manifest)
    check(not any(ph in blob for ph in ("TODO", "TEMP")), "no template placeholders left")

    print()
    if failures:
        print(f"  {len(failures)} to fix before publishing")
        return 1
    print("  MANIFEST READY")
    print("  Still required for a FIRST release: a short video of the app running")
    print("  on a real device, posted in the packages PR. Updates merge without one.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
