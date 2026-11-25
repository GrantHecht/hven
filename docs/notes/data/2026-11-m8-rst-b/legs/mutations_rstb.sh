#!/usr/bin/env bash
# RST-B mutation leg (../DECLARATION-RST-B.md section 4): RF-1c's mutations_rf1c_r3.sh with the table mutate_rstb.py and
# the filters widened by the RST-B suites. Control, then each mutation, rebuilt and run on the pinning tests; the tree is
# restored after each. KILLED iff a pinning test fails; the killers are listed per mutation for the declared-killer check.
# usage: mutations_rstb.sh <out> <tree> <build dir> <ids...>   Run under: ~/.local/bin/box-lane -- bash mutations_rstb.sh ...
set -o pipefail
OUT=${1:?out dir}; TREE=${2:?tree}; B=${3:?build dir}; shift 3; IDS="$*"
L=$(cd "$(dirname "$0")" && pwd)
export MUT_TREE=$TREE MKL_NUM_THREADS=1 CCACHE_DISABLE=1 TMPDIR=$TREE/.scratch/tmp GIT_CEILING_DIRECTORIES=$TREE/.scratch/tmp
mkdir -p "$TMPDIR"; OUT=$(realpath -m "$OUT"); mkdir -p "$OUT"
echo "FOREGROUND_START $(date -Is)"
cd "$TREE" || exit 1
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_DIRTY_BEFORE; exit 2; }
for m in $IDS; do python3 "$L/mutate_rstb.py" "$m" > /dev/null || { echo "PREFLIGHT_DOES_NOT_APPLY $m"; python3 "$L/mutate_rstb.py" restore; exit 2; }; python3 "$L/mutate_rstb.py" restore > /dev/null; done
# PARITY PREFLIGHT (before any build): the runner's ARGUED list must equal the declaration table's argued-survivor
# set, and the table's ids must equal mutate_rstb.py's. I19-2a's runner and table disagreed; this fails closed.
ARGUED=""   # declared argued survivors (DECLARATION-RST-B.md section 4: none)
DECL="$L/../DECLARATION-RST-B.md"
python3 - "$DECL" "$L/mutate_rstb.py" "$ARGUED" <<'PY' || { echo PARITY_FAIL; exit 2; }
import re, subprocess, sys
decl, mut, argued = sys.argv[1], sys.argv[2], set(sys.argv[3].split())
sec = open(decl).read().split("## 4. Mutations and must-kills")[1].split("\n## 5.")[0]
ids, table_argued = [], set()
for line in sec.splitlines():
    m = re.match(r"\|\s*([A-Z]+[0-9]+[a-z]?)\s*\|[^|]*\|\s*([^|]*)\|", line)
    if m and m.group(1) not in ("id",):
        ids.append(m.group(1))
        if m.group(2).strip().startswith("ARGUED"):
            table_argued.add(m.group(1))
script_ids = subprocess.check_output([sys.executable, mut, "list"], text=True).split()
bad = []
if table_argued != argued: bad.append(f"argued: script {sorted(argued)} vs table {sorted(table_argued)}")
if sorted(ids) != sorted(script_ids): bad.append(f"ids: table {sorted(ids)} vs script {sorted(script_ids)}")
if len(set(ids)) != len(ids): bad.append("duplicate table ids")
if bad:
    print("PARITY_FAIL " + "; ".join(bad)); sys.exit(1)
print(f"PARITY ok: {len(ids)} ids, argued {sorted(argued)}")
PY
echo "PREFLIGHT every mutation applies and restores: [$IDS]"
TARGETS="hven_sqp_tests hven_sqp_seam_tests hven_trace_tests"
is_argued() { case " $ARGUED " in *" $1 "*) return 0 ;; esac; return 1; }
run_one() { local id=$1 r=0 r1 r2 r3
  [ "$id" = control ] || python3 "$L/mutate_rstb.py" "$id" > /dev/null || return 9
  [ "$id" = control ] || git diff -- src include > "$OUT/$id.diff"
  timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/$id.build.log" 2>&1 || { echo "$id BUILD_FAIL"; python3 "$L/mutate_rstb.py" restore; return 8; }
  { timeout 1200 "$B/tests/sqp/hven_sqp_tests" --gtest_filter='Rstb*:Rf1*:RestorationBestOrder.*:QpCap*:QpFailureStep.*:SqpSolverQpFailure.*:SqpSolverRestoration*:RestorationResumeReduction*:RestorationEntriesOption.*'; r1=$?
    timeout 1200 "$B/tests/sqp/hven_sqp_seam_tests" --gtest_filter='SqpLadderSeam.*:Rf1*:Rstb*'; r2=$?
    timeout 600 "$B/tests/trace/hven_trace_tests" --gtest_filter='TraceMessagesSqp.*'; r3=$?; } > "$OUT/$id.tests.log" 2>&1
  r=$((r1 + r2 + r3))
  grep -a "^\[  FAILED  \] [A-Za-z]" "$OUT/$id.tests.log" | sed 's/^.*\] //; s/ (.*//' | sort -u > "$OUT/$id.killers.txt"
  [ "$id" = control ] || python3 "$L/mutate_rstb.py" restore > /dev/null
  echo "MUT $id rc=$r killers=$(wc -l < "$OUT/$id.killers.txt")"; sed 's/^/   /' "$OUT/$id.killers.txt"
  case "$id" in control) [ $r -eq 0 ] ;; *) if is_argued "$id"; then [ $r -ne 0 ] && echo "KILLED_ARGUED $id (reported, not a failure)"; true; else [ $r -ne 0 ]; fi ;; esac
}
verdict=MUTATIONS_AS_DECLARED
run_one control || verdict=CONTROL_NOT_CLEAN
for m in $IDS; do run_one $m || verdict="MUTATION_SURVIVED_$m"; done
timeout 2400 ninja -C "$B" -j${BOX_LANE_JOBS:-4} $TARGETS > "$OUT/final.build.log" 2>&1
git diff --quiet --ignore-submodules=all -- src include tests || { echo TREE_NOT_RESTORED; verdict=TREE_NOT_RESTORED; }
echo "VERDICT $verdict"; echo "FOREGROUND_END $(date -Is)"
[ "$verdict" = MUTATIONS_AS_DECLARED ]
