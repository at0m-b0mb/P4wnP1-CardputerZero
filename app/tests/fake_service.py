#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 at0m-b0mb
# SPDX-License-Identifier: GPL-3.0-only
#
# A stand-in for the P4wnP1 service, answering the five reads the Status screen
# makes with the shapes oled/client.go expects. It exists so the panel's client
# can be exercised without a device, and so the request side is pinned too: it
# rejects anything that is not a POST to /api/v1/rpc/<Method> carrying the
# bearer token, and records what it was asked.

import json
import sys
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer

TOKEN = "test-token-not-a-real-credential"

REPLIES = {
    # Carries more than the toggles -- vendor and product ids, a serial, a
    # name -- precisely so the test can prove a deploy preserves them instead
    # of rebuilding the message from the checkboxes.
    #
    # And "enabled" is False here on purpose: that is what configfs reports
    # once the gadget is torn down, and a client that carries it faithfully
    # deploys another disabled gadget. The test asserts we force it true.
    "GetDeployedGadgetSetting": {
        "enabled": False,
        "vid": "0x1d6b",
        "pid": "0x1337",
        "manufacturer": "MaMe82",
        "product": "P4wnP1 by MaMe82",
        "serial": "deadbeef1337",
        "use_HID_KEYBOARD": True,
        "use_HID_MOUSE": False,
        "use_HID_RAW": False,
        "use_RNDIS": True,
        "use_CDC_ECM": False,
        "use_SERIAL": False,
        "use_UMS": False,
    },
    "GetAllDeployedEthernetInterfaceSettings": {
        "list": [
            {"name": "usbeth", "ipAddress4": "172.16.0.1", "mode": "server"},
            {"name": "wlan0", "ipAddress4": "172.24.0.1", "mode": "ap"},
        ]
    },
    "GetWiFiState": {"mode": "AP", "ssid": "P4wnP1"},
    "GetDeployedTriggerActionSet": {
        "TriggerActions": [{"isActive": True}, {"isActive": True}, {"isActive": False}]
    },
    "HIDGetRunningScriptJobs": {"ids": [7, 9]},
    # Loadouts are P4wnP1's "master templates": one stored bundle of USB,
    # network, Bluetooth and reflex settings.
    "ListStoredMasterTemplate": {"msgArray": ["office-rig", "airgap", "demo-bench"]},
    "GetStartupMasterTemplate": {"msg": "airgap"},
    "DeployStoredMasterTemplate": {},
    "SetStartupMasterTemplate": {},
    "DeleteStoredMasterTemplate": {},
    "Reboot": {},
    "Shutdown": {},
    "SetLEDSettings": {},
    # Bare names, exactly as the real ListStoredHIDScripts returns them. The
    # run below refuses anything that is not an absolute path, which is what
    # the device does and what makes the trap reproducible here.
    "ListStoredHIDScripts": {"msgArray": [
        "hello.js", "linux_enum.js", "mac_prompt.js", "win_recon.js", "exfil_wifi.js",
    ]},
    "HIDCancelAllScriptJobs": {},
}

seen = []
deployed = []          # every gadget message the client sent us
ran = []               # every scriptPath the client asked us to run
acted = []             # every {msg} action: loadout deploy/startup/delete
refuse_deploy = [None] # set to a message to make the next deploy fail
refuse_next = {}       # method -> message, to make one call fail

# What each running job's source looks like. The service has no RPC saying
# which FILE a job came from, so a label can only come from the source text --
# job 9 deliberately has no comment header, to exercise the fallback.
JOB_SOURCES = {
    7: "// quiet recon of the host\nlayout('us');\nGUI r\n",
    9: "layout('us');\nSTRING whoami\n",
}

MSG_ACTIONS = (
    "DeployStoredMasterTemplate",
    "SetStartupMasterTemplate",
    "DeleteStoredMasterTemplate",
)


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args):
        pass  # the test prints its own report

    def _send(self, code, payload):
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        # The real service has a discovery endpoint; anything else is a 404,
        # and the panel must never need one.
        if self.path == "/api/v1/rpc":
            self._send(200, {"methods": sorted(REPLIES)})
        elif self.path == "/__seen":
            self._send(200, {"seen": seen})
        elif self.path == "/__deployed":
            self._send(200, {"deployed": deployed})
        elif self.path == "/__ran":
            self._send(200, {"ran": ran})
        elif self.path == "/__acted":
            self._send(200, {"acted": acted})
        elif self.path.startswith("/__refuse_method/"):
            rest = self.path[len("/__refuse_method/"):]
            method, _, message = rest.partition("/")
            refuse_next[method] = message.replace("%20", " ") or "refused"
            self._send(200, {"ok": True})
        elif self.path.startswith("/__refuse/"):
            refuse_deploy[0] = self.path[len("/__refuse/"):].replace("%20", " ")
            self._send(200, {"ok": True})
        else:
            self._send(404, {"error": "not found"})

    def do_POST(self):
        prefix = "/api/v1/rpc/"
        if not self.path.startswith(prefix):
            self._send(400, {"error": "expected /api/v1/rpc/<MethodName>"})
            return

        auth = self.headers.get("Authorization", "")
        if auth != f"Bearer {TOKEN}":
            self._send(401, {"error": "unauthorized"})
            return

        method = self.path[len(prefix):]
        length = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(length)
        seen.append(method)

        if method in refuse_next:
            self._send(400, {"error": refuse_next.pop(method)})
            return

        if method == "HIDGetRunningJobState":
            try:
                payload = json.loads(raw.decode() or "null")
            except ValueError:
                payload = None
            job_id = (payload or {}).get("id")
            self._send(200, {"source": JOB_SOURCES.get(job_id, "")})
            return

        if method in MSG_ACTIONS:
            try:
                payload = json.loads(raw.decode() or "null")
            except ValueError:
                payload = None
            acted.append({"method": method, "msg": (payload or {}).get("msg")})
            self._send(200, {})
            return

        if method == "HIDRunScriptJob":
            try:
                payload = json.loads(raw.decode() or "null")
            except ValueError:
                payload = None
            path = (payload or {}).get("scriptPath", "")
            ran.append(path)
            # The device checks this and says exactly this.
            if not path.startswith("/"):
                self._send(400, {"error": "HIDScript path rejected: path must be absolute"})
                return
            self._send(200, {"id": 42})
            return

        if method == "DeployGadgetSetting":
            try:
                deployed.append(json.loads(raw.decode() or "null"))
            except ValueError:
                deployed.append({"__unparseable__": raw.decode(errors="replace")})
            if refuse_deploy[0]:
                message, refuse_deploy[0] = refuse_deploy[0], None
                self._send(400, {"error": message})
                return
            self._send(200, {})
            return

        if method not in REPLIES:
            self._send(404, {"error": f"unknown method {method}"})
            return
        self._send(200, REPLIES[method])


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8099
    server = HTTPServer(("127.0.0.1", port), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    print(f"fake P4wnP1 service on 127.0.0.1:{port}", flush=True)
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
