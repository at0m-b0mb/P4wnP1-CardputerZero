#!/usr/bin/env python3
"""A narrated tour of the panel, as an animated GIF.

Frames are the real 320x170 framebuffers from app/screenshot/, scaled by an
integer factor with NEAREST so no pixel is invented. The caption under each
says what you are looking at and, where it matters, why it is worded the way
it is -- the point of several of these screens is what they refuse to claim,
which a silent slideshow would not convey.
"""
import os, sys
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.path.join(HERE, "app", "screenshot")
OUT = os.path.join(HERE, "images")

DIDOT = "/System/Library/Fonts/Supplemental/Didot.ttc"
AVENIR = "/System/Library/Fonts/Avenir Next.ttc"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
FALLBACK = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

PAPER = (0xF3, 0xF1, 0xEC)
INK   = (0x1A, 0x17, 0x13)
INK2  = (0x5C, 0x56, 0x4B)
BRASS = (0x8A, 0x6A, 0x1F)
RULE  = (0xD8, 0xD3, 0xC8)

SCALE = 2
HOLD  = 2000   # ms per frame
LONG  = 2800   # ms for the ones worth reading twice

# (file, title, caption, duration)
TOUR = [
    ("status-live-light.png", "Status",
     "What the service reports. Re-read every five seconds.", HOLD),
    ("cable-light.png", "Cable",
     "Compose a USB gadget. The meter counts endpoints before you deploy.", HOLD),
    ("cable-refused.png", "Cable / over budget",
     "Refused here, with what to turn off -- not by the device afterwards.", LONG),
    ("cable-confirm.png", "Cable / confirm",
     "The consequence is named before it happens, never after.", LONG),
    ("cable-deployed.png", "Cable / deployed",
     "The host sees the new gadget.", HOLD),
    ("payloads-light.png", "Payloads",
     "Stored HID scripts, listed by the names the service gives them.", HOLD),
    ("payloads-confirm.png", "Payloads / confirm",
     "\"Run\" is named, not implied: it types into whatever is plugged in.", LONG),
    ("loadouts-light.png", "Loadouts",
     "Saved bundles of USB, network and reflex settings. One boots.", HOLD),
    ("loadouts-confirm.png", "Loadouts / apply",
     "A loadout changes USB and networking at once, so it says so.", LONG),
    ("loadouts-delete.png", "Loadouts / delete",
     "The only wording in the app that says it cannot be undone.", LONG),
    ("jobs-light.png", "Jobs",
     "What is running now. Re-read every two seconds: jobs end by themselves.", HOLD),
    ("system-light.png", "System",
     "Which build this is, what it is talking to, and the two ways out.", HOLD),
    ("system-reboot.png", "System / reboot",
     "It ends the session drawing this screen, so it cannot report success.", LONG),
    ("status-lost.png", "When the service dies",
     "It stops saying \"up\", dims every figure, and says how old they are.", LONG),
    ("loadouts-noservice.png", "No service at all",
     "An empty shelf and an unreachable service are worded differently.", LONG),
    ("status-redacted.png", "P4WNP1_REDACT=1",
     "Your addresses and SSID masked, so a screenshot is safe to publish.", LONG),
]


def font(path, px, index=0):
    try:
        return ImageFont.truetype(path, px, index=index)
    except OSError:
        return ImageFont.truetype(FALLBACK, px)


def build():
    f_title = font(AVENIR, 26, index=0)
    f_cap   = font(AVENIR, 19, index=0)
    f_tag   = font(MONO, 15)

    shot_w, shot_h = 320 * SCALE, 170 * SCALE
    pad, cap_h = 26, 92
    W = shot_w + pad * 2
    H = pad + shot_h + cap_h

    frames, durations = [], []
    missing = []
    for name, title, caption, ms in TOUR:
        path = os.path.join(SHOTS, name)
        if not os.path.exists(path):
            missing.append(name)
            continue
        shot = Image.open(path).convert("RGB").resize((shot_w, shot_h), Image.NEAREST)

        img = Image.new("RGB", (W, H), PAPER)
        d = ImageDraw.Draw(img)
        d.rectangle((pad - 2, pad - 2, pad + shot_w + 1, pad + shot_h + 1),
                    outline=RULE, width=2)
        img.paste(shot, (pad, pad))

        y = pad + shot_h + 18
        d.text((pad, y), title, font=f_title, fill=INK)
        d.text((pad, y + 34), caption, font=f_cap, fill=INK2)
        tag = f"{len(frames) + 1}/{len([t for t in TOUR if os.path.exists(os.path.join(SHOTS, t[0]))])}"
        d.text((W - pad - d.textlength(tag, font=f_tag), y + 4), tag, font=f_tag, fill=BRASS)

        frames.append(img)
        durations.append(ms)

    if missing:
        print("  missing captures, skipped: " + ", ".join(missing))
    if not frames:
        print("  no frames; nothing written")
        return 1

    os.makedirs(OUT, exist_ok=True)
    out = os.path.join(OUT, "tour.gif")
    frames[0].save(out, save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, optimize=True)
    size = os.path.getsize(out)
    print(f"  tour.gif  {W}x{H}  {len(frames)} frames  "
          f"{sum(durations)/1000:.0f}s  {size/1024:.0f} KB")
    if size > 10 * 1024 * 1024:
        print("  WARNING: over 10 MB; GitHub will be slow to render it")
    return 0


if __name__ == "__main__":
    print("Panel tour, from the real 320x170 captures:")
    sys.exit(build())
