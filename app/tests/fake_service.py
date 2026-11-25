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
    "GetDeployedGadgetSetting": {
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
    "HIDGetRunningScriptJobs": {"ids": [7]},
}

seen = []


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
        self.rfile.read(length)
        seen.append(method)

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
