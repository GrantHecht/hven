#!/usr/bin/env bash
# M8 DN-3a (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-nonum-restore/runtime-r2/ and re-pointed in code lines only (worktree agent-acfc8f7d568381148, evidence runtime/ of this directory, arm A build-dn3a/armAbin (b70e45cfb), arm B frozen to build-dn3a/armBbin, perf scratch build-dn3a/runtime-perf); same path depth (five levels); older provenance lines below are historical.
# M8 IPM no-numerics restoration routing r2 (../DECLARATION-r2.txt): the r1 copy of this file re-pointed for the review round (r2 output directories, arm B frozen to build-dn3a/armBbin); older provenance lines below are historical.
# M8 IPM no-numerics restoration routing (../DECLARATION.txt): copied from ../../2026-11-m8-ipm-ladder-status/ (legs/, runtime/) and re-pointed in code lines only (worktree agent-a3d6ec088fa738122, build-dn3a/, this directory, arm A b9626e7e5, replay outputs nnr-*); older provenance lines below are historical.
# M8 IPM ladder status (../DECLARATION.txt): copied from ../../2026-11-m8-nan-gates/ (legs/, runtime-r2/ -> runtime/) and re-pointed in code lines only (worktree ipmlad, build-ipmlad/, this directory, arm A); older provenance lines below are historical.
# M8 NaN-sticky gates r2 (../DECLARATION-r2.txt): copied from ../../2026-11-m8-dn1d-singular/r3/runtime/ and re-pointed (worktree nan, evidence this directory, arm A and arm B trees build-nan/, arm A = origin/m8 9c6059593); older provenance lines below are historical and untouched.
# M8 DN-1d r3 (../../DECLARATION-r3.txt): r2's copy of this file re-pointed (r3/, build-dn1de3/); older provenance lines below are historical.
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../runtime/ and re-pointed (evidence r2/runtime, arm B build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 (2026-11-m8-f1-init; runtime from its runtime-r2, period 400000; census legs from PF-1) and re-pointed: worktree r11, build-r11e, this directory, arm A a8c540cf.
# M8 F1 RUNTIME r2 (../DECLARATION-r2.txt: run 1 VOID on a THROTTLE event; period 200000 -> 400000, evidence directory runtime-r2; nothing else). Was: M8 F1 (copied from PF-1, re-pointed: worktree f1, build-f1, this directory, arm A 2340b95d). Was: M8 PF-1 (PF-HB's instrument, re-pointed) runtime wrapper (the B8 riders' copy of the B2-runtime-r5 instrument; arm A = bdd797e3) (DECLARATION.txt in this directory).
# Run as: flock /tmp/box-build.lock bash run_all.sh <expected-armB-sha256>
# Freezes arm B, checks its sha256 against the declaration's (fail closed), runs the
# fixture parser pre-flight, then the four measured modes (interior, ipm, ssn, walk), each
# analysed as soon as it ends. STOPS at the first mode that breaks counter identity or
# reads outside the band; nothing is re-run.
set -o pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148
EV=$WT/docs/notes/data/2026-11-m8-dn3a/runtime
EXPECT_B=${1:?expected arm B sha256}
if [ -n "$(pgrep -a hven_bench_cell)" ]; then echo "PGREP_BUSY"; pgrep -a hven_bench_cell; exit 3; fi
echo PGREP_EMPTY
echo "FOREGROUND_START $(date -Is)"
cd "$WT" || exit 1
mkdir -p "$WT/build-dn3a/armBbin"
cp -p "$WT/build-dn3a/rel/bench/hven_sqp_corpus" "$WT/build-dn3a/armBbin/hven_sqp_corpus" || exit 1
GOT_B=$(sha256sum "$WT/build-dn3a/armBbin/hven_sqp_corpus" | cut -d' ' -f1)
sha256sum "$WT/build-dn3a/armAbin/hven_sqp_corpus" "$WT/build-dn3a/armBbin/hven_sqp_corpus"
GOT_A=$(sha256sum "$WT/build-dn3a/armAbin/hven_sqp_corpus" | cut -d' ' -f1)
if [ "$GOT_A" != 3e2ae414a7d9341b6887c3c1329eb4af8210062c26a06d92a955bcbc483d4b6c ]; then echo "ARM_A_SHA_MISMATCH got=$GOT_A"; exit 4; fi
if [ "$GOT_B" != "$EXPECT_B" ]; then echo "ARM_B_SHA_MISMATCH got=$GOT_B expected=$EXPECT_B"; exit 4; fi
[ -f "$WT/scripts/compare_replay.py" ] || { echo "COMPARE_NOT_FOUND"; exit 7; }
python3 "$EV/analyze_r11.py" --preflight "$EV/fixture/perf-table-excerpt.txt" "$EV/fixture/expected.txt" || { echo "FIXTURE_PREFLIGHT_FAIL"; exit 7; }
verdict=PASS
for MODE in interior ipm ssn walk; do
  bash "$EV/runtime_leg.sh" "$MODE" "$EV/$MODE" > "$EV/$MODE.log" 2>&1
  rc=$?; echo "LEG mode=$MODE rc=$rc $(date -Is)"
  python3 "$EV/analyze_r11.py" "$EV/$MODE" "$MODE" > "$EV/$MODE/analysis.txt" 2>&1
  arc=$?; echo "ANALYSIS mode=$MODE rc=$arc"; grep -E 'STOP CONDITION|VALIDITY|B/A|MODE ' "$EV/$MODE/analysis.txt"
  if [ $rc -ne 0 ] || [ $arc -ne 0 ]; then verdict="FAIL(mode=$MODE leg_rc=$rc analysis_rc=$arc)"; break; fi
done
echo "VERDICT $verdict"
echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = PASS ]
