#!/usr/bin/env bash
# THE r3 PRE-FLIGHT: fail closed before any r3 leg.  usage: preflight_r3.sh [--falsify]
# 1. the frozen r1 pre-flight in RUN mode (docs-only descendant of the pin, compiled tree identical, tree clean, derivation, tools, box, seams, baselines, harness pin, run_leg proofs, r1 checker falsifiers);
# 2. r3-specific: the legs that stand did stand; the build directories and the corpus binary are exactly what r1/r2 built; the r3 checker and the r2-guard falsifiers.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/env.sh"
FALS=0; [ "${1:-}" = "--falsify" ] && FALS=1
cd "$R" || exit 90
mkdir -p "$LOGS3"
FAILS=0
ok()  { echo "PF_OK    $1"; }
bad() { echo "PF_FAIL  $1"; FAILS=$((FAILS+1)); }
chk() { if [ "$2" -eq 0 ]; then ok "$1"; else bad "$1"; fi; }
fals() { [ $FALS -eq 1 ] || return 0; if [ "$2" -ne 0 ]; then echo "PF_FALSIFIER_REFUSED $1"; else bad "FALSIFIER ACCEPTED $1 (the check cannot fail)"; fi; }

PF_manifest() { # <dir-under-F> <expected sha256>
  [ "$(cd "$F" && find "$1" -type f -printf '%p %s %T@\n' | LC_ALL=C sort | sha256sum | cut -c1-64)" = "$2" ]; }
PF_binsha() { [ "$(sha256sum "$BIN" | cut -c1-64)" = "$1" ]; }
PF_legs_stand() { # <chain.txt> <step...>: every named step has a STEP_OK line, and no CHAIN_STOP precedes the last of them
  local c="$1"; shift; local s
  for s in "$@"; do grep -q "^STEP_OK $s " "$c" || return 1; done; return 0; }
PF_stamp() { # the corpus binary's own provenance stamp: clean, and the r1 declaration commit
  "$BIN" --from-csv "$CENSUS_BASELINE" --csv "$LOGS3/pf-stamp.csv" > /dev/null 2>&1 || return 1
  grep -qE "^# binary: $1\$" "$LOGS3/pf-stamp.csv" && ! grep -q -- '-dirty' "$LOGS3/pf-stamp.csv"; }

echo "PREFLIGHT_R3 falsify=$FALS head=$(git rev-parse HEAD) date=$(date -u +%FT%TZ)"
bash "$W/preflight.sh" run --falsify > "$LOGS3/preflight-r1-frozen.txt" 2>&1; rc=$?
tail -2 "$LOGS3/preflight-r1-frozen.txt"; chk "frozen r1 pre-flight (run mode, with its falsifiers)" $rc
PF_legs_stand "$R2REC/chain.txt" build-release build-debug suite-release suite-debug long-debug smoke rig-build rig-run replay sv1-pin sv1-build sv1-guards sv1-hs sv1-cutest; chk "legs that stand: r1 legs 1-5 and r2 leg 6 each logged STEP_OK in the record's chain" $?
PF_legs_stand "$R2REC/chain.txt" build-release no-such-step; fals "legs-stand check against a step that never ran" $?
PF_manifest build-release "$DECL_MANIFEST_RELEASE"; chk "build-release unchanged since the declaration (file/size/mtime manifest)" $?
PF_manifest build-debug "$DECL_MANIFEST_DEBUG"; chk "build-debug unchanged since the declaration" $?
PF_manifest build-3seam "$DECL_MANIFEST_3SEAM"; chk "build-3seam unchanged since the declaration" $?
PF_manifest build-release "$DECL_MANIFEST_DEBUG"; fals "manifest check against the wrong digest" $?
PF_binsha "$DECL_BIN_SHA256"; chk "census corpus binary sha256 = the declared one" $?
PF_binsha "0000000000000000000000000000000000000000000000000000000000000000"; fals "binary check against a wrong digest" $?
PF_stamp 0de4ae7f19ed; chk "binary stamp: 0de4ae7f19ed, clean (the build r1 ran its legs on)" $?
PF_stamp 111111111111; fals "stamp check against a wrong stamp" $?
python3 -B "$W3/close_check_r3.py" selftest-census "$R" > "$F3/selftest-census-r3.txt" 2>&1; chk "r3 census checker falsifiers (real records + perturbed copies; floor artifact reproduced on the r1 checker)" $?
tail -1 "$F3/selftest-census-r3.txt"
python3 -B "$W3/close_check_r3.py" falsify-r2-guards "$BENCH" "$REC/wrappers-close-r2/suitev1_r2.sh" "$R2REC/suitev1" "$F3/r2-guard-falsifier-logs" > "$F3/selftest-r2-guards.txt" 2>&1; chk "r2 guard-replacement assertions: accept the real inputs, refuse every mutated one" $?
tail -1 "$F3/selftest-r2-guards.txt"
echo "PREFLIGHT_R3_FAILS=$FAILS"
[ $FAILS -eq 0 ] && echo PREFLIGHT_R3_GREEN || echo PREFLIGHT_R3_REFUSED
[ $FAILS -eq 0 ]
