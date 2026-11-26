# p4wnp1-aloa — the engine, as a Debian package

`p4wnp1-panel` is the face. This is the thing behind it: the P4wnP1 A.L.O.A.
service, CLI, payload tree, keymaps, web console and systemd units, installable
with `apt` and removable without a trace.

Two packages, deliberately:

| | |
|---|---|
| `p4wnp1-aloa` | the engine — service on `:8000`, USB gadget, HID, DuckyScript |
| `p4wnp1-panel` | the 320x170 on-device front-end that talks to it |

The panel runs without the engine (it will tell you the service is not
answering) and the engine runs without the panel (drive it from the web console
or the CLI). Install both for a device that does the whole job from its own
screen.

```bash
sudo apt install ./p4wnp1-aloa_0.9.0_arm64.deb ./p4wnp1-panel_0.5.0_rev1_arm64.deb
sudo reboot            # USB gadget mode needs dtoverlay=dwc2 and a UDC
```

## What it does to your device, in full

**Networking: nothing, until you arm a mode.**

This is the whole reason the package exists rather than `install.sh`. Upstream's
installer runs `systemctl disable NetworkManager` at install time, and the
CardputerZero's own LaunchWizard and Settings pages depend on NetworkManager —
so installing P4wnP1 the upstream way breaks the device's networking UI. The
postinst here changes no network service state at all. The panel stops
NetworkManager when a network mode is **armed** and starts it again on
**disarm**, on crash, and on removal of either package.

**Files**

| path | what | survives removal? |
|---|---|---|
| `/usr/local/bin/P4wnP1_service`, `_cli`, `p4wnp1-hashpw`, `p4wnp1-update` | binaries | no |
| `/usr/share/p4wnp1-aloa/payload/` | pristine shipped content | no |
| `/usr/lib/systemd/system/P4wnP1.service`, `p4wnp1-firstboot.service` | units | no |
| `/var/lib/p4wnp1/` | **your** live tree: `store/`, `ums/`, payloads you wrote | **yes** — only `apt purge` removes it |
| `/usr/local/P4wnP1` | symlink to `/var/lib/p4wnp1` | no (the link goes, the data stays) |
| `/etc/p4wnp1/initial.conf` | per-device WiFi SSID + passphrase, 0600 | yes, unless purged |

On upgrade, shipped directories (`keymaps`, `www`, `helper`, `legacy`) are
**replaced** and `scripts`/`HIDScripts` are **merged**, so payloads you wrote
are left alone. `store/` and `ums/` are never touched. That is the same rule
`p4wnp1-update` follows, for the same reason.

**Credentials are per-device and generated at install.** No shared default ever
ships. Read the passphrase with `sudo cat /etc/p4wnp1/initial.conf`.

## Why it installs into `/usr/local`

Debian policy says packages should not, and this one does. Fourteen shipped
files hardcode those paths — `common/globals.go` sets
`PATH_ROOT = "/usr/local/P4wnP1"`, both units `ExecStart=/usr/local/bin/...`,
and the healthcheck, firstboot and updater scripts all look there. Relocating
would mean patching a tested engine on its way into a package, at the one point
in the pipeline where nothing re-tests it.

The compromise: binaries in `/usr/local/bin`, and the writable tree in
`/var/lib/p4wnp1` with `/usr/local/P4wnP1` as a symlink. Hardcoded paths
resolve, and state lives where state belongs.

## Built from the released update bundle

```bash
./engine/build-engine-deb.sh --version v0.9.0
./engine/build-engine-deb.sh --bundle ../../P4wnP1_aloa/image/out/P4wnP1-ALOA-v0.9.0-update-arm64.tar.xz
```

The `.deb` is packed from the same `-update-arm64.tar.xz` that
`p4wnp1-update` installs, so an apt install and an in-place update cannot drift
apart and there is no second definition of what P4wnP1 consists of.

Note that **v0.8.0 and earlier shipped no update bundle at all** — images only —
so `--version` has nothing to fetch for those tags. v0.9.0 is the first release
with both architectures' bundles attached.

## Not verified on hardware

Consistent with the rest of this repository, and the list is short because the
package is new:

- **Nothing here has run on a CardputerZero.** The layout, maintainer scripts
  and seeding logic are tested by building the package and running
  `dpkg --install` / `--remove` / `--purge` in a Debian trixie arm64 container,
  which is a real `dpkg` doing real work — but it is not the device.
- **USB gadget mode on a CM0 is unconfirmed.** `dtoverlay=dwc2` is present in
  the stock image's `[all]` section with no `dr_mode`, so it should default to
  OTG, and there is a physical Host/Slave switch whose software visibility is
  unknown.
- **This would be the first arm64 boot this engine has ever had.** The P4wnP1
  arm64 images have never been booted on any board.
