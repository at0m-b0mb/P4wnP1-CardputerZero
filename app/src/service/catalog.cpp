/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "catalog.h"

#include <nlohmann/json.hpp>

namespace service {
namespace {

// The characters a comment marker is made of, stripped from the front of a
// source line: "// recon", "# recon", "/* recon" all reduce to "recon".
const char* const kLeadTrim = "/*# \t\r";
const char* const kTailTrim = "*/ \t\r";

std::string trim(const std::string& s, const char* lead, const char* tail) {
    const std::size_t first = s.find_first_not_of(lead);
    if (first == std::string::npos) {
        return {};
    }
    const std::size_t last = s.find_last_not_of(tail);
    if (last == std::string::npos || last < first) {
        return {};
    }
    return s.substr(first, last - first + 1);
}

} // namespace

Listing parse_string_array(const std::string& json) {
    Listing out;
    const nlohmann::json doc = nlohmann::json::parse(json, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return out;
    }
    const auto arr = doc.find("msgArray");
    if (arr == doc.end()) {
        // The service answers a list RPC with no stored items by omitting the
        // array entirely rather than sending an empty one. That is an answer,
        // so it counts as ok with nothing in it -- not as a failure.
        out.ok = true;
        return out;
    }
    if (arr->is_null()) {
        out.ok = true;
        return out;
    }
    if (!arr->is_array()) {
        return out;
    }
    for (const auto& entry : *arr) {
        if (entry.is_string()) {
            out.items.push_back(entry.get<std::string>());
        }
    }
    out.ok = true;
    return out;
}

Listing parse_msg(const std::string& json) {
    Listing out;
    const nlohmann::json doc = nlohmann::json::parse(json, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return out;
    }
    const auto msg = doc.find("msg");
    if (msg == doc.end() || msg->is_null()) {
        // Answered, and nothing is set. An ordinary state.
        out.ok = true;
        return out;
    }
    if (!msg->is_string()) {
        return out;
    }
    const std::string value = msg->get<std::string>();
    out.ok = true;
    if (!value.empty()) {
        out.items.push_back(value);
    }
    return out;
}

JobIds parse_job_ids(const std::string& json) {
    JobIds out;
    const nlohmann::json doc = nlohmann::json::parse(json, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return out;
    }
    const auto ids = doc.find("ids");
    if (ids == doc.end() || ids->is_null()) {
        // Nothing running. An answer, and the commonest one.
        out.ok = true;
        return out;
    }
    if (!ids->is_array()) {
        return out;
    }
    for (const auto& entry : *ids) {
        if (entry.is_number_integer()) {
            out.ids.push_back(entry.get<int>());
        }
    }
    out.ok = true;
    return out;
}

std::string first_meaningful_line(const std::string& source, std::size_t max_chars) {
    std::size_t start = 0;
    while (start <= source.size()) {
        std::size_t end = source.find('\n', start);
        if (end == std::string::npos) {
            end = source.size();
        }
        const std::string line = trim(source.substr(start, end - start), kLeadTrim, kTailTrim);
        if (!line.empty()) {
            if (max_chars > 3 && line.size() > max_chars) {
                // Marked as cut rather than silently shortened: a truncated
                // name that looks whole is a name the operator may not
                // recognise as the wrong one.
                //
                // ASCII dots, not U+2026. The bundled face has no ellipsis
                // glyph and draws a tofu box, which is how the em dash got
                // caught in this codebase already.
                return line.substr(0, max_chars - 3) + "...";
            }
            return line;
        }
        if (end == source.size()) {
            break;
        }
        start = end + 1;
    }
    return {};
}

std::string parse_job_label(const std::string& json, std::size_t max_chars) {
    const nlohmann::json doc = nlohmann::json::parse(json, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        return {};
    }
    const auto src = doc.find("source");
    if (src == doc.end() || !src->is_string()) {
        return {};
    }
    return first_meaningful_line(src->get<std::string>(), max_chars);
}

} // namespace service
