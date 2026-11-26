/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "../src/service/catalog.h"

#include <cstdio>
#include <string>

namespace {

using namespace service;

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-62s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

} // namespace

int main() {
    std::printf("Catalog\n\n  listing stored items\n");
    {
        const Listing l = parse_string_array(R"({"msgArray":["office","airgap","demo"]})");
        check(l.ok, "a list is read");
        check(l.items.size() == 3, "...with all three entries");
        check(l.items[0] == "office" && l.items[2] == "demo", "...in the order sent");
    }
    {
        // THE distinction this type exists for. Both of these put no rows on
        // screen, and the screen must not word them the same way.
        const Listing answered = parse_string_array(R"({"msgArray":[]})");
        const Listing silent   = parse_string_array("");
        check(answered.ok && answered.items.empty(), "an empty list is an ANSWER, not a failure");
        check(!silent.ok, "an unreadable reply is a failure, not an empty list");
        check(answered.ok != silent.ok, "...and the two are distinguishable");
    }
    {
        // The service omits the array entirely when it has nothing stored.
        const Listing l = parse_string_array(R"({})");
        check(l.ok && l.items.empty(), "a missing array means none stored, not unreadable");
        const Listing n = parse_string_array(R"({"msgArray":null})");
        check(n.ok && n.items.empty(), "an explicit null means the same");
    }
    {
        const Listing wrong = parse_string_array(R"({"msgArray":"office"})");
        check(!wrong.ok, "a string where an array belongs is unreadable");
        const Listing arr = parse_string_array(R"([1,2,3])");
        check(!arr.ok, "a bare array is not the shape either");
    }
    {
        const Listing mixed = parse_string_array(R"({"msgArray":["ok",7,null,"fine"]})");
        check(mixed.ok && mixed.items.size() == 2, "non-strings are dropped, the rest kept");
    }

    std::printf("\n  the startup loadout\n");
    {
        const Listing l = parse_msg(R"({"msg":"airgap"})");
        check(l.ok && l.items.size() == 1 && l.items[0] == "airgap", "a set value is read");
    }
    {
        const Listing empty = parse_msg(R"({"msg":""})");
        check(empty.ok && empty.items.empty(), "an empty string means none is set");
        const Listing absent = parse_msg(R"({})");
        check(absent.ok && absent.items.empty(), "...so does a missing field");
        const Listing bad = parse_msg("not json");
        check(!bad.ok, "...but garbage is still a failure");
    }

    std::printf("\n  running jobs\n");
    {
        const JobIds j = parse_job_ids(R"({"ids":[7,9,12]})");
        check(j.ok && j.ids.size() == 3, "three ids are read");
        check(j.ids[0] == 7 && j.ids[2] == 12, "...with their values");
    }
    {
        // The upstream bug, pinned. The old client decoded
        // {"jobs":[{"id":...}]} and so always saw nothing running while
        // payloads were running. Reading THAT shape must not succeed.
        const JobIds wrong = parse_job_ids(R"({"jobs":[{"id":7,"scriptPath":"/x.js"}]})");
        check(wrong.ok && wrong.ids.empty(),
              "the shape the service never sends yields no ids");
        const JobIds right = parse_job_ids(R"({"ids":[7]})");
        check(right.ids.size() == 1, "...while the real shape does");
    }
    {
        const JobIds none = parse_job_ids(R"({"ids":[]})");
        check(none.ok && none.ids.empty(), "nothing running is an answer");
        const JobIds dead = parse_job_ids("");
        check(!dead.ok, "...and no answer is not 'nothing running'");
    }

    std::printf("\n  labelling a job from its source\n");
    {
        check(first_meaningful_line("// recon the host\nGUI r\n") == "recon the host",
              "a // comment becomes the label");
        check(first_meaningful_line("# exfil wifi keys\n") == "exfil wifi keys",
              "a # comment does too");
        check(first_meaningful_line("/* staged dropper */\n") == "staged dropper",
              "a /* */ comment is stripped both ends");
        check(first_meaningful_line("\n\n\n   \n// after the blanks\n") == "after the blanks",
              "leading blank lines are skipped");
        check(first_meaningful_line("layout('us');\n") == "layout('us');",
              "a script with no comment labels by its first line of code");
        check(first_meaningful_line("").empty(), "an empty source has no label");
        check(first_meaningful_line("///////\n").empty(),
              "a line of nothing but comment markers is not a label");
    }
    {
        const std::string label = first_meaningful_line(
            "// a header comment far longer than any row on this screen can show", 20);
        check(label.size() == 20, "a long label is cut to the limit");
        check(label.substr(label.size() - 3) == "...", "...and marked as cut");
        check(label.find("\xE2\x80\xA6") == std::string::npos,
              "...with ASCII dots, since the bundled face has no ellipsis glyph");
    }
    {
        check(parse_job_label(R"({"source":"// quiet recon\nGUI r"})") == "quiet recon",
              "the label is read out of the job state reply");
        check(parse_job_label(R"({})").empty(), "a reply with no source has no label");
        check(parse_job_label("nonsense").empty(), "neither does an unreadable one");
    }

    std::printf("\n%s\n", failures == 0 ? "ALL CATALOG CHECKS PASS" : "CATALOG FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
