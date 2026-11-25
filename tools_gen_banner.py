#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 at0m-b0mb
# SPDX-License-Identifier: GPL-3.0-only
"""
Repo art for the P4wnP1 CardputerZero panel.

Three assets, all carrying REAL device captures rather than drawings of them:

  images/social-preview.png     1280x640, 80px safe border, uploaded by hand
  images/banner{,-dark}.png     2560x800, a light/dark pair for the README
  images/screens{,-dark}.png    contact sheet, also a pair

The captures are the actual 320x170 simulator framebuffers from
app/screenshot/, scaled by an INTEGER factor with NEAREST. Resampling a
320x170 panel non-integrally is how a screenshot starts looking like a drawing
of a screenshot.

Every rectangle that carries meaning is registered with SafeBox as it is drawn,
so the margin is asserted rather than eyeballed. Background art is never
registered and stays free to bleed.
"""

import os

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "images")
SHOTS = os.path.join(HERE, "app", "screenshot")

# A Didone for identity, a neutral sans for anything read, mono for measured
# values. The same split the house style asks for everywhere.
DIDOT = "/System/Library/Fonts/Supplemental/Didot.ttc"
AVENIR = "/System/Library/Fonts/Avenir Next.ttc"
MONO = "/System/Library/Fonts/Supplemental/Andale Mono.ttf"
FALLBACK = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"

# The app's own palette, so the art and the product cannot drift apart.
PAPER = (0xF3, 0xF1, 0xEC)
CARD = (0xFF, 0xFF, 0xFF)
INK = (0x1A, 0x17, 0x13)
INK_2 = (0x5C, 0x56, 0x4B)
RULE_L = (0xD9, 0xD3, 0xC7)
BRASS_L = (0x80, 0x63, 0x1D)

BLACK = (0x00, 0x00, 0x00)
SURF_D = (0x0E, 0x0E, 0x0E)
INK_D = (0xED, 0xE8, 0xE0)
INK_D2 = (0xC2, 0xBB, 0xB1)
RULE_D = (0x2A, 0x2A, 0x2A)
BRASS_D = (0xD7, 0xB4, 0x5A)
SHINE_D = (0xF0, 0xCE, 0x6A)

SAFE = 80      # GitHub's 40pt border at this 2x canvas
CUSHION = 96   # author here, assert at SAFE


class SafeBox:
    """Records every rectangle that carries meaning, so the margin can be
    asserted instead of eyeballed. Background art is never registered."""

    def __init__(self, w, h, margin):
        self.w, self.h, self.margin = w, h, margin
        self.boxes = []

    def add(self, x0, y0, x1, y1, what):
        self.boxes.append((int(x0), int(y0), int(x1), int(y1), what))

    def check(self):
        if not self.boxes:
            return None
        worst = None
        for x0, y0, x1, y1, what in self.boxes:
            m = min(x0, y0, self.w - x1, self.h - y1)
            if worst is None or m < worst[0]:
                worst = (m, what, (x0, y0, x1, y1))
        m, what, box = worst
        if m < self.margin:
            raise AssertionError(
                f"{what} at {box} leaves a {m}px margin on a {self.w}x{self.h} "
                f"canvas; the safe border needs {self.margin}px. Move it inside "
                f"x {self.margin}..{self.w - self.margin}, "
                f"y {self.margin}..{self.h - self.margin}.")
        return m, what


def font(path, px, index=0):
    try:
        return ImageFont.truetype(path, px, index=index)
    except OSError:
        return ImageFont.truetype(FALLBACK, px)


def capture(name, scale):
    """A real 320x170 framebuffer, enlarged by an integer factor only."""
    im = Image.open(os.path.join(SHOTS, name)).convert("RGB")
    if im.size != (320, 170):
        raise SystemExit(f"{name} is {im.size}, expected the panel's 320x170")
    return im.resize((im.width * scale, im.height * scale), Image.NEAREST)


def text_size(draw, s, f):
    x0, y0, x1, y1 = draw.textbbox((0, 0), s, font=f)
    return x1 - x0, y1 - y0


def tracked(draw, xy, s, f, fill, spacing):
    """Letter-spaced text. Returns the drawn width."""
    x, y = xy
    for ch in s:
        draw.text((x, y), ch, font=f, fill=fill)
        x += draw.textlength(ch, font=f) + spacing
    return x - spacing - xy[0]


def endpoint_strip(draw, x, y, w, used, total, on, off, gap=6, h=14):
    """The signature element, drawn from real numbers: one cell per USB
    endpoint, filled when spent. The shape carries the reading."""
    cell = (w - gap * (total - 1)) // total
    for i in range(total):
        cx = x + i * (cell + gap)
        box = (cx, y, cx + cell, y + h)
        if i < used:
            draw.rectangle(box, fill=on)
        else:
            draw.rectangle(box, outline=off, width=2)
    return x + total * (cell + gap) - gap, y + h


def panel(dark):
    if dark:
        return dict(bg=BLACK, surf=SURF_D, ink=INK_D, ink2=INK_D2,
                    rule=RULE_D, brass=BRASS_D, shine=SHINE_D,
                    shot="status-live-dark.png", cable="cable-dark.png")
    return dict(bg=PAPER, surf=CARD, ink=INK, ink2=INK_2,
                rule=RULE_L, brass=BRASS_L, shine=BRASS_L,
                shot="status-live-light.png", cable="cable-light.png")


def render_social(path):
    W, H = 1280, 640
    p = panel(dark=True)          # the card reads best on true black
    img = Image.new("RGB", (W, H), p["bg"])
    d = ImageDraw.Draw(img)
    sb = SafeBox(W, H, SAFE)

    # A hairline frame, bleeding art is allowed but this is decoration only.
    d.rectangle((40, 40, W - 41, H - 41), outline=p["rule"], width=2)

    # --- identity, left column -------------------------------------------
    x = CUSHION
    f_name = font(DIDOT, 92)
    f_sub = font(AVENIR, 26, index=0)
    f_tag = font(AVENIR, 23, index=0)
    f_mono = font(MONO, 20)

    y = 170
    w_name, h_name = text_size(d, "P4wnP1", f_name)
    d.text((x, y), "P4wnP1", font=f_name, fill=p["ink"])
    sb.add(x, y, x + w_name, y + h_name + 24, "wordmark")

    y += h_name + 34
    w_sub = tracked(d, (x, y), "A.L.O.A.", f_sub, p["brass"], 7)
    sb.add(x, y, x + w_sub, y + 30, "sub-wordmark")

    y += 56
    for line, col in (("A USB red-team rig you install,", p["ink"]),
                      ("not an OS you commit to.", p["ink2"])):
        w_l, h_l = text_size(d, line, f_tag)
        d.text((x, y), line, font=f_tag, fill=col)
        sb.add(x, y, x + w_l, y + h_l + 8, "tagline")
        y += 34

    # --- the signature, from real numbers ---------------------------------
    y += 26
    cap = "USB ENDPOINTS"
    w_c, h_c = text_size(d, cap, f_mono)
    d.text((x, y), cap, font=f_mono, fill=p["ink2"])
    sb.add(x, y, x + w_c, y + h_c + 4, "meter caption")
    y += h_c + 14
    ex, ey = endpoint_strip(d, x, y, 300, used=3, total=7,
                            on=p["shine"], off=p["rule"])
    sb.add(x, y, ex, ey, "endpoint meter")
    tally = "3 of 7"
    d.text((x + 316, y - 3), tally, font=f_mono, fill=p["ink"])
    sb.add(x + 316, y - 3, x + 316 + text_size(d, tally, f_mono)[0], ey, "tally")

    # --- the product itself, at integer scale -----------------------------
    shot = capture(p["shot"], 2)                       # 640 x 340
    sx, sy = W - CUSHION - shot.width, (H - shot.height) // 2
    d.rectangle((sx - 3, sy - 3, sx + shot.width + 2, sy + shot.height + 2),
                outline=p["rule"], width=2)
    img.paste(shot, (sx, sy))
    sb.add(sx - 3, sy - 3, sx + shot.width + 3, sy + shot.height + 3, "device capture")

    note = "REAL CAPTURE  320x170"
    f_note = font(MONO, 16)
    w_n, h_n = text_size(d, note, f_note)
    nx = min(max(sx, SAFE), W - SAFE - w_n)
    ny = sy + shot.height + 14
    d.text((nx, ny), note, font=f_note, fill=p["ink2"])
    sb.add(nx, ny, nx + w_n, ny + h_n + 4, "capture caption")

    m, what = sb.check()
    img.save(path)
    print(f"  {os.path.basename(path)}  {W}x{H}  tightest margin {m}px ({what})")


def render_banner(path, dark):
    # 1280x400 grid at 2x. No safe border: nothing crops a README.
    W, H = 2560, 800
    p = panel(dark)
    img = Image.new("RGB", (W, H), p["bg"])
    d = ImageDraw.Draw(img)

    d.rectangle((0, H - 8, W, H), fill=p["brass"])   # a carrier band may bleed

    # Lay the captures out first, so the text column knows how much room it
    # actually has. Writing the text first is how it ended up running
    # underneath them.
    shots = []
    for i, name in enumerate((p["shot"], p["cable"])):
        shot = capture(name, 2)
        sx = W - 150 - (2 - i) * (shot.width + 40) + 40
        sy = (H - shot.height) // 2
        shots.append((shot, sx, sy))
    text_right = min(sx for _, sx, _ in shots) - 70

    x = 150
    avail = text_right - x
    f_name = font(DIDOT, 150)
    f_sub = font(AVENIR, 38, index=0)
    f_tag = font(AVENIR, 34, index=0)

    y = 176
    d.text((x, y), "P4wnP1", font=f_name, fill=p["ink"])
    y += text_size(d, "P4wnP1", f_name)[1] + 40
    w_sub = tracked(d, (x, y), "A.L.O.A.", f_sub, p["brass"], 10)
    y += 70

    lines = ["Install it, use it, remove it.", "The device keeps its own settings."]
    widths = [w_sub]
    for line in lines:
        w_l = text_size(d, line, f_tag)[1 - 1]
        widths.append(w_l)
        d.text((x, y), line, font=f_tag, fill=p["ink2"])
        y += 46

    # Nothing in the text column may reach the captures.
    overflow = max(widths) - avail
    if overflow > 0:
        raise AssertionError(
            f"banner text is {overflow}px wider than the {avail}px column before "
            f"the captures at x={text_right + 70}; shorten it or move them right.")

    for shot, sx, sy in shots:
        d.rectangle((sx - 3, sy - 3, sx + shot.width + 2, sy + shot.height + 2),
                    outline=p["rule"], width=2)
        img.paste(shot, (sx, sy))

    img.save(path)
    print(f"  {os.path.basename(path)}  {W}x{H}  ({'dark' if dark else 'light'})")


def render_screens(path, dark):
    p = panel(dark)
    names = [("status-live-" + ("dark" if dark else "light") + ".png", "Status"),
             ("cable-" + ("dark" if dark else "light") + ".png", "Cable"),
             ("payloads-" + ("dark" if dark else "light") + ".png", "Payloads")]
    shots = [(capture(n, 2), label) for n, label in names]

    pad, gap, cap_h = 40, 32, 46
    w = pad * 2 + sum(s.width for s, _ in shots) + gap * (len(shots) - 1)
    h = pad * 2 + shots[0][0].height + cap_h
    img = Image.new("RGB", (w, h), p["bg"])
    d = ImageDraw.Draw(img)
    f_cap = font(MONO, 22)

    x = pad
    for shot, label in shots:
        d.rectangle((x - 3, pad - 3, x + shot.width + 2, pad + shot.height + 2),
                    outline=p["rule"], width=2)
        img.paste(shot, (x, pad))
        d.text((x, pad + shot.height + 16), label, font=f_cap, fill=p["ink2"])
        x += shot.width + gap

    img.save(path)
    print(f"  {os.path.basename(path)}  {w}x{h}  ({'dark' if dark else 'light'})")


def rounded(d, box, r, fill=None, outline=None, width=2):
    d.rounded_rectangle(box, radius=r, fill=fill, outline=outline, width=width)


def arrow(d, x0, y0, x1, y1, col, width=3, head=11):
    """A straight arrow with a solid head. Only horizontal or vertical."""
    d.line((x0, y0, x1, y1), fill=col, width=width)
    if x0 == x1:
        s = 1 if y1 > y0 else -1
        d.polygon([(x1, y1), (x1 - head // 2, y1 - s * head), (x1 + head // 2, y1 - s * head)], fill=col)
    else:
        s = 1 if x1 > x0 else -1
        d.polygon([(x1, y1), (x1 - s * head, y1 - head // 2), (x1 - s * head, y1 + head // 2)], fill=col)


def render_flow(path, dark):
    """Where every key takes you.

    Drawn rather than written because the interesting part is not the list of
    keys, it is that the two destructive actions sit behind the same confirm
    shape, and that Esc means 'dismiss' before it means 'back'."""
    p = panel(dark)
    W, H = 2200, 1060
    img = Image.new("RGB", (W, H), p["bg"])
    d = ImageDraw.Draw(img)

    f_h = font(DIDOT, 54)
    f_page = font(AVENIR, 34, index=0)
    f_body = font(AVENIR, 25, index=0)
    f_key = font(MONO, 23)
    f_note = font(AVENIR, 23, index=0)

    d.text((80, 62), "Where the keys take you", font=f_h, fill=p["ink"])
    d.line((80, 148, W - 80, 148), fill=p["rule"], width=2)

    col_w, col_x0, gap = 560, 80, 90
    top = 210

    pages = [
        ("Status", "read only", [
            ("", "Nothing here changes the device."),
            ("", "It reports what the service reports."),
        ]),
        ("Cable", "compose USB", [
            ("Up Dn", "move between functions"),
            ("Lf Rt", "jump column"),
            ("Space", "toggle one on or off"),
            ("Enter", "deploy, after a confirm"),
        ]),
        ("Payloads", "run HID scripts", [
            ("Up Dn", "move between payloads"),
            ("Enter", "run, after a confirm"),
            ("X", "stop everything running"),
        ]),
    ]

    centres = []
    for i, (name, kind, rows) in enumerate(pages):
        x = col_x0 + i * (col_w + gap)
        box = (x, top, x + col_w, top + 300)
        rounded(d, box, 14, fill=p["surf"], outline=p["rule"], width=2)
        d.text((x + 28, top + 24), name, font=f_page, fill=p["ink"])
        d.text((x + 28, top + 70), kind.upper(), font=f_key, fill=p["brass"])
        yy = top + 118
        for key, what in rows:
            if key:
                d.text((x + 28, yy), key, font=f_key, fill=p["brass"])
                d.text((x + 150, yy), what, font=f_body, fill=p["ink2"])
            else:
                d.text((x + 28, yy), what, font=f_body, fill=p["ink2"])
            yy += 40
        centres.append((x + col_w // 2, box))

    # The page key cycles the three, so the arrows wrap.
    for i in range(2):
        x0 = centres[i][1][2]
        x1 = centres[i + 1][1][0]
        arrow(d, x0 + 10, top + 150, x1 - 10, top + 150, p["brass"])
    d.text((centres[0][1][2] + 24, top + 108), "page", font=f_key, fill=p["brass"])
    d.text((centres[1][1][2] + 24, top + 108), "page", font=f_key, fill=p["brass"])

    # --- the confirm shape, shared by both destructive actions ------------
    cy = top + 420
    d.text((80, cy - 56), "Both destructive actions use the same shape",
           font=f_page, fill=p["ink"])

    stages = [
        ("Selected", "nothing has happened yet", p["ink2"]),
        ("Confirm", "the consequence is named", p["brass"]),
        ("Running", "the panel is blocked", p["ink2"]),
        ("Result", "in the service's own words", p["ink2"]),
    ]
    bw, bgap = 430, 120
    for i, (title, sub, col) in enumerate(stages):
        x = 80 + i * (bw + bgap)
        box = (x, cy, x + bw, cy + 140)
        rounded(d, box, 14, fill=p["surf"],
                outline=p["brass"] if i == 1 else p["rule"], width=3 if i == 1 else 2)
        d.text((x + 26, cy + 26), title, font=f_page, fill=col)
        d.text((x + 26, cy + 80), sub, font=f_note, fill=p["ink2"])
        if i < len(stages) - 1:
            arrow(d, x + bw + 14, cy + 70, x + bw + bgap - 14, cy + 70, p["rule"])

    d.text((80 + bw + 24, cy + 158), "Enter", font=f_key, fill=p["brass"])
    d.text((80 + 2 * bw + bgap + 24, cy + 158), "Y", font=f_key, fill=p["brass"])

    # N and Esc come back from Confirm, and Esc dismisses a Result.
    back_y = cy + 206
    d.line((80 + bw + bgap + bw // 2, cy + 140, 80 + bw + bgap + bw // 2, back_y),
           fill=p["rule"], width=3)
    d.line((80 + bw // 2, back_y, 80 + bw + bgap + bw // 2, back_y), fill=p["rule"], width=3)
    arrow(d, 80 + bw // 2, back_y, 80 + bw // 2, cy + 148, p["rule"])
    d.text((80 + bw + 40, back_y + 12), "N or Esc", font=f_key, fill=p["ink2"])

    # --- the one sentence worth drawing -----------------------------------
    note_y = H - 170
    d.line((80, note_y - 28, W - 80, note_y - 28), fill=p["rule"], width=2)
    for i, line in enumerate([
        "Esc dismisses a confirm or a result first. Only on an idle screen does it mean go back.",
        "Deploying re-enumerates the host's USB connection. Running a payload types into it. Both say so before they happen.",
    ]):
        d.text((80, note_y + i * 40), line, font=f_note,
               fill=p["ink"] if i == 0 else p["ink2"])

    img.save(path)
    print(f"  {os.path.basename(path)}  {W}x{H}  ({'dark' if dark else 'light'})")


def render_icon(path, px=256):
    """The store icon: 256px square, inside the 128-512 the store requires.

    The mark is the endpoint budget -- seven cells, three spent -- because it
    is the one thing only this app shows. Kept to bars rather than a glyph so
    it still reads when the store shrinks it.

    Separate from the launcher icons, which stay at the sizes APPLaunch is
    given on a 320x170 screen.
    """
    SS = 4
    s = px * SS
    img = Image.new("RGB", (s, s), BLACK)
    d = ImageDraw.Draw(img)

    pad = int(s * 0.14)
    rounded(d, (pad // 2, pad // 2, s - pad // 2, s - pad // 2), int(s * 0.17),
            fill=SURF_D, outline=RULE_D, width=max(2, s // 128))

    total, used = 7, 3
    gap = int(s * 0.028)
    inner = s - pad * 2
    cell = (inner - gap * (total - 1)) // total
    top, bot = int(s * 0.30), int(s * 0.70)
    for i in range(total):
        x = pad + i * (cell + gap)
        box = (x, top, x + cell, bot)
        if i < used:
            d.rounded_rectangle(box, radius=max(2, cell // 5), fill=SHINE_D)
        else:
            d.rounded_rectangle(box, radius=max(2, cell // 5),
                                outline=RULE_D, width=max(2, s // 150))

    img = img.resize((px, px), Image.LANCZOS)
    img.save(path)
    print(f"  {os.path.basename(path)}  {px}x{px}  (store icon)")


def main():
    os.makedirs(OUT, exist_ok=True)
    print("Repo art, from real 320x170 captures:")
    render_social(os.path.join(OUT, "social-preview.png"))
    render_banner(os.path.join(OUT, "banner.png"), dark=False)
    render_banner(os.path.join(OUT, "banner-dark.png"), dark=True)
    render_screens(os.path.join(OUT, "screens.png"), dark=False)
    render_screens(os.path.join(OUT, "screens-dark.png"), dark=True)
    render_flow(os.path.join(OUT, "flow.png"), dark=False)
    render_flow(os.path.join(OUT, "flow-dark.png"), dark=True)
    render_icon(os.path.join(HERE, "app", "assets", "images", "store-icon.png"), 256)
    print("\nThe social preview must be uploaded by hand:")
    print("  Settings -> General -> Social preview -> Upload an image")


if __name__ == "__main__":
    main()
