# Proteus — P4wnP1 on the Cardputer Zero, as an app

Draft plan, 2026-11-24. No hardware yet; every claim below is labelled.

**The thesis in one line:** you should be able to `apt install` your USB red-team rig onto the
stock Cardputer Zero OS, use it, and remove it — never swapping SD cards to change what the
device is.

---

## This is not a port

I went in expecting to port P4wnP1 A.L.O.A. and that is the wrong frame. Porting the *image*
is exactly the SD-card swapping we are trying to escape. And `at0m-b0mb/P4wnP1_aloa` is
already **v0.9.0 with 160 of your commits** — the hard parts exist:

| already done | where |
|---|---|
| aarch64 build | `make build-arm64`, and `build/arm64/` is populated |
| USB gadget composition via configfs | `service/SubSysUSB.go` |
| HID engine + DuckyScript | `hid/`, `duckyscript/` |
| HTTP RPC API | `POST /api/v1/rpc/{method}` on `:8000` |
| a small-screen client | `oled/` — 38 files, 10,523 lines |
| a **no-hardware simulator** | `make oled-sim` (browser), `make oled-shots` (contact sheet) |
| install payload | `dist/` — units, HIDScripts, keymaps, ums, www, scripts, db |
| update + rollback | verified on hardware |

So the work is three things: **packaging**, **a 320x170 front-end**, and **stock-OS
integration**. Not a port.

## What actually reuses, honestly

I checked rather than assumed, and my first read was too optimistic.

- **`oled/client.go` — reuse wholesale.** It is a plain HTTP RPC client
  (`POST {BaseURL}/api/v1/rpc/{method}`), plus an endpoint-budget model mirrored from
  `SubSysUSB.go`. Transport and domain logic, no rendering.
- **The information model — reuse as design.** The 18 screens in
  `image/out/oled-screens.png` are a complete, mature spec of what the UI must say, including
  the honesty lines ("The USB connection to the host will re-enumerate", "no raw pin access",
  "the WiFi subsystem is unavailable on this device").
- **The renderer — does not reuse.** `NewFramebuffer()` takes no arguments and writes a fixed
  array exposed through `Page(p int)`: it is hard-wired to 128x64 page-addressed mono, for
  SSD1306/SH1106. `font5x7.go`, `display.go` and `panel_linux.go` are panel-specific too.

So roughly: the client and domain layer carry over, the view layer is new. Calling the whole
10,523 lines reusable would have been wrong.

## The constraints that shape everything

**1. One USB controller.** `SOURCE`, needs hardware to confirm. The CM0 has a single dwc2
controller, and USB-A, the GL852G hub and the SR9900A USB-Ethernet all hang off it. In gadget
mode you **lose USB-A and wired Ethernet**. You **keep** the LCD (SPI), the keyboard
(TCA8418 on I2C), **Wi-Fi (SDIO)**, microSD, IR, audio, camera and the IMU. Wi-Fi surviving
is what preserves the entire P4wnP1 workflow: BadUSB into a target, driven from the screen,
with Wi-Fi for C2. There is also a physical Host/Slave switch, so entering device mode may
need a hardware flip rather than software alone.

**2. USB gadget is available on the stock OS.** `SOURCE`, confirmed. The dtoverlays
`config_setup` runs `rpi-config.py --no-backup section-add all dtoverlay dwc2` — the `[all]`
section, no `dr_mode=`, so it defaults to OTG. Do not be misled by
`stage1/00-boot-files/files/config.txt`: its `otg_mode=1` and `dwc2,dr_mode=host` sit in
`[cm4]`/`[cm5]` blocks that do not apply to a CM0. `libcomposite` is not in
`modules-load.d`, so the app must modprobe it.

**3. Networking must be borrowed, not seized.** `SOURCE`, and this is the critical one.
`install.sh:404` does `systemctl disable NetworkManager`. The Cardputer Zero's own
LaunchWizard and Settings pages *depend* on NetworkManager. Installing P4wnP1 as-is breaks
the device's networking UI — which is your own recorded P4wnP1 lesson, *"adding auth broke
the device's own boot scripts."*

> **The design rule that follows:** the app stops NetworkManager when **arming** a network
> mode and restores it on **disarm**, with a guaranteed-restore path on crash, cancel and
> uninstall. Never at install time. This single property is what makes it an app you can
> remove rather than an OS you commit to.

**4. GPL-3.0.** P4wnP1 is GPL-3.0 and this is a derivative, so Proteus is GPL-3.0 — declared
in `app-builder.json`'s SPDX `license` field. Unlike your MIT Pharos/Aegis work. Worth
stating so it does not get mislabelled at publish time.

**5. The arm64 build has never been booted.** Your own `RESUME.md` says so. The Cardputer
Zero would be the **first real arm64 validation** of this codebase. Opportunity and risk
both.

## Workstreams

### A. Debian packaging — replaces `install.sh`

`install.sh` is 456 lines that reconfigure a whole system: apt installs, binaries to
`/usr/local/bin`, payload to `/usr/local/P4wnP1`, units to `/etc/systemd/system`, config
backups, and disabling conflicting services. A `.deb` must split that into:

- **package payload** — binaries, `dist/`, units, defaults
- **`postinst`** — enable the service, nothing destructive
- **runtime, not install time** — every networking takeover
- **`prerm`/`postrm`** — stop, disarm, restore NetworkManager, leave the device as found

Testable here: build it, `dpkg-deb -c` the layout, dry-run maintainer scripts. No device.

### B. The 320x170 front-end

The Cardputer Zero gives ~6.6x the pixels of the OLED **and a 46-key keyboard**. That is a
better interaction surface, not just a bigger one:

- "L/R then press" for Yes/No becomes actual Y/N keys
- payload lists get type-to-filter instead of scrolling
- the endpoint budget ("6/7 ep") becomes a gauge rather than a number
- armed/disarmed can be colour, not just text

Two credible routes, and this is the main open decision:

| | Go + direct framebuffer | C++ / LVGL Template |
|---|---|---|
| reuses `oled/client.go` | yes, directly | no — reimplement the RPC client in C++ |
| look | ours to build | native Cardputer, matches built-in apps |
| input | read evdev ourselves | Template already handles the keypad |
| APPLaunch fit | fine — it just execs a binary | idiomatic |
| effort | lower | higher |

My lean is **Go + framebuffer** for the first working version, because it reuses the client
and domain layer and gets something real on screen fastest; LVGL later if the look matters.
Worth deciding before any code.

### C. Stock-OS integration

- `.desktop` into `/usr/share/APPLaunch/applications/` so it appears in the launcher
- root operations: either the service's own systemd unit, or the launcher's
  `cp0_sudo_run_argv_async` — note that prompt currently discloses nothing
  (CardputerZero/launcher#278, which we filed)
- declare the Store permissions block honestly and narrowly, and say in the README that the
  block is advisory (CardputerZero/AppBuilder#24, also ours)

## Milestones

**M1 — it installs and uninstalls cleanly.** A `.deb` that drops the arm64 service onto the
stock OS, starts it, and on removal leaves no trace and no disabled NetworkManager.
*Proves the thesis. Fully testable here — no device.*

**M2 — the UI talks to the service.** The 320x170 front-end driving the real RPC API against
a service running locally. *Testable here: run the service on this Mac or in a container and
point the UI at it.*

**M3 — gadget composition on hardware.** First boot on the Cardputer Zero: confirm dwc2
comes up in device mode, what the Host/Slave switch does, and what is actually lost in gadget
mode. *Needs the device. Also the first arm64 boot this codebase has ever had.*

**M4 — HID into a target.** DuckyScript to a real host. *Needs the device and a target.*

## Open questions for hardware day

1. Does the Host/Slave switch gate dwc2 in software, or is it purely physical?
2. In gadget mode, what exactly disappears — confirm USB-A and Ethernet drop, Wi-Fi survives.
3. Does `libcomposite` load, and are all the `usb_f_*` functions present in the stock kernel?
4. The endpoint budget: the dwc2 note in `oled/client.go` says seven usable endpoints on a
   Pi Zero W. Is CM0 the same?
5. Does the stock OS fight us for `/dev/fb0` — APPLaunch is a framebuffer app too.
