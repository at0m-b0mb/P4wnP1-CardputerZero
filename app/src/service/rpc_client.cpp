/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "rpc_client.h"

#include <curl/curl.h>

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <utility>

namespace service {
namespace {

std::size_t collect(void* data, std::size_t size, std::size_t count, void* user) {
    auto* out = static_cast<std::string*>(user);
    const std::size_t total = size * count;
    out->append(static_cast<char*>(data), total);
    return total;
}

std::string trimmed(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

RpcClient::RpcClient(std::string base_url, std::string token_path)
    : base_url_(std::move(base_url)), token_path_(std::move(token_path)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

std::string RpcClient::bearer_token() {
    if (!token_.empty()) {
        return token_;
    }
    std::ifstream file(token_path_);
    if (!file) {
        // Not an error worth reporting on its own: the service may simply not
        // be running yet, and the call that follows will say so properly.
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    token_ = trimmed(buffer.str());
    return token_;
}

RpcResult RpcClient::call(const std::string& method, const std::string& json_body) {
    RpcResult result;

    CURL* curl = curl_easy_init();
    if (!curl) {
        result.error = "curl unavailable";
        return result;
    }

    const std::string url = base_url_ + "/api/v1/rpc/" + method;
    const std::string token = bearer_token();

    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (!token.empty()) {
        const std::string auth = "Authorization: Bearer " + token;
        headers = curl_slist_append(headers, auth.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(json_body.size()));
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, collect);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result.body);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeout_ms_);
    // The service is on this device. Following a redirect off it would mean
    // sending the bearer token somewhere we did not intend.
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);

    const CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.http_status);
        result.ok = result.http_status >= 200 && result.http_status < 300;
        if (!result.ok && result.http_status == 401) {
            // The cached token went stale: drop it so the next call re-reads.
            token_.clear();
        }
    }
    else {
        result.error = curl_easy_strerror(code);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return result;
}

DeployOutcome RpcClient::deploy_gadget(const std::vector<std::string>& enabled_keys) {
    DeployOutcome outcome;

    // Read first. The gadget message is not ours to reconstruct.
    const RpcResult current = call("GetDeployedGadgetSetting");
    if (!current.ok) {
        outcome.error = current.error.empty() ? "the service refused to report the gadget"
                                              : "no service: " + current.error;
        return outcome;
    }

    nlohmann::json gs = nlohmann::json::parse(current.body, nullptr, false);
    if (gs.is_discarded() || !gs.is_object()) {
        outcome.error = "the service sent a gadget setting we cannot read";
        return outcome;
    }

    // Every known toggle is written explicitly, so the deployed composition is
    // exactly what was ticked rather than what was ticked plus whatever was
    // already on.
    for (const auto& [key, label] : usb_toggle_table()) {
        (void)label;
        gs[key] = false;
    }
    for (const auto& key : enabled_keys) {
        gs[key] = true;
    }

    // See the header: this is forced, not carried.
    gs["enabled"] = true;

    outcome.sent_body = gs.dump();
    const RpcResult deployed = call("DeployGadgetSetting", outcome.sent_body);
    if (!deployed.ok) {
        if (!deployed.error.empty()) {
            outcome.error = "no service: " + deployed.error;
        }
        else if (!deployed.body.empty()) {
            // The service explains refusals in words, including the endpoint
            // overflow. Show its words rather than inventing our own.
            const nlohmann::json err = nlohmann::json::parse(deployed.body, nullptr, false);
            outcome.error = (!err.is_discarded() && err.is_object() && err.contains("error"))
                                ? err["error"].get<std::string>()
                                : "the service refused the deploy";
        }
        else {
            outcome.error = "the service refused the deploy";
        }
        return outcome;
    }

    outcome.ok = true;
    return outcome;
}

// Where the service keeps stored payloads. It has to be spelled out because
// the list and run RPCs disagree about what a payload is called.
constexpr const char* kHidScriptDir = "/usr/local/P4wnP1/HIDScripts";

std::vector<std::string> RpcClient::list_payloads() {
    std::vector<std::string> names;
    const RpcResult r = call("ListStoredHIDScripts");
    if (!r.ok) {
        return names;
    }
    const nlohmann::json doc = nlohmann::json::parse(r.body, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return names;
    }
    const auto arr = doc.find("msgArray");
    if (arr == doc.end() || !arr->is_array()) {
        return names;
    }
    for (const auto& entry : *arr) {
        if (entry.is_string()) {
            names.push_back(entry.get<std::string>());
        }
    }
    return names;
}

RunOutcome RpcClient::run_payload(const std::string& bare_name) {
    RunOutcome outcome;
    if (bare_name.empty()) {
        outcome.error = "no payload selected";
        return outcome;
    }

    // The join the service insists on. A bare name here is refused.
    outcome.sent_path = bare_name.front() == '/'
                            ? bare_name
                            : std::string(kHidScriptDir) + "/" + bare_name;

    nlohmann::json body;
    body["scriptPath"] = outcome.sent_path;
    body["timeoutSeconds"] = 0;

    const RpcResult r = call("HIDRunScriptJob", body.dump());
    if (!r.ok) {
        if (!r.error.empty()) {
            outcome.error = "no service: " + r.error;
        }
        else {
            const nlohmann::json err = nlohmann::json::parse(r.body, nullptr, false);
            outcome.error = (!err.is_discarded() && err.is_object() && err.contains("error"))
                                ? err["error"].get<std::string>()
                                : "the service refused to run it";
        }
        return outcome;
    }

    const nlohmann::json doc = nlohmann::json::parse(r.body, nullptr, false);
    if (!doc.is_discarded() && doc.is_object()) {
        outcome.job_id = doc.value("id", 0);
    }
    outcome.ok = true;
    return outcome;
}

bool RpcClient::cancel_all_jobs() {
    return call("HIDCancelAllScriptJobs").ok;
}

bool RpcClient::reachable() {
    return call("GetDeployedGadgetSetting").ok;
}

StatusSnapshot RpcClient::fetch_status() {
    StatusSnapshot snapshot;

    // Each read stands alone. A device whose Wi-Fi subsystem is unavailable --
    // which P4wnP1 reports on some hardware -- should still show its USB and
    // network state rather than a blank screen.
    if (const RpcResult r = call("GetDeployedGadgetSetting"); r.ok) {
        parse_gadget_settings(r.body, snapshot);
    }
    if (const RpcResult r = call("GetAllDeployedEthernetInterfaceSettings"); r.ok) {
        parse_ethernet_interfaces(r.body, snapshot);
    }
    if (const RpcResult r = call("GetWiFiState"); r.ok) {
        parse_wifi_state(r.body, snapshot);
    }
    if (const RpcResult r = call("GetDeployedTriggerActionSet"); r.ok) {
        parse_trigger_actions(r.body, snapshot);
    }
    if (const RpcResult r = call("HIDGetRunningScriptJobs"); r.ok) {
        parse_running_jobs(r.body, snapshot);
    }

    return snapshot;
}

// ---------------------------------------------------------------------------
// Shared refusal handling
// ---------------------------------------------------------------------------

ActionOutcome RpcClient::outcome_of(const RpcResult& result, const char* fallback) {
    ActionOutcome out;
    if (result.ok) {
        out.ok = true;
        return out;
    }
    if (!result.error.empty()) {
        // Nothing answered at all. Said plainly, because "no service" and
        // "the service said no" are different problems with different fixes.
        out.error = "no service: " + result.error;
        return out;
    }
    const nlohmann::json err = nlohmann::json::parse(result.body, nullptr, false);
    if (!err.is_discarded() && err.is_object()) {
        const auto field = err.find("error");
        if (field != err.end() && field->is_string() && !field->get<std::string>().empty()) {
            out.error = field->get<std::string>();
            return out;
        }
    }
    out.error = fallback;
    return out;
}

ActionOutcome RpcClient::msg_action(const std::string& method,
                                    const std::string& name,
                                    const char* fallback) {
    if (name.empty()) {
        ActionOutcome out;
        out.error = "nothing selected";
        return out;
    }
    nlohmann::json body;
    body["msg"] = name;
    return outcome_of(call(method, body.dump()), fallback);
}

// ---------------------------------------------------------------------------
// Loadouts
// ---------------------------------------------------------------------------

Listing RpcClient::list_loadouts() {
    const RpcResult r = call("ListStoredMasterTemplate");
    if (!r.ok) {
        return {};  // ok stays false: no answer is not an empty shelf
    }
    return parse_string_array(r.body);
}

Listing RpcClient::startup_loadout() {
    const RpcResult r = call("GetStartupMasterTemplate");
    if (!r.ok) {
        return {};
    }
    return parse_msg(r.body);
}

ActionOutcome RpcClient::deploy_loadout(const std::string& name) {
    return msg_action("DeployStoredMasterTemplate", name, "the service refused the loadout");
}

ActionOutcome RpcClient::set_startup_loadout(const std::string& name) {
    return msg_action("SetStartupMasterTemplate", name, "the service would not set it");
}

ActionOutcome RpcClient::delete_loadout(const std::string& name) {
    return msg_action("DeleteStoredMasterTemplate", name, "the service would not delete it");
}

// ---------------------------------------------------------------------------
// Jobs
// ---------------------------------------------------------------------------

JobIds RpcClient::running_job_ids() {
    const RpcResult r = call("HIDGetRunningScriptJobs");
    if (!r.ok) {
        return {};
    }
    return parse_job_ids(r.body);
}

std::string RpcClient::job_label(int id) {
    nlohmann::json body;
    body["id"] = id;
    const RpcResult r = call("HIDGetRunningJobState", body.dump());
    if (!r.ok) {
        return {};
    }
    return parse_job_label(r.body);
}

// ---------------------------------------------------------------------------
// The device itself
// ---------------------------------------------------------------------------

ActionOutcome RpcClient::reboot() {
    return outcome_of(call("Reboot"), "the service would not reboot");
}

ActionOutcome RpcClient::shutdown() {
    return outcome_of(call("Shutdown"), "the service would not shut down");
}

ActionOutcome RpcClient::set_led_blink(int count) {
    nlohmann::json body;
    body["blink_count"] = count;
    return outcome_of(call("SetLEDSettings", body.dump()), "the service would not set the LED");
}

} // namespace service
