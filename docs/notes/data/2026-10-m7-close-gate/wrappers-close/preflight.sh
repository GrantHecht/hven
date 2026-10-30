#!/usr/bin/env bash
# THE PRE-FLIGHT: fail closed before the first leg.  usage: preflight.sh <declare|run> [--falsify]
#   declare : the tree may carry changes ONLY under the record directory (the declaration being written);
#   run     : the tree must be entirely clean (the declaration is committed).
# Every check is a function PF_<name> taking its input as an argument.  --falsify runs each check a second time on a
# deliberately wrong input and requires it to REFUSE; a check that accepts its own falsifier fails the pre-flight.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$HERE/env.sh"
MODE="${1:-run}"; FALS=0; [ "${2:-}" = "--falsify" ] && FALS=1
cd "$R" || exit 90
FAILS=0
ok()  { echo "PF_OK    $1"; }
bad() { echo "PF_FAIL  $1"; FAILS=$((FAILS+1)); }
chk() { # name, rc-of-check
  if [ "$2" -eq 0 ]; then ok "$1"; else bad "$1"; fi; }
fals() { # name, rc-of-check-on-wrong-input (must be nonzero)
  [ $FALS -eq 1 ] || return 0
  if [ "$2" -ne 0 ]; then echo "PF_FALSIFIER_REFUSED $1"; else bad "FALSIFIER ACCEPTED $1 (the check cannot fail)"; fi; }

# ---- checks ---------------------------------------------------------------------------------------------------------
PF_head() { # <base-ref>: HEAD == base + docs-only record-dir commits; the compiled tree is the base's
  local base="$1"
  git merge-base --is-ancestor "$base" HEAD || return 1
  [ -z "$(git diff --stat "$base" HEAD -- src include tests bench cmake CMakeLists.txt scripts CMakePresets.json dep .gitmodules)" ] || return 2
  local outside; outside="$(git diff --name-only "$base" HEAD | grep -v '^docs/notes/data/2026-10-m7-close-gate/' || true)"
  [ -z "$outside" ] || return 3
  return 0; }
PF_clean() { # <mode>
  local dirty; dirty="$(git status --porcelain)"
  if [ "$1" = run ]; then [ -z "$dirty" ]; else [ -z "$(echo "$dirty" | grep -v ' docs/notes/data/2026-10-m7-close-gate/\|^?? docs/notes/data/2026-10-m7-close-gate/' || true)" ]; fi; }
PF_derivation() { # <dir>: the committed derivation equals a fresh one
  local t; t="$(mktemp -d "$F/pf-derive.XXXX")"; python3 -B "$W/close_check.py" derive "$t/d" "$R" > /dev/null 2>&1 || { rm -rf "$t"; return 1; }
  diff -r "$1" "$t/d" > /dev/null; local rc=$?; rm -rf "$t"; return $rc; }
PF_tool() { command -v "$1" > /dev/null 2>&1; }
PF_box() { # <process-name>: nothing of that name runs
  ! pgrep -x "$1" > /dev/null; }
PF_seams() { # <tycho-tree-pin> <sqp-commit-pin>
  [ "$(git -C /home/ghecht/Projects/tycho rev-parse HEAD:psiopt/include/tycho/detail/solvers/linear)" = "$1" ] || return 1
  [ -z "$(git -C /home/ghecht/Projects/tycho status --porcelain -- psiopt/include/tycho/detail/solvers/linear)" ] || return 2
  [ "$(git -C /home/ghecht/Projects/tycho_sqp rev-parse 'phase-7-close^{commit}')" = "$2" ] || return 3
  git -C /home/ghecht/Projects/tycho_sqp diff --quiet phase-7-close -- include/tycho_sqp || return 4
  return 0; }
PF_replay_inputs() { # <dir> <expected-cells>: three 75-column head CSVs of N cells
  local d="$1" n="$2" e
  for e in walk ssn ipm; do
    [ -f "$d/head-$e.csv" ] || return 1
    [ "$(grep -vc '^#\|^cell_id' "$d/head-$e.csv")" = "$n" ] || return 2
    [ "$(grep -m1 '^cell_id' "$d/head-$e.csv" | awk -F, '{print NF}')" = 76 ] || return 3
  done; return 0; }
PF_cells() { [ "$(grep -v '^#' "$1" | tr -d '\n' | tr ',' '\n' | grep -c .)" = "$2" ]; }
PF_census_baseline() { # <baseline csv> <m6 census csv>: 57 rows, and the M6 census equals it under the byte-exact comparator
  [ "$(grep -vc '^#\|^cell_id' "$1")" = 57 ] || return 1
  python3 -B "$R/scripts/census_compare.py" "$1" "$2" | tail -1 | grep -q '^57 baseline cells, 57 fresh cells, 0 mismatches' || return 2
  return 0; }
PF_census_lineage() { # <expected-last-dir>: no census record newer (by name) than the expected one
  local last; last="$(find "$R/docs/notes/data" -name 'walk_census*.csv' -not -path '*/2026-10-m7-close-gate/*' | sed 's|.*/data/||;s|/.*||' | sort | tail -1)"
  [ "$last" = "$1" ]; }
PF_path() { [ -e "$1" ]; }
PF_count() { # <glob-dir> <n>
  [ "$(ls "$1" 2>/dev/null | wc -l)" = "$2" ]; }
PF_bench() { # <dir> <expected pin sha> <expected harness head prefix>
  [ "$(sed -n 's/^set(HVEN_PIN_SHA "\(.*\)")$/\1/p' "$1/cmake/hven_pin.cmake")" = "$2" ] || return 1
  [ "$(git -C "$1" rev-parse HEAD | cut -c1-7)" = "$3" ] || return 2
  [ -z "$(git -C "$1" status --porcelain)" ] || return 3
  git -C "$R" cat-file -e "$2^{commit}" || return 4
  return 0; }
PF_submodules() { [ -f "$1/dep/eigen/Eigen/Core" ] && [ -f "$1/dep/fmt/include/fmt/core.h" ]; }
PF_runleg() { # protocol self-proofs of run_leg.sh (executed for real, tiny commands); prints the evidence
  local L; L="$(mktemp -d "$F/pf-runleg.XXXX")"
  bash "$W/run_leg.sh" "$L/ok.log" bash -o pipefail -c 'true' > /dev/null 2>&1 || return 1
  grep -q 'PGREP_EMPTY (inside the lock)' "$L/ok.log" && grep -q FOREGROUND_START "$L/ok.log" && grep -q FOREGROUND_END "$L/ok.log" || return 2
  bash "$W/run_leg.sh" "$L/fail.log" bash -o pipefail -c 'false | cat; exit 7' > /dev/null 2>&1; [ $? -eq 7 ] || return 3
  grep -q 'LEG_RC=7' "$L/fail.log" || return 4
  bash "$W/run_leg.sh" "$L/pipe.log" bash -o pipefail -c 'false | cat' > /dev/null 2>&1; [ $? -ne 0 ] || return 5
  bash "$W/run_leg.sh" "$L/ref.log" bash -c 'true' > /dev/null 2>&1; [ $? -eq 2 ] || return 6   # an un-flagged nested shell is refused
  # a foreign process inside the lock refuses the leg (decoy started and reaped by THIS script only)
  local dd="$L/decoy"; mkdir -p "$dd"; ln -s "$(command -v sleep)" "$dd/hven_bench_cell"; "$dd/hven_bench_cell" 30 & local dp=$!
  PGREP_PATTERN='hven_bench_cell' bash "$W/run_leg.sh" "$L/foreign.log" bash -o pipefail -c 'true' > /dev/null 2>&1; local frc=$?
  kill "$dp" 2> /dev/null; wait "$dp" 2> /dev/null
  [ $frc -eq 75 ] || return 7
  grep -q PGREP_NONEMPTY "$L/foreign.log" || return 8
  rm -rf "$L"; return 0; }

# ---- run them ---------------------------------------------------------------------------------------------------------
echo "PREFLIGHT mode=$MODE falsify=$FALS head=$(git rev-parse HEAD) date=$(date -u +%FT%TZ)"
mkdir -p "$F"
PF_head "$PIN"; chk "head: docs-only descendant of the pin $PIN12, compiled tree identical" $?
PF_head "aaa95642"; fals "head against aaa95642 (a tests-bearing range)" $?
PF_clean "$MODE"; chk "tree clean ($MODE)" $?
if [ $FALS -eq 1 ]; then ( touch "$R/stray-file-preflight-falsifier"; PF_clean run; r=$?; rm -f "$R/stray-file-preflight-falsifier"; exit $r ); fals "clean against a stray untracked file" $?; fi
PF_derivation "$REC/derivation"; chk "derivation reproducible from git + the frozen records" $?
if [ $FALS -eq 1 ]; then t="$(mktemp -d "$F/pf-der-f.XXXX")"; cp -r "$REC/derivation" "$t/d"; sed -i '1d' "$t/d/expected-skipped-debug.txt"; PF_derivation "$t/d"; fals "derivation with a skipped-set line deleted" $?; rm -rf "$t"; fi
for t in clang++ cmake ninja ctest flock taskset python3 pgrep git; do PF_tool $t; chk "tool $t" $?; done
PF_tool no-such-tool-xyz; fals "tool check against a missing tool" $?
PF_box hven_bench_cell; chk "box: no hven_bench_cell process" $?
if [ $FALS -eq 1 ]; then dd="$(mktemp -d "$F/pf-decoy.XXXX")"; ln -s "$(command -v sleep)" "$dd/hven_bench_cell"; "$dd/hven_bench_cell" 20 & dp=$!; PF_box hven_bench_cell; r=$?; kill $dp 2>/dev/null; wait $dp 2>/dev/null; rm -rf "$dd"; fals "box check against a live decoy named hven_bench_cell" $r; fi
PF_seams bfb7d30c5cc821069b7af3f9efa077f29474de5e 4faa1df116da53c9dc68f36635c118f52d39d2b9; chk "rig seams at their pins (tycho consumed tree, tycho_sqp phase-7-close)" $?
PF_seams 0000000000000000000000000000000000000000 4faa1df116da53c9dc68f36635c118f52d39d2b9; fals "seams against a wrong tycho pin" $?
PF_replay_inputs "$WX2/gates/replay" 27; chk "replay baseline: the WX2 record, 3 engines x 27 cells x 75 compared columns (76 header fields)" $?
PF_replay_inputs "$WX2/gates/replay" 26; fals "replay baseline against a wrong cell count" $?
PF_replay_inputs "$M6/replay" 27; chk "replay baseline: the M6 pin, 3 x 27 x 76 fields" $?
PF_cells "$CELLS_FILE" 27; chk "replay cell list = 27 cells" $?
PF_cells "$CELLS_FILE" 26; fals "cell list against 26" $?
PF_census_baseline "$CENSUS_BASELINE" "$M6/census/walk_census.csv"; chk "census baseline: 57 rows; the M6 close census equals it (57/57/0, byte-exact comparator)" $?
PF_census_baseline "$CENSUS_BASELINE" "$WX2/gates/replay/head-walk.csv"; fals "census baseline against a non-census csv" $?
PF_census_lineage 2026-09-m6-close-gate; chk "census lineage: the M6 close census is the latest census record on the tree" $?
PF_census_lineage 2026-08-21-m4-task2b-census; fals "lineage against an older expected record" $?
PF_path /opt/intel/oneapi/mkl/latest/lib/libmkl_core.a; chk "MKL present" $?
PF_path "$HOME/Software/snopt7/lib/libsnopt7_cpp.so"; chk "SNOPT install (lane invoked only through the harness)" $?
PF_path "$HOME/Software/ipopt-pardisomkl/install"; chk "IPOPT install (lane invoked only through the harness)" $?
PF_path "$HOME/Software/nonexistent-solver"; fals "install check against a missing path" $?
PF_count "$BENCH/sets/cutest/sif" 35; chk "CUTEst: 35 fetched instances" $?
PF_count "$BENCH/.scratch/h3/decoded" 35; chk "CUTEst: 35 decoded instances" $?
PF_count "$BENCH/sets/cutest/sif" 36; fals "instance count against 36" $?
PF_bench "$BENCH" "$PIN" 451b51b; chk "hven-bench: pin file = $PIN12, harness HEAD 451b51b, tree clean" $?
PF_bench "$BENCH" "$PIN" 0000000; fals "hven-bench against a wrong harness head" $?
PF_submodules "$R"; chk "dep/eigen and dep/fmt populated" $?
PF_submodules /tmp; fals "submodule check against an empty dir" $?
PF_runleg; chk "run_leg.sh protocol proofs (fold, pipefail refusal, in-lock foreign-process refusal)" $?
python3 -B "$W/close_check.py" selftest "$R" "$REC/derivation" > "$F/selftest-close-check.txt" 2>&1; chk "close_check.py falsifiers (suite/configure/replay/census/rig assertions)" $?
tail -1 "$F/selftest-close-check.txt"
python3 -B "$W/check_suitev1.py" --selftest "$BENCH" x > "$F/selftest-suitev1.txt" 2>&1; chk "check_suitev1.py falsifiers" $?
tail -1 "$F/selftest-suitev1.txt"
echo "PREFLIGHT_FAILS=$FAILS"
[ $FAILS -eq 0 ] && echo PREFLIGHT_GREEN || echo PREFLIGHT_REFUSED
[ $FAILS -eq 0 ]
