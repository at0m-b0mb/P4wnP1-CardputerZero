/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * The half of network custody that touches the machine, driven against a fake
 * runner and a marker in a temp directory. Nothing here needs root, systemd,
 * or a NetworkManager -- which is the point: the paths that matter are the
 * failure paths, and those are the ones you cannot arrange on a real device
 * on demand.
 */

#include "../src/service/network_custody_runtime.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

using namespace service::netcustody;

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("    %-62s %s\n", what, ok ? "pass" : "FAIL");
    if (!ok) {
        ++failures;
    }
}

bool exists(const std::string& p) {
    struct stat st {};
    return stat(p.c_str(), &st) == 0;
}

std::string temp_marker() {
    static int n = 0;
    std::string dir = "/tmp/p4wnp1-custody-test-" + std::to_string(getpid()) +
                      "-" + std::to_string(n++);
    mkdir(dir.c_str(), 0755);
    return dir + "/marker";
}

// A runner that answers from a script and records what it was asked.
struct FakeSystemctl {
    bool nm_running = true;
    bool stop_fails_permission = false;
    bool start_fails = false;
    bool missing = false;            // systemctl not on this machine
    std::vector<std::string> calls;

    Runner runner() {
        return [this](const std::vector<std::string>& argv) {
            CommandResult r;
            std::string joined;
            for (const auto& a : argv) {
                joined += (joined.empty() ? "" : " ") + a;
            }
            calls.push_back(joined);

            if (missing) {
                r.ran = false;
                r.output = "command not found: systemctl";
                return r;
            }
            r.ran = true;

            if (argv.size() >= 2 && argv[1] == "is-active") {
                r.exit_code = nm_running ? 0 : 3;
                return r;
            }
            if (argv.size() >= 2 && argv[1] == "stop") {
                if (stop_fails_permission) {
                    r.exit_code = 1;
                    r.output = "Interactive authentication required.";
                    return r;
                }
                nm_running = false;
                r.exit_code = 0;
                return r;
            }
            if (argv.size() >= 2 && argv[1] == "start") {
                if (start_fails) {
                    r.exit_code = 1;
                    r.output = "Job for NetworkManager.service failed.";
                    return r;
                }
                nm_running = true;
                r.exit_code = 0;
                return r;
            }
            r.exit_code = 0;
            return r;
        };
    }
};

} // namespace

int main() {
    std::printf("Custody runtime\n\n  borrowing\n");
    {
        FakeSystemctl sys;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();

        const Outcome out = reconcile_now(fx, /*want_armed=*/true);
        check(out.ok, "arming succeeds");
        check(out.changed, "...and says something changed");
        check(exists(fx.marker_path), "the borrow is recorded on disk");
        check(!sys.nm_running, "NetworkManager is stopped");
        check(out.did.size() == 2, "two steps were taken");
        // The ordering IS the safety property.
        check(!out.did.empty() && out.did[0] == std::string("record that networking is borrowed"),
              "the record is written BEFORE the stop");
        std::remove(fx.marker_path.c_str());
    }
    {
        // Arming twice must not write a second marker or stop a stopped
        // service: reconcile is called on every arm, not only the first.
        FakeSystemctl sys;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();
        reconcile_now(fx, true);
        sys.calls.clear();
        const Outcome again = reconcile_now(fx, true);
        check(again.ok, "arming again succeeds");
        check(!again.changed, "...and changes nothing the second time");
        std::remove(fx.marker_path.c_str());
    }

    std::printf("\n  giving it back\n");
    {
        FakeSystemctl sys;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();
        reconcile_now(fx, true);           // borrow
        const Outcome out = reconcile_now(fx, false);  // disarm
        check(out.ok, "disarming succeeds");
        check(sys.nm_running, "NetworkManager is running again");
        check(!exists(fx.marker_path), "the record is cleared");
        check(out.did.size() == 2 && out.did[0] == std::string("start NetworkManager"),
              "it is STARTED before the record is cleared");
    }
    {
        // The whole reason the marker exists: the process died while armed.
        // A fresh run, told nothing is armed, must still give networking back.
        FakeSystemctl sys;
        sys.nm_running = false;            // as the crash left it
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();
        { std::FILE* f = std::fopen(fx.marker_path.c_str(), "w"); std::fputs("x\n", f); std::fclose(f); }

        const Outcome out = reconcile_now(fx, /*want_armed=*/false);
        check(out.ok, "a crash recovery succeeds");
        check(sys.nm_running, "...and NetworkManager is back up");
        check(!exists(fx.marker_path), "...with the record cleared");
    }
    {
        // NetworkManager down, no marker: somebody else stopped it on purpose
        // and it is not ours to switch back on.
        FakeSystemctl sys;
        sys.nm_running = false;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();

        const Outcome out = reconcile_now(fx, false);
        check(out.ok, "a service we never borrowed is left alone");
        check(!out.changed, "...nothing happens at all");
        check(!sys.nm_running, "...and it stays down");
    }

    std::printf("\n  when it is not allowed to\n");
    {
        FakeSystemctl sys;
        sys.stop_fails_permission = true;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();

        const Outcome out = reconcile_now(fx, true);
        check(!out.ok, "a refused stop is reported as a failure");
        check(out.needs_root, "...named as a permissions problem, not a mystery");
        check(out.error.find("authentication") != std::string::npos,
              "...in the system's own words");
        // It recorded a borrow it did not manage to take. That is the SAFE
        // direction: the next reconcile starts a service that is already up,
        // which is harmless, and clears the record.
        check(exists(fx.marker_path), "the record was written first, as designed");

        const Outcome recover = reconcile_now(fx, false);
        check(recover.ok, "...and the next disarm cleans it up");
        check(!exists(fx.marker_path), "...leaving no record behind");
    }
    {
        // A start that fails must NOT clear the record: that is precisely the
        // state where the next run has to know it still owes a restore.
        FakeSystemctl sys;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();
        reconcile_now(fx, true);
        sys.start_fails = true;

        const Outcome out = reconcile_now(fx, false);
        check(!out.ok, "a failed restore is reported as a failure");
        check(exists(fx.marker_path),
              "...and the record SURVIVES, so the debt is not forgotten");

        sys.start_fails = false;
        const Outcome retry = reconcile_now(fx, false);
        check(retry.ok && !exists(fx.marker_path), "...and a retry finishes the job");
    }
    {
        // No systemd at all, which is every non-Linux host this builds on.
        FakeSystemctl sys;
        sys.missing = true;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();

        const Outcome out = reconcile_now(fx, true);
        check(!out.ok, "a machine with no systemctl fails honestly");
        check(out.unsupported, "...flagged as unsupported rather than refused");
        std::remove(fx.marker_path.c_str());
    }

    std::printf("\n  dry run\n");
    {
        // What the first run on real hardware should do: say what it would do.
        FakeSystemctl sys;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();
        fx.dry_run = true;

        const Outcome out = reconcile_now(fx, true);
        check(out.ok, "a dry run succeeds");
        check(!out.changed, "...and changes nothing");
        check(!exists(fx.marker_path), "...writes no marker");
        check(sys.nm_running, "...and leaves NetworkManager alone");
        check(out.did.size() == 2, "...while still reporting both steps");
        check(!out.did.empty() && out.did[0].rfind("would ", 0) == 0,
              "...each marked as hypothetical");
    }

    std::printf("\n  observing\n");
    {
        // A command that could not run must not read as "not running": that
        // would let a plan skip the stop and believe it had borrowed
        // networking it never took.
        FakeSystemctl sys;
        sys.missing = true;
        Effects fx;
        fx.run = sys.runner();
        fx.marker_path = temp_marker();

        const Reading reading = observe(fx, true);
        check(!reading.nm_known, "an unrunnable probe is recorded as NOT KNOWN");
        check(!reading.why.empty(), "...with the reason it could not be read");

        // And the thing that makes it matter: were this collapsed into
        // "not running", reconcile would plan a marker and no stop, and the
        // panel would record a borrow it never made.
        Observed collapsed = reading.observed;
        collapsed.nm_running = false;
        const Plan wrong = reconcile(collapsed);
        check(wrong.contains(Step::WriteMarker) && !wrong.contains(Step::StopNetworkManager),
              "...which collapsed would plan a marker with no stop");
    }

    std::printf("\n%s\n",
                failures == 0 ? "ALL CUSTODY RUNTIME CHECKS PASS" : "CUSTODY RUNTIME FAILURES PRESENT");
    return failures == 0 ? 0 : 1;
}
