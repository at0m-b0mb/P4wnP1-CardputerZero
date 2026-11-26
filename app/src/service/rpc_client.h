/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The thin edge: HTTP to the P4wnP1 service. Everything that interprets what
 * comes back lives in status_parse, which is testable without a socket.
 *
 *   POST {base}/api/v1/rpc/{Method}   Authorization: Bearer <token>
 *
 * The token is the service's own local credential, written to
 * /run/p4wnp1/local.token -- the same mechanism its CLI and OLED client use,
 * so the panel needs no credential of its own and stores none.
 */

#pragma once

#include "catalog.h"
#include "status_parse.h"

#include <string>
#include <vector>

namespace service {

struct DeployOutcome {
    bool ok = false;
    std::string error;       // why not, in words the screen can show
    std::string sent_body;   // what was actually deployed, for the test to inspect
};

struct RunOutcome {
    bool ok = false;
    int job_id = 0;
    std::string error;
    std::string sent_path;  // the absolute path we actually asked for
};

// An action either happened or it did not, and when it did not the operator
// is told in the service's own words rather than ours. We do not know the
// device's reasons better than it does.
struct ActionOutcome {
    bool ok = false;
    std::string error;
};

struct RpcResult {
    bool ok = false;
    long http_status = 0;
    std::string body;
    std::string error;  // transport-level failure, empty when the server answered
};

class RpcClient {
public:
    RpcClient(std::string base_url, std::string token_path);

    // One unary call. A non-2xx answer is still "the server answered": ok is
    // false but http_status says why, which the caller distinguishes from a
    // service that is not running at all.
    RpcResult call(const std::string& method, const std::string& json_body = "null");

    // The five reads the Status screen needs. Each failure leaves its own
    // *_ok flag clear rather than failing the whole snapshot, so a device with
    // no Wi-Fi subsystem still reports its USB state.
    StatusSnapshot fetch_status();

    // True when the service answered the cheapest call we have.
    bool reachable();

    // Deploy a USB composition.
    //
    // Read-modify-write, never build-from-scratch: the gadget message carries
    // vendor and product IDs, the serial and more besides, and a message built
    // only from the toggles would silently reset all of it.
    //
    // And "enabled" is forced true rather than carried over.
    // GetDeployedGadgetSetting reports what is in configfs RIGHT NOW, not the
    // stored intent, so when the gadget is torn down -- which every service
    // start does, and every failed deploy leaves behind -- that read comes
    // back enabled:false. Carrying it faithfully means ticking every function,
    // deploying, being told it worked, and building another disabled gadget:
    // the one screen whose job is to bring USB back could not, and said
    // nothing about why. Learned the hard way upstream; not re-learning it.
    //
    // enabled_keys are the toggle keys to switch on; every other known toggle
    // is switched off, so the deployed composition is exactly what was ticked.
    DeployOutcome deploy_gadget(const std::vector<std::string>& enabled_keys);

    // Stored payloads, by bare name.
    //
    // ListStoredHIDScripts returns bare names ("recon.js") while
    // HIDRunScriptJob demands an ABSOLUTE path and rejects anything else with
    // "HIDScript path rejected: path must be absolute". Feeding one straight
    // into the other is the obvious thing to do and is wrong; run_payload
    // does the joining so no caller has to know.
    std::vector<std::string> list_payloads();

    // Start one. Returns the job id, or 0 with an error set. The call returns
    // as soon as the job starts, so the timeout covers the start and never
    // the script itself.
    RunOutcome run_payload(const std::string& bare_name);

    // Stop everything that is running.
    bool cancel_all_jobs();

    // --- loadouts: P4wnP1 calls them master templates --------------------
    //
    // One stored bundle of USB, network, Bluetooth and reflex settings.
    // Deploying one changes several subsystems at once, which is exactly why
    // the screen confirms before it does.
    Listing list_loadouts();

    // Which loadout the service applies at boot. ok with nothing in it means
    // none is set, which is an ordinary state and not a fault.
    Listing startup_loadout();

    ActionOutcome deploy_loadout(const std::string& name);
    ActionOutcome set_startup_loadout(const std::string& name);
    ActionOutcome delete_loadout(const std::string& name);

    // --- jobs -------------------------------------------------------------
    //
    // Ids only: the service has no RPC that reports what a job was started
    // from. Cheap, so the Status poll can call it every few seconds.
    JobIds running_job_ids();

    // One label, costing one call, so the Jobs screen asks only for the rows
    // it can actually show. Empty when there is nothing usable to show, and
    // the screen then falls back to the id rather than inventing a name.
    std::string job_label(int id);

    // --- the device itself -------------------------------------------------
    //
    // Reboot and shutdown end the session that is running the panel. Both are
    // confirmed on screen before they are called; neither is undoable.
    ActionOutcome reboot();
    ActionOutcome shutdown();

    // Blink the device LED, which is how you find which board on the bench is
    // this one. Harmless, so it is not confirmed.
    ActionOutcome set_led_blink(int count);

    const std::string& base_url() const { return base_url_; }
    void set_timeout_ms(long ms) { timeout_ms_ = ms; }

private:
    std::string bearer_token();

    // The service's own wording for a refusal, falling back to `fallback`
    // when the reply carried none. A transport failure is reported as such,
    // because "no service" and "the service said no" call for different
    // actions by the operator.
    ActionOutcome outcome_of(const RpcResult& result, const char* fallback);

    // POST a {"msg": name} action and report what happened.
    ActionOutcome msg_action(const std::string& method,
                             const std::string& name,
                             const char* fallback);

    std::string base_url_;
    std::string token_path_;
    std::string token_;
    long timeout_ms_ = 2000;
};

} // namespace service
