/*
 * SPDX-FileCopyrightText: 2026 at0m-b0mb
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "network_custody_runtime.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sys/stat.h>

#if defined(__linux__)
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace service::netcustody {
namespace {

constexpr const char* kUnit = "NetworkManager.service";

#if defined(__linux__)
std::string trim(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return {};
    }
    const std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}
#endif

bool contains_ci(const std::string& haystack, const char* needle) {
    const std::string n(needle);
    if (n.empty() || haystack.size() < n.size()) {
        return false;
    }
    for (std::size_t i = 0; i + n.size() <= haystack.size(); ++i) {
        std::size_t j = 0;
        for (; j < n.size(); ++j) {
            const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(haystack[i + j])));
            const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(n[j])));
            if (a != b) {
                break;
            }
        }
        if (j == n.size()) {
            return true;
        }
    }
    return false;
}

// Rejected because it needed privileges, rather than because the service was
// not there. polkit's wording differs from sudo's, and both differ from a
// plain EPERM, so all three are matched.
bool looks_like_permission(const CommandResult& r) {
    return contains_ci(r.output, "permission denied") ||
           contains_ci(r.output, "access denied") ||
           contains_ci(r.output, "interactive authentication required") ||
           contains_ci(r.output, "authentication is required") ||
           contains_ci(r.output, "must be root");
}

#if defined(__linux__)
CommandResult run_real(const std::vector<std::string>& argv) {
    CommandResult out;
    if (argv.empty()) {
        return out;
    }

    int fds[2];
    if (pipe(fds) != 0) {
        return out;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return out;
    }

    if (pid == 0) {
        // Child. Both streams go down the pipe: a failure's reason is usually
        // on stderr, and it is the reason we most want to show the operator.
        close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        close(fds[1]);

        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv) {
            cargv.push_back(const_cast<char*>(a.c_str()));
        }
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);  // execvp only returns on failure
    }

    close(fds[1]);
    std::string collected;
    std::array<char, 512> buf{};
    ssize_t n = 0;
    while ((n = read(fds[0], buf.data(), buf.size())) > 0) {
        collected.append(buf.data(), static_cast<std::size_t>(n));
    }
    close(fds[0]);

    int status = 0;
    waitpid(pid, &status, 0);

    out.output = trim(collected);
    if (WIFEXITED(status)) {
        out.exit_code = WEXITSTATUS(status);
        // 127 is our own marker for "could not exec", which is not the same
        // as the command running and failing.
        out.ran = out.exit_code != 127;
        if (!out.ran) {
            out.output = "command not found: " + argv[0];
        }
    }
    else {
        out.exit_code = -1;
        out.ran = true;
        out.output = out.output.empty() ? "terminated by a signal" : out.output;
    }
    return out;
}
#else
CommandResult run_unsupported(const std::vector<std::string>& argv) {
    CommandResult out;
    out.ran = false;
    out.exit_code = -1;
    out.output = "not a Linux device, so there is no " +
                 (argv.empty() ? std::string("service manager") : argv[0]);
    return out;
}
#endif

bool marker_exists(const std::string& path) {
    struct stat st {};
    return !path.empty() && stat(path.c_str(), &st) == 0;
}

// Best effort mkdir -p for the marker's directory. /run/<something> usually
// does not exist yet on first use.
void ensure_parent(const std::string& path) {
    const std::size_t slash = path.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
        return;
    }
    const std::string dir = path.substr(0, slash);
    struct stat st {};
    if (stat(dir.c_str(), &st) == 0) {
        return;
    }
    mkdir(dir.c_str(), 0755);
}

} // namespace

const char* default_marker_path() {
    return "/run/p4wnp1-panel/networkmanager-borrowed";
}

Effects system_effects() {
    Effects fx;
#if defined(__linux__)
    fx.run = run_real;
#else
    fx.run = run_unsupported;
#endif
    fx.marker_path = default_marker_path();
    return fx;
}

const char* step_name(Step step) {
    switch (step) {
        case Step::WriteMarker:          return "record that networking is borrowed";
        case Step::StopNetworkManager:   return "stop NetworkManager";
        case Step::StartNetworkManager:  return "start NetworkManager";
        case Step::ClearMarker:          return "clear the borrowed record";
        case Step::Nothing:
        default:                         return "nothing";
    }
}

Reading observe(const Effects& fx, bool want_armed) {
    Reading reading;
    reading.observed.want_armed = want_armed;
    reading.observed.marker_present = marker_exists(fx.marker_path);

    // `is-active` exits non-zero when the unit is not running, which is an
    // answer and not an error. A command that could not run at all is a
    // different thing entirely, and is recorded as such.
    const CommandResult r = fx.run({"systemctl", "is-active", "--quiet", kUnit});
    reading.nm_known = r.ran;
    reading.observed.nm_running = r.ran && r.exit_code == 0;
    if (!r.ran) {
        reading.why = r.output.empty() ? "could not ask whether NetworkManager is running"
                                       : r.output;
    }
    return reading;
}

Outcome apply(const Effects& fx, const Plan& plan) {
    Outcome out;
    out.ok = true;

    for (int i = 0; i < plan.count; ++i) {
        const Step step = plan.steps[i];
        if (step == Step::Nothing) {
            continue;
        }

        if (fx.dry_run) {
            out.did.push_back(std::string("would ") + step_name(step));
            continue;
        }

        switch (step) {
            case Step::WriteMarker: {
                ensure_parent(fx.marker_path);
                std::ofstream f(fx.marker_path, std::ios::trunc);
                if (!f) {
                    out.ok = false;
                    out.error = "could not record the borrow at " + fx.marker_path;
                    // Writing the marker is the step that makes stopping safe,
                    // so failing it must stop the whole plan. Carrying on would
                    // stop NetworkManager with nothing on disk saying we did.
                    out.needs_root = errno == EACCES || errno == EPERM;
                    return out;
                }
                f << "networkmanager stopped by the P4wnP1 panel\n";
                out.changed = true;
                out.did.push_back(step_name(step));
                break;
            }
            case Step::ClearMarker: {
                if (marker_exists(fx.marker_path) && std::remove(fx.marker_path.c_str()) != 0) {
                    out.ok = false;
                    out.error = "could not clear the borrow record at " + fx.marker_path;
                    out.needs_root = errno == EACCES || errno == EPERM;
                    return out;
                }
                out.changed = true;
                out.did.push_back(step_name(step));
                break;
            }
            case Step::StopNetworkManager:
            case Step::StartNetworkManager: {
                const char* verb = step == Step::StopNetworkManager ? "stop" : "start";
                const CommandResult r = fx.run({"systemctl", verb, kUnit});
                if (!r.ran) {
                    out.ok = false;
                    out.unsupported = true;
                    out.error = r.output.empty() ? "systemctl is not available here" : r.output;
                    return out;
                }
                if (r.exit_code != 0) {
                    out.ok = false;
                    out.needs_root = looks_like_permission(r);
                    out.error = r.output.empty()
                                    ? std::string("systemctl ") + verb + " failed"
                                    : r.output;
                    return out;
                }
                out.changed = true;
                out.did.push_back(step_name(step));
                break;
            }
            case Step::Nothing:
            default:
                break;
        }
    }

    return out;
}

Outcome reconcile_now(const Effects& fx, bool want_armed) {
    const Reading reading = observe(fx, want_armed);

    // Not knowing is not the same as knowing it is down, and acting on the
    // guess is how the panel would come to record a borrow it never made.
    // Refuse instead, and say why.
    if (!reading.nm_known) {
        Outcome out;
        out.ok = false;
        out.unsupported = true;
        out.error = reading.why;
        return out;
    }

    const Observed& o = reading.observed;
    const Plan plan = reconcile(o);

    if (!plan_is_safe(o, plan)) {
        // Should be unreachable: reconcile() is the only producer of plans and
        // its own suite proves every plan it makes is safe. Kept because the
        // cost of being wrong here is a device with no networking and nothing
        // recording why, and the cost of the check is one function call.
        Outcome out;
        out.ok = false;
        out.error = "refused an unsafe networking plan";
        return out;
    }

    return apply(fx, plan);
}

} // namespace service::netcustody
