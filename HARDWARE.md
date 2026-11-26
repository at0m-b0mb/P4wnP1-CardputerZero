# Bring-up: the day the CardputerZero arrives

Ordered so that nothing destructive happens until something safe has already told you it will
work. Every step says what you should see, so a surprise is recognisable as one.

This is also the **first arm64 boot this codebase has ever had**. Expect to find things; that
is the point of doing it in this order.

---

## 0. Before you install anything

Get a shell on the device and record the starting state. If something goes wrong later, this
is what "as it was" means.

```bash
systemctl is-enabled NetworkManager; systemctl is-active NetworkManager
ls /boot/firmware/config.txt && grep -n dwc2 /boot/firmware/config.txt
uname -m    # expect aarch64
```

**Expect:** `enabled`, `active`, and a `dtoverlay=dwc2` line in the `[all]` section.
If NetworkManager is *not* enabled here, stop and find out why before going further — the
custody logic assumes a reboot restores it, and that is only true while it is enabled.

---

## 1. Install

```bash
sudo apt install ./P4wnP1_0.5.0_rev1_arm64.deb
p4wnp1_panel --version
```

**Expect:** the version printed matches the file name. Nothing else happens — installing does
not start anything, does not touch networking, and does not enable a service.

---

## 2. Ask the networking custody what it *would* do

This changes nothing. It is the first thing to run because it answers, in one command, the
two questions that decide whether the feature works on this device at all.

```bash
p4wnp1_panel --netcheck
```

**Expect:**

```
  borrow recorded      no
  NetworkManager       running
  treating as armed    no

  nothing to do.
```

- `NetworkManager  UNKNOWN (...)` → systemd is not reachable the way the panel expects. The
  panel will refuse to touch networking rather than guess, which is correct, but the feature
  is off until that is understood.
- Run it as a **normal user** first and then under `sudo`. If the unprivileged run cannot
  read the state, that tells you the panel launched from APPLaunch will not be able to
  either — see step 6.

Then the armed side, still changing nothing:

```bash
p4wnp1_panel --netcheck-armed
```

**Expect:** `would record that networking is borrowed` / `would stop NetworkManager`, in that
order. The order is the safety property — the record is written *before* the stop, so a crash
in between leaves a recoverable state.

---

## 3. Run the panel

```bash
p4wnp1_panel
```

**Expect:** the Status screen, and within five seconds either real figures or **No service**
if P4wnP1 itself is not running. `No service` is a correct answer here, not a failure.

Walk the six screens with **Tab**. Check the things that only hardware can show:

- text is legible at 320×170 on the real panel, not just in the simulator
- the keypad reaches every key the screens claim: arrows, Enter, Y, N, X, B, Tab, Esc
- the LCD colours match the theme in both light and dark (`6` toggles)

---

## 4. The USB questions — the ones nothing off-device could answer

With P4wnP1 running and a host attached by the USB-C data port:

```bash
lsmod | grep -c dwc2
ls /sys/class/udc/          # non-empty means device mode is available
```

Then on the **Cable** screen compose **Keyboard only** and deploy.

**Expect:** the host enumerates a keyboard. Record what the physical Host/Slave switch does
— whether it gates this in software at all is listed in the README as unverified, and this is
where it stops being unverified.

Then record what gadget mode actually costs:

```bash
ip link            # is wired Ethernet gone?
iwconfig 2>/dev/null; nmcli dev status   # did Wi-Fi survive?
lsusb              # is USB-A still enumerating anything?
```

The README predicts **USB-A and wired Ethernet drop, Wi-Fi survives**, because all three hang
off the one dwc2 controller. Confirm or correct it.

**And the endpoint budget.** The panel assumes 7, the Pi Zero W figure. Tick keyboard +
RNDIS + CDC ECM + mass storage (8 by the panel's arithmetic) and watch whether the device
refuses with its own message naming a different number. That number is the real budget.

---

## 5. Networking custody, for real

Only now, with the dry runs understood. Compose a gadget **with RNDIS or CDC ECM** and
deploy.

**Expect:** the confirm says *"Deploy? USB re-enumerates and NetworkManager pauses."* — the
second clause appears only because a network function is selected. After deploying:

```bash
systemctl is-active NetworkManager        # expect: inactive
ls /run/p4wnp1-panel/                     # expect: networkmanager-borrowed
```

Now deploy **keyboard only** and check it comes back:

```bash
systemctl is-active NetworkManager        # expect: active
ls /run/p4wnp1-panel/                     # expect: empty
```

### The test that matters most

Borrow networking, then kill the panel without letting it disarm:

```bash
# with a network gadget deployed and NetworkManager stopped
pkill -9 p4wnp1_panel
systemctl is-active NetworkManager        # inactive, and nothing is running
p4wnp1_panel --restore-networking
systemctl is-active NetworkManager        # expect: active again
```

And the removal path:

```bash
# borrow again, then:
sudo apt remove p4wnp1-panel
systemctl is-active NetworkManager        # expect: active
ls /run/p4wnp1-panel/ 2>/dev/null         # expect: gone
```

**If any of these leaves NetworkManager down**, a reboot fixes it — the panel only ever
`stop`s the service, never `disable`s it, unlike P4wnP1's own `install.sh`. Say so in the bug
report, because that distinction is the whole safety argument.

---

## 6. If the panel cannot stop NetworkManager

Likely, and not a crisis. APPLaunch does not necessarily run apps as root, and
`systemctl stop` needs privileges.

**Expect, if so:** the Cable screen says *"Deployed, but NetworkManager needs root to pause."*
The deploy still succeeded — custody failing does not fail the deploy, because the gadget
really is up and saying otherwise would be the larger lie.

The fix is a decision, not a patch: either a polkit rule for this one unit, or run the panel
from its shipped systemd unit (which has no `User=`, so it runs as root) instead of from
APPLaunch. Record which you chose.

---

## 6b. Before any screenshot leaves the device

**The Status screen shows your own network.** `wlan0`'s address and the Wi-Fi SSID are real
once this runs on hardware, and the CardputerZero store *requires* screenshots — so the one
moment you most want a picture of the panel is the moment it is displaying something that
should not be published.

Capture with redaction on:

```bash
P4WNP1_REDACT=1 p4wnp1_panel
```

Addresses become `172.16.x.x`, the SSID becomes `<ssid>`, and anything you named yourself —
loadouts, payloads, job titles — becomes `item 1`, `item 2`. The shape survives, so the
screenshot still documents the panel; the identity does not. It is display-only: the panel
still knows the real values and still acts on them.

Then check the repo before pushing anything:

```bash
./scan-privacy.sh
```

It reads the working tree, **the whole git history**, commit identities and any built `.deb`,
and prints how many bytes it read — a scan that silently matched nothing and a scan that read
nothing look identical otherwise.

### What not to paste

Terminal output from the steps above is the likeliest leak, because it is the most useful
thing to quote in an issue. Before pasting any of it anywhere public, strip:

| from | what it reveals |
|---|---|
| `ip link` / `ip addr` | MAC addresses, your addressing |
| `nmcli dev status` / `iwconfig` | your SSID |
| `hostnamectl` / the shell prompt | your hostname, your username |
| `lsusb -v` | serial numbers of your own devices |
| `journalctl` | all of the above, mixed with everything else |

Quote the one line that shows the problem, not the whole command.

## 7. For the store

The CardputerZero store will not merge a first release without a video of the app running on
a real device. A phone recording is explicitly fine. Capture it once steps 3 and 4 pass —
the six screens and one real deploy are enough — then see [STORE.md](STORE.md).

---

## What to write down

Fill these in and the README's "not yet verified" section can go:

| question | answer |
|---|---|
| does dwc2 enter device mode | |
| what the Host/Slave switch does in software | |
| the real endpoint budget (panel assumes 7) | |
| what gadget mode costs (USB-A / Ethernet / Wi-Fi) | |
| does the panel run with enough privilege to stop NetworkManager | |
| anything that behaved differently from the simulator | |
