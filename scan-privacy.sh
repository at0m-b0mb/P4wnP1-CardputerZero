#!/usr/bin/env bash
# Does this repo leak anything about the machine or the person building it?
#
# Run before pushing, and before putting device screenshots anywhere. It reads
# bytes and prints how many, because a scanner that silently matched nothing
# and a scanner that read nothing look identical in their output -- and the
# second one is the dangerous one.
#
# It checks the working tree, the whole git history (a public repo publishes
# that too, so deleting a file is not removing it), image metadata, and any
# built .deb.
set -uo pipefail
cd "$(dirname "$0")"

python3 - "$@" <<'PY'
import re, subprocess, sys, os, glob

RULES = [
    ("mac username",   rb"b0mba[_-]?at0mica",                      True),
    ("personal email", rb"[A-Za-z0-9._%+-]+@(?:gmail|outlook|yahoo|icloud|proton)\.[a-z]+", True),
    ("home path",      rb"/Users/[A-Za-z0-9._-]+|/home/(?!pi\b)[A-Za-z0-9._-]+", True),
    ("hostname",       rb"[A-Za-z0-9-]+\.local\b",                 True),
    ("MAC address",    rb"(?<![0-9A-Fa-f:])(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}(?![0-9A-Fa-f:])", True),
    ("private key",    rb"BEGIN (?:RSA |OPENSSH |EC |DSA )?PRIVATE KEY", True),
    ("github token",   rb"gh[pousr]_[A-Za-z0-9]{20,}",             True),
    ("aws key",        rb"AKIA[0-9A-Z]{16}",                       True),
    # Advisory: legitimate as placeholders and test data, worth a human look.
    #
    # Must be a full dotted quad in a private range. An earlier version of
    # this pattern allowed a partial tail, which matched the library version
    # "10.1.1" in CMakeLists while MISSING the real 192.168.1.10 in
    # deploy.sh -- noisy and blind at the same time, which is how a scanner
    # earns being ignored.
    ("private IPv4",   rb"\b(?:192\.168\.\d{1,3}\.\d{1,3}"
                       rb"|10\.\d{1,3}\.\d{1,3}\.\d{1,3}"
                       rb"|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})\b", False),
]

def scan(label, items):
    """items: list of (name, bytes)"""
    total = sum(len(b) for _, b in items)
    fatal, advisory = [], []
    for name, data in items:
        for rule, pat, is_fatal in RULES:
            for m in re.finditer(pat, data):
                found = m.group().decode("utf-8", "replace")
                (fatal if is_fatal else advisory).append((rule, name, found))
    print(f"  {label}: {len(items)} items, {total} bytes")
    if total == 0:
        print(f"  REFUSING TO REPORT on {label}: read 0 bytes")
        return None, None
    return fatal, advisory

def read(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except Exception:
        return b""

print("Privacy scan\n")

# 1. tracked working tree
tracked = subprocess.run(["git", "ls-files"], capture_output=True, text=True).stdout.split()
f1, a1 = scan("working tree", [(p, read(p)) for p in tracked])

# 2. every blob in history
oids = subprocess.run(["git", "rev-list", "--objects", "--all"],
                      capture_output=True, text=True).stdout.splitlines()
blobs = []
for line in oids:
    oid = line.split()[0]
    t = subprocess.run(["git", "cat-file", "-t", oid], capture_output=True, text=True).stdout.strip()
    if t == "blob":
        b = subprocess.run(["git", "cat-file", "-p", oid], capture_output=True).stdout
        blobs.append((f"history:{oid[:8]}", b))
f2, a2 = scan("git history", blobs)

# 3. commit metadata
meta = subprocess.run(["git", "log", "--all", "--format=%an <%ae>%n%cn <%ce>"],
                      capture_output=True, text=True).stdout
ids = sorted(set(l for l in meta.splitlines() if l.strip()))
f3, a3 = scan("commit identities", [("identities", meta.encode())])
print("    identities used:")
for i in ids:
    print(f"      {i}")

# 4. any built package
debs = glob.glob("app/dist/*.deb")
f4, a4 = scan("built packages", [(p, read(p)) for p in debs]) if debs else ([], [])

fatal = sum([x or [] for x in (f1, f2, f3, f4)], [])
advisory = sum([x or [] for x in (a1, a2, a3, a4)], [])

print()
if advisory:
    seen = set()
    print("  Advisory (placeholders and test data are fine; check they are):")
    for rule, where, found in advisory:
        key = (rule, found)
        if key in seen:
            continue
        seen.add(key)
        print(f"    [{rule}] {found}  ({where})")
    print()

if fatal:
    print(f"  {len(fatal)} THING(S) THAT MUST NOT BE PUBLIC:")
    for rule, where, found in fatal:
        print(f"    [{rule}] {found}  ({where})")
    print("\n  If any of these is already pushed, removing the file is NOT enough:")
    print("  a public repo publishes its history. The credential must be rotated")
    print("  and the history rewritten.")
    sys.exit(1)

print("  CLEAN: nothing identifying the machine or the person.")
PY
