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

#include "status_parse.h"

#include <string>

namespace service {

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

    const std::string& base_url() const { return base_url_; }
    void set_timeout_ms(long ms) { timeout_ms_ = ms; }

private:
    std::string bearer_token();

    std::string base_url_;
    std::string token_path_;
    std::string token_;
    long timeout_ms_ = 2000;
};

} // namespace service
