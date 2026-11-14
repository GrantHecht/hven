#!/usr/bin/env bash
# Self-test of check_bench_stamp.sh on a throwaway git repo: a matching stamp
# passes; an uncommitted edit and a new commit after "configure" each fail
# loudly; a re-stamp passes again; the no-git and bad-input cases behave.
# Usage: check_bench_stamp_selftest.sh PATH_TO_check_bench_stamp.sh
set -uo pipefail
chk="$1"; bad=0
t="$(mktemp -d)"; trap 'rm -rf "$t"' EXIT
g() { git -C "$t/r" "$@"; }
expect() { local want="$1" what="$2"; shift 2; "$@" >/dev/null 2>&1; local rc=$?
    if [[ $rc -eq $want ]]; then echo "PASS $what"; else echo "FAIL $what (rc $rc, want $want)"; bad=1; fi; }
mkdir "$t/r"
g init -q
g config user.email t@t
g config user.name t
echo a > "$t/r/f"
g add f
g commit -q -m one
stamp="$t/stamp"
g describe --always --dirty --abbrev=12 > "$stamp"
expect 0 "clean tree, stamp matches" bash "$chk" "$stamp" "$t/r"
echo b >> "$t/r/f"
expect 1 "uncommitted edit after configure -> STALE" bash "$chk" "$stamp" "$t/r"
g commit -q -am two
expect 1 "new commit after configure -> STALE" bash "$chk" "$stamp" "$t/r"
g describe --always --dirty --abbrev=12 > "$stamp"
expect 0 "re-stamped stamp matches again" bash "$chk" "$stamp" "$t/r"
mkdir "$t/nogit"
echo unknown > "$t/unk"
expect 0 "no git and stamp 'unknown' -> OK" bash "$chk" "$t/unk" "$t/nogit"
expect 1 "no git but a real stamp -> STALE" bash "$chk" "$stamp" "$t/nogit"
expect 2 "missing stamp file -> usage error" bash "$chk" "$t/none" "$t/r"
exit $bad
