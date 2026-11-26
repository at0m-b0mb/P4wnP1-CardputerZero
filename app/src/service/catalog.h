/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * Reading the service's stored-item and job replies.
 *
 * Pure: strings in, values out, no sockets and no LVGL, so every shape can be
 * pinned on a host against recorded replies. The transport is next door in
 * rpc_client.
 *
 * Shapes mirror what oled/client.go reads, which is the authority:
 *
 *   ListStoredMasterTemplate    -> {"msgArray":["a","b"]}
 *   GetStartupMasterTemplate    -> {"msg":"a"}
 *   HIDGetRunningScriptJobs     -> {"ids":[7,9]}
 *   HIDGetRunningJobState       -> {"source":"// recon\n..."}
 *
 * The ids shape is worth spelling out. Upstream decoded it as
 * {"jobs":[{"id","scriptPath"}]} -- a shape the service has never sent -- so
 * the list came back empty every time and the Jobs screen always said nothing
 * was running while payloads were in fact running. Unmarshalling into tags
 * that match nothing is not an error, it is a zero value, so nothing reported
 * a problem anywhere. Hence Listing below: a read that fails and a read that
 * returned no rows must not arrive at the screen looking alike.
 */

#pragma once

#include <string>
#include <vector>

namespace service {

// An answer, or the absence of one. `ok` false means the service did not
// answer or sent something unreadable; `ok` true with an empty `items` means
// it answered and has nothing stored. The screen says those differently,
// because one is a fault and the other is a fact.
struct Listing {
    bool ok = false;
    std::vector<std::string> items;
};

struct JobIds {
    bool ok = false;
    std::vector<int> ids;
};

// {"msgArray":[...]} -- the reply shape of every ListStored* RPC.
Listing parse_string_array(const std::string& json);

// {"msg":"..."} -- a single stored string, e.g. the startup loadout. ok is
// false when unreadable; ok true with an empty value means none is set, which
// is an ordinary state and not a fault.
Listing parse_msg(const std::string& json);

// {"ids":[...]} -- the running HIDScript jobs. Ids only; the service has no
// RPC that reports what a job was started FROM.
JobIds parse_job_ids(const std::string& json);

// {"source":"..."} -> a short label for one running job.
//
// There is no RPC that says which file a job came from: the job list carries
// ids and the job state carries the script's source text, not its path. So the
// label is the first meaningful line of the source, which for every payload in
// the P4wnP1 tree is its header comment. Costs one call per row, which is why
// only the Jobs screen asks and only for rows it can actually show.
//
// Empty when the reply cannot be read or the source has no usable line -- the
// screen then shows the id alone rather than inventing a name for it.
std::string parse_job_label(const std::string& json, std::size_t max_chars = 34);

// The labelling rule on its own, so it can be tested without a JSON wrapper.
std::string first_meaningful_line(const std::string& source, std::size_t max_chars = 34);

} // namespace service
