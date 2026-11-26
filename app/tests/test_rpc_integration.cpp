/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Drives the real client over a real socket against tests/fake_service.py.
 * The parse tests prove we read the shapes correctly; this proves we ask for
 * them correctly -- right method, right path, right bearer token -- and that
 * the two halves meet.
 *
 * argv[1] base url, argv[2] token file, argv[3] a token file holding the wrong
 * token so the unauthorised path is exercised too.
 */

#include "../src/service/rpc_client.h"

#include <nlohmann/json.hpp>

#include <cstdio>
#include <string>
#include <vector>
#include <cstdlib>

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-62s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <base-url> <good-token-file> <bad-token-file>\n", argv[0]);
        return 2;
    }
    const std::string base = argv[1];
    const std::string good = argv[2];
    const std::string bad = argv[3];

    std::printf("RPC client against the fake service\n\n  reachability\n");
    {
        service::RpcClient client(base, good);
        check(client.reachable(), "the service answers with a valid token");
    }
    {
        service::RpcClient client(base, bad);
        const service::RpcResult r = client.call("GetDeployedGadgetSetting");
        check(!r.ok, "a wrong token is refused");
        check(r.http_status == 401, "...with 401, so it is distinguishable from being down");
        check(r.error.empty(), "...and is not reported as a transport failure");
    }
    {
        // Nothing is listening on this port. A service that is down and a
        // service that says no are different states and the screen says so.
        service::RpcClient client("http://127.0.0.1:1", good);
        client.set_timeout_ms(1500);
        const service::RpcResult r = client.call("GetDeployedGadgetSetting");
        check(!r.ok, "a service that is not running fails");
        check(!r.error.empty(), "...as a transport error, not an HTTP status");
        check(r.http_status == 0, "...with no status, because nothing answered");
    }

    std::printf("\n  a full status read\n");
    {
        service::RpcClient client(base, good);
        const service::StatusSnapshot s = client.fetch_status();

        check(s.usb_ok, "USB answered");
        check(s.usb_composed, "...and reports a composed gadget");
        check(service::usb_summary(s) == "Keyboard, RNDIS net", "...listing keyboard and RNDIS");

        check(s.net_ok && s.interfaces.size() == 2, "two interfaces came back");
        check(service::interface_ip(s, "usbeth") == "172.16.0.1", "usbeth has its address");
        check(service::interface_ip(s, "wlan0") == "172.24.0.1", "wlan0 has its address");

        check(s.wifi_ok && s.wifi == "AP P4wnP1", "wifi state is read");
        check(s.reflexes_ok && s.reflexes == 3 && s.reflexes_armed == 2, "three reflexes, two armed");
        check(s.jobs_ok && s.jobs_running == 2, "two jobs running");
    }

    std::printf("\n  an unknown method\n");
    {
        service::RpcClient client(base, good);
        const service::RpcResult r = client.call("NoSuchMethod");
        check(!r.ok && r.http_status == 404, "is a 404, not a crash");
    }

    std::printf("\n  deploying a composition\n");
    {
        service::RpcClient client(base, good);
        const service::DeployOutcome out =
            client.deploy_gadget({"use_HID_KEYBOARD", "use_UMS"});
        check(out.ok, "the deploy is accepted");

        const nlohmann::json sent = nlohmann::json::parse(out.sent_body, nullptr, false);
        check(!sent.is_discarded() && sent.is_object(), "...and what we sent is readable");

        // Read-modify-write: fields we never touched must survive.
        check(sent.value("vid", "") == "0x1d6b", "vendor id is preserved, not rebuilt");
        check(sent.value("serial", "") == "deadbeef1337", "serial is preserved");
        check(sent.value("product", "") == "P4wnP1 by MaMe82", "product name is preserved");

        // The one the upstream comment was written in blood about.
        check(sent.value("enabled", false), "enabled is FORCED true, though the read said false");

        // Exactly what was ticked, nothing carried over.
        check(sent.value("use_HID_KEYBOARD", false), "a ticked function is on");
        check(sent.value("use_UMS", false), "the other ticked function is on");
        check(!sent.value("use_RNDIS", true),
              "a function that was on before but not ticked is explicitly off");
        check(!sent.value("use_HID_MOUSE", true), "an untouched function stays off");
    }

    std::printf("\n  a deploy the service refuses\n");
    {
        // Ask the fake service to refuse the next one the way the real service
        // refuses an over-budget composition.
        const std::string cmd = "curl -s -o /dev/null '" + base +
                          "/__refuse/Gadget%20Settings%20consume%208%20out%20of%207%20available%20USB%20Endpoints'";
        if (std::system(cmd.c_str()) != 0) {
            check(false, "could not arm the refusal");
        }

        service::RpcClient client(base, good);
        const service::DeployOutcome out = client.deploy_gadget({"use_HID_KEYBOARD"});
        check(!out.ok, "a refused deploy is reported as failed");
        check(out.error.find("8 out of 7") != std::string::npos,
              "...in the service's own words, not ours");
    }

    std::printf("\n  payloads\n");
    {
        service::RpcClient client(base, good);
        const std::vector<std::string> names = client.list_payloads();
        check(names.size() == 5, "five stored payloads are listed");
        check(!names.empty() && names[0] == "hello.js", "...by bare name, as the service sends them");
    }
    {
        service::RpcClient client(base, good);
        const service::RunOutcome out = client.run_payload("win_recon.js");
        check(out.ok, "a payload starts");
        check(out.job_id == 42, "...and its job id comes back");
        // The trap: a bare name is refused by the device. We must have joined.
        check(out.sent_path == "/usr/local/P4wnP1/HIDScripts/win_recon.js",
              "...having been sent as an ABSOLUTE path, not the bare name");
    }
    {
        // Prove the fake service would have refused a bare name, so the check
        // above is testing something real rather than agreeing with itself.
        service::RpcClient client(base, good);
        const service::RpcResult r =
            client.call("HIDRunScriptJob", R"({"scriptPath":"win_recon.js","timeoutSeconds":0})");
        check(!r.ok && r.http_status == 400, "a bare name really is refused");
        check(r.body.find("must be absolute") != std::string::npos,
              "...with the device's own wording");
    }
    {
        service::RpcClient client(base, good);
        check(client.cancel_all_jobs(), "running jobs can be cancelled");
    }

    std::printf("\n  loadouts\n");
    {
        service::RpcClient client(base, good);
        const service::Listing l = client.list_loadouts();
        check(l.ok, "the loadout shelf answers");
        check(l.items.size() == 3, "...with three stored loadouts");
        check(!l.items.empty() && l.items[0] == "office-rig", "...named as the service names them");
    }
    {
        service::RpcClient client(base, good);
        const service::Listing s0 = client.startup_loadout();
        check(s0.ok && s0.items.size() == 1 && s0.items[0] == "airgap",
              "the startup loadout is read");
    }
    {
        service::RpcClient client(base, good);
        const service::ActionOutcome out = client.deploy_loadout("office-rig");
        check(out.ok, "a loadout deploys");

        // Prove the NAME travelled, not just that a 200 came back. A deploy
        // that silently applied the wrong bundle would otherwise pass.
        const std::string cmd = "curl -s '" + base + "/__acted' -o /tmp/.acted.json";
        check(std::system(cmd.c_str()) == 0, "...and the service recorded the call");
        std::FILE* f = std::fopen("/tmp/.acted.json", "rb");
        std::string body;
        if (f) {
            char buf[4096];
            const std::size_t n = std::fread(buf, 1, sizeof(buf), f);
            body.assign(buf, n);
            std::fclose(f);
        }
        check(body.find("DeployStoredMasterTemplate") != std::string::npos,
              "...by the right method");
        check(body.find("office-rig") != std::string::npos,
              "...carrying the name that was selected");
    }
    {
        service::RpcClient client(base, good);
        check(client.set_startup_loadout("demo-bench").ok, "the startup loadout can be set");
        check(client.delete_loadout("demo-bench").ok, "a loadout can be deleted");
    }
    {
        // An empty selection must never reach the wire: {"msg":""} would ask
        // the service to deploy a loadout with no name.
        service::RpcClient client(base, good);
        const service::ActionOutcome out = client.deploy_loadout("");
        check(!out.ok, "deploying nothing is refused locally");
        check(out.error == "nothing selected", "...and said plainly");
    }
    {
        service::RpcClient client(base, good);
        const std::string cmd = "curl -s -o /dev/null '" + base +
                                "/__refuse_method/DeployStoredMasterTemplate/no%20such%20template'";
        check(std::system(cmd.c_str()) == 0, "a refusal can be armed");
        const service::ActionOutcome out = client.deploy_loadout("ghost");
        check(!out.ok, "a refused loadout is reported as failed");
        check(out.error == "no such template", "...in the service's own words, not ours");
    }

    std::printf("\n  jobs\n");
    {
        service::RpcClient client(base, good);
        const service::JobIds j = client.running_job_ids();
        check(j.ok, "the running-job list answers");
        check(j.ids.size() == 2, "...with both running jobs");
        check(!j.ids.empty() && j.ids[0] == 7, "...by id");
    }
    {
        service::RpcClient client(base, good);
        check(client.job_label(7) == "quiet recon of the host",
              "a job is labelled from its source comment");
        // Job 9's source has no comment header, so the label falls back to
        // its first line of code rather than to a name we made up.
        check(client.job_label(9) == "layout('us');",
              "a job with no comment labels by its first line");
        check(client.job_label(999).empty(),
              "an unknown job has no label at all");
    }

    std::printf("\n  the device itself\n");
    {
        service::RpcClient client(base, good);
        check(client.set_led_blink(3).ok, "the LED can be blinked");
    }
    {
        service::RpcClient client(base, good);
        const std::string cmd = "curl -s -o /dev/null '" + base +
                                "/__refuse_method/Reboot/not%20now';";
        check(std::system(cmd.c_str()) == 0, "a refusal can be armed");
        const service::ActionOutcome out = client.reboot();
        check(!out.ok && out.error == "not now", "a refused reboot says why");
    }
    {
        // Nothing answering at all must read differently from a refusal.
        service::RpcClient dead("http://127.0.0.1:1", good);
        dead.set_timeout_ms(800);
        const service::ActionOutcome out = dead.reboot();
        check(!out.ok, "a reboot with no service fails");
        check(out.error.rfind("no service", 0) == 0,
              "...and says NO SERVICE, not that the device refused");
    }

    std::printf("\n%s\n", failures == 0 ? "ALL RPC CHECKS PASS" : "RPC FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
