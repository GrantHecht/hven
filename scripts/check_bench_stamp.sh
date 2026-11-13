#!/usr/bin/env bash
# M7-16 guard: the bench binaries' `# binary:` provenance stamp is resolved at
# CMake configure time (bench/CMakeLists.txt). A tree configured clean and then
# edited or committed would rebuild with the stale stamp. This compares the
# stamp recorded at configure with a live `git describe` and fails loudly on
# any mismatch.
#
# Usage: check_bench_stamp.sh STAMP_FILE SOURCE_DIR
#   exit 0  the configured stamp equals the live one
#   exit 1  STALE: re-configure (`cmake <build-dir>`) before producing a CSV
#   exit 2  usage / unreadable stamp
set -o pipefail
if [[ $# -ne 2 ]]; then echo "usage: $0 STAMP_FILE SOURCE_DIR" >&2; exit 2; fi
stamp_file="$1"; src="$2"
if [[ ! -r "$stamp_file" ]]; then echo "check_bench_stamp: cannot read $stamp_file" >&2; exit 2; fi
configured="$(head -n1 "$stamp_file")"
if live="$(git -C "$src" describe --always --dirty --abbrev=12 2>/dev/null)" && [[ -n "$live" ]]; then :; else live="unknown"; fi
if [[ "$configured" == "$live" ]]; then
    echo "check_bench_stamp: OK ($configured)"
    exit 0
fi
echo "check_bench_stamp: STALE bench stamp: configured '$configured' but the tree is now '$live'." >&2
echo "  Re-configure the build tree (cmake <build-dir>) and rebuild before producing a measurement CSV." >&2
exit 1
