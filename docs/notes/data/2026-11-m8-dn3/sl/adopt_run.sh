#!/usr/bin/env bash
# M8 DN-3 (../DECLARATION-r1.md): the enumeration leg (E) and the adoption legs (S, P, T, G). Serial: one cell process
# at a time, alone on the box under /tmp/box-build.lock, THREADS=1 (MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, the lane on
# core 2; perf record's reader on core 4) unless stated; CAP 300 s per process (cell.sh's own watchdog). The two arms are
# labels of ONE lane binary: `default` (kNever, arm N) and `auto-default` (kAuto, arm A).
#   E  every roster cell, `run` N then A (STAT binary)                              -> EV/enum/cells
#      then enum_analyze.py -> EV/enum/enum-analysis.txt and EV/enum/candidates.tsv
#   S  every roster cell: one untimed warm-up per arm, then perf stat rounds in the order N A A N N A (STAT binary)
#                                                                                   -> EV/stat/<tag>.<arm>.r<k>.*
#   P  every CANDIDATE cell (EV/enum/candidates.tsv), per arm: perf record on the CFI RECORD binary, period and
#      repeats from that arm's S median (DECLARATION-r1 A2), then adopt_attr.py      -> EV/record/<tag>.<arm>.{runs,attr.json}
#   T  the four median cells at the engines' default threads (MKL/OMP variables unset, unpinned, common.threads = 0 by
#      the `t0-` label prefix), per arm: 3 perf stat rounds (N A A N N A) and the P-leg record    -> EV/threads/
#   G  the four median cells, per arm: gdb entry counts of MKL_Read_ThreadENV_Count_Cores and __kmp_serial_initialize
#      on the RECORD binary                                                         -> EV/gdb/
# usage: adopt_run.sh <leg> <evidence-dir> <stat-binary sha256> <record-binary sha256>
# Run as: flock /tmp/box-build.lock bash adopt_run.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/agent-a4a349c1df0402cb8
D=$WT/docs/notes/data/2026-11-m8-dn3/sl
LEG=${1:?leg}
EV=${2:?evidence dir}
WANT_S=${3:?stat binary sha256}
WANT_R=${4:?record binary sha256}
STATBIN=$WT/build-dn3/adoptbin/hven_bench_cell
RECBIN=$WT/build-dn3/adoptbin/hven_bench_cell.cfi
SCRATCH=$WT/build-dn3/adopt-scratch
ROSTER=$WT/docs/notes/data/2026-11-m8-dn0/raw/roster.tsv
MEDIANS="hven-ipm hs026|hven-ipm HS67|hven-sqp hs043|hven-sqp HS84"
CAP=300
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START ${LEG} $(date -Is) max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate)"
[[ "$(sha256sum "$STATBIN" | cut -d' ' -f1)" == "$WANT_S" ]] || { echo STAT_BINARY_SHA_MISMATCH; exit 4; }
[[ "$(sha256sum "$RECBIN" | cut -d' ' -f1)" == "$WANT_R" ]] || { echo RECORD_BINARY_SHA_MISMATCH; exit 4; }
[[ $(wc -l < "$ROSTER") -eq 238 ]] || { echo ROSTER_COUNT; exit 4; }
mkdir -p "$EV" "$SCRATCH"
if [[ -n "${DEV_ROSTER:-}" ]]; then   # scratch dev probes only (disclosed); never set on a declared leg
  echo "DEV_ROSTER=${DEV_ROSTER} (dev probe, not a declared leg)"
  cp "$DEV_ROSTER" "$EV/roster.tsv"
elif [[ ! -f "$EV/roster.tsv" ]]; then
  python3 "$D/roster.py" /home/ghecht/Projects/hven-bench-dn1 "$STATBIN" > "$EV/roster.check.tmp" || { echo "ROSTER_REFUSED (S-H)"; exit 3; }
  cmp -s "$EV/roster.check.tmp" "$ROSTER" || { echo ROSTER_DIFFERS_FROM_DN0; exit 3; }
  mv "$EV/roster.check.tmp" "$EV/roster.tsv"
fi
arm_label() { [[ "$1" == N ]] && echo default || echo auto-default; }
cell() { # <mode> <bin> <lane> <prob> <label> <threads> <prefix> [period]
  CELL_BIN=$2 bash "$D/cell.sh" "$1" "$3" "$4" "$5" "$6" "$7" "$CAP" ${8:-} > /dev/null
}
stat_median() { # <dir> <tag> <arm>
  python3 - "$1" "$2" "$3" <<'PY'
import sys, os, statistics
d, tag, arm = sys.argv[1:4]
v = []
for r in (1, 2, 3):
    f = os.path.join(d, f"{tag}.{arm}.r{r}.stat")
    if os.path.exists(f):
        for line in open(f):
            parts = line.split(",")
            if len(parts) > 2 and parts[2].startswith("instructions"):
                v.append(int(parts[0]))
print(int(statistics.median(v)) if len(v) == 3 else 0)
PY
}
stat_rounds() { # <threads> <label-prefix> <outdir> <lane> <prob> <tag>
  local th=$1 pre=$2 out=$3 lane=$4 prob=$5 tag=$6 k a
  if [[ ! -f "$out/$tag.A.r3.result" ]]; then
    for a in N A; do cell stat "$STATBIN" "$lane" "$prob" "$pre$(arm_label $a)" "$th" "$SCRATCH/warmup"; done
    rm -f "$SCRATCH"/warmup.*
    for ra in 1:N 1:A 2:A 2:N 3:N 3:A; do
      k=${ra%%:*}; a=${ra#*:}
      cell stat "$STATBIN" "$lane" "$prob" "$pre$(arm_label $a)" "$th" "$out/$tag.$a.r$k"
      grep -q ',instructions' "$out/$tag.$a.r$k.stat" || { echo "STAT_MISSING_INSTRUCTIONS $tag $a r$k"; rc_leg=4; }
    done
  fi
}
record_arm() { # <threads> <label-prefix> <statdir> <outdir> <lane> <prob> <tag> <arm>
  local th=$1 pre=$2 sd=$3 out=$4 lane=$5 prob=$6 tag=$7 a=$8
  [[ -f "$out/$tag.$a.attr.json" ]] && return
  local T base reps k per st lost lsamp thr
  T=$(stat_median "$sd" "$tag" "$a")
  [[ "$T" -gt 0 ]] || { echo "NO_STAT_MEDIAN $tag $a"; rc_leg=4; return; }
  read -r base reps < <(python3 -c "import math,sys; T=int(sys.argv[1]); b=max(400009, T//8000)|1; print(b, min(40, math.ceil(8000*b/T)))" "$T")
  : > "$out/$tag.$a.runs"
  rm -f "$SCRATCH/$tag.$a.script"
  for ((k = 0; k < reps; k++)); do
    per=$(( (base * (1000 + 7 * k) / 1000) | 1 ))
    cell record "$RECBIN" "$lane" "$prob" "$pre$(arm_label $a)" "$th" "$SCRATCH/$tag.$a.k$k" "$per"
    st="$(perf report -i "$SCRATCH/$tag.$a.k$k.perf.data" --stats 2>/dev/null)"
    lost=$(grep -E '^\s*LOST events' <<< "$st" | awk '{print $3}' | head -1)
    lsamp=$(grep -E '^\s*LOST_SAMPLES events' <<< "$st" | awk '{print $3}' | head -1)
    thr=$(grep -E '^\s*THROTTLE events' <<< "$st" | awk '{print $3}' | head -1)
    echo "k=$k period=$per $(cat "$SCRATCH/$tag.$a.k$k.result") LOST=${lost:-0} LOST_SAMPLES=${lsamp:-0} THROTTLE=${thr:-0} status=$(grep -m1 '^status=' "$SCRATCH/$tag.$a.k$k.out" | cut -d= -f2)" >> "$out/$tag.$a.runs"
    if [[ "${lost:-0}" != 0 || "${lsamp:-0}" != 0 || "${thr:-0}" != 0 ]]; then
      echo "SAMPLING_DEFECT $tag $a k=$k LOST=${lost:-0} LOST_SAMPLES=${lsamp:-0} THROTTLE=${thr:-0}"; rc_leg=5
    fi
    perf script -i "$SCRATCH/$tag.$a.k$k.perf.data" --no-inline -F comm,pid,tid,period,ip,sym,dso \
      2>/dev/null >> "$SCRATCH/$tag.$a.script"
    rm -f "$SCRATCH/$tag.$a.k$k".*
  done
  python3 "$D/adopt_attr.py" "$D/bucket_map.tsv" "$SCRATCH/$tag.$a.script" > "$out/$tag.$a.attr.json.tmp" \
    && mv "$out/$tag.$a.attr.json.tmp" "$out/$tag.$a.attr.json"
  gzip -f "$SCRATCH/$tag.$a.script"
  [[ -f "$out/$tag.$a.attr.json" ]] || { echo "ATTRIBUTION_MISSING $tag $a"; rc_leg=4; }
  echo "RECORD_DONE $tag $a T=$T base=$base reps=$reps $(date -Is)"
}
rc_leg=0
case "$LEG" in
E)
  mkdir -p "$EV/enum/cells" "$EV/results"
  if [[ ! -f "$EV/results/threshold.txt" ]]; then   # L1, criterion T, before the first cell
    python3 "$WT/docs/notes/data/2026-11-m8-dn3/tools/threshold.py" \
      "$WT/docs/notes/data/2026-11-m8-dn1-design/matrices.csv.gz" > "$EV/results/threshold.txt" 2>&1
    trc=$?; cat "$EV/results/threshold.txt"
    [[ $trc -eq 0 ]] || { echo "CRITERION_T_STOP rc=$trc"; exit 6; }
  fi
  while IFS=$'\t' read -r -u 3 pop lane prob; do
    tag="${pop}-${lane}-${prob}"
    for a in N A; do
      [[ -f "$EV/enum/cells/$tag.$a.result" ]] || cell run "$STATBIN" "$lane" "$prob" "$(arm_label $a)" 1 "$EV/enum/cells/$tag.$a"
    done
  done 3< "$EV/roster.tsv"
  python3 "$D/enum_analyze.py" "$EV/enum" > "$EV/enum/enum-analysis.txt" 2>&1; rc=$?
  echo "enum analysis rc=$rc"; [[ $rc -gt $rc_leg ]] && rc_leg=$rc
  sed -n '/== VERDICTS/,$p' "$EV/enum/enum-analysis.txt"
  ;;
S)
  mkdir -p "$EV/stat"
  while IFS=$'\t' read -r -u 3 pop lane prob; do
    stat_rounds 1 "" "$EV/stat" "$lane" "$prob" "${pop}-${lane}-${prob}"
  done 3< "$EV/roster.tsv"
  echo "S done: $(ls "$EV"/stat/*.result | wc -l) results"
  ;;
P)
  mkdir -p "$EV/record"
  [[ -f "$EV/enum/candidates.tsv" ]] || { echo NO_CANDIDATES_FILE; exit 4; }
  while IFS=$'\t' read -r -u 3 pop lane prob; do
    tag="${pop}-${lane}-${prob}"
    for a in N A; do record_arm 1 "" "$EV/stat" "$EV/record" "$lane" "$prob" "$tag" "$a"; done
  done 3< "$EV/enum/candidates.tsv"
  ;;
T)
  mkdir -p "$EV/threads"
  IFS='|' read -r -a cells <<< "$MEDIANS"
  for c in "${cells[@]}"; do
    lane=${c% *}; prob=${c#* }; pop=hs; [[ "$prob" =~ ^hs ]] || pop=sd
    tag="${pop}-${lane}-${prob}"
    stat_rounds default "t0-" "$EV/threads" "$lane" "$prob" "$tag"
    for a in N A; do record_arm default "t0-" "$EV/threads" "$EV/threads" "$lane" "$prob" "$tag" "$a"; done
  done
  ;;
G)
  mkdir -p "$EV/gdb"
  export MKL_NUM_THREADS=1 OMP_NUM_THREADS=1
  IFS='|' read -r -a cells <<< "$MEDIANS"
  printf '%s\n' 'set debuginfod enabled off' 'set breakpoint pending on' 'set pagination off' \
    'break MKL_Read_ThreadENV_Count_Cores' 'commands 1' 'silent' 'printf "HIT_READENV\n"' 'continue' 'end' \
    'break __kmp_serial_initialize' 'commands 2' 'silent' 'printf "HIT_KMPINIT\n"' 'continue' 'end' 'run' \
    > "$SCRATCH/gdb.cmd"
  for c in "${cells[@]}"; do
    lane=${c% *}; prob=${c#* }; pop=hs; [[ "$prob" =~ ^hs ]] || pop=sd
    tag="${pop}-${lane}-${prob}"
    for a in N A; do
      (cd /home/ghecht/Projects/hven-bench-dn1 && taskset -c 2 gdb -batch -nx -x "$SCRATCH/gdb.cmd" --args "$RECBIN" \
        --lane "$lane" --problem "$prob" --options "$(arm_label $a)" --budget-iters 0 \
        --options-dump "$SCRATCH/gdb.optdump" --assert-reference > "$SCRATCH/gdb.$tag.$a.out" 2>&1)
      echo "$tag $a readenv=$(grep -c HIT_READENV "$SCRATCH/gdb.$tag.$a.out") kmp_serial_init=$(grep -c HIT_KMPINIT "$SCRATCH/gdb.$tag.$a.out") status=$(grep -m1 '^status=' "$SCRATCH/gdb.$tag.$a.out" | cut -d= -f2)" | tee -a "$EV/gdb/gdb-counts.txt"
    done
  done
  ;;
*) echo "unknown leg $LEG"; exit 2 ;;
esac
echo "FOREGROUND_END ${LEG} $(date -Is) max_sample_rate=$(cat /proc/sys/kernel/perf_event_max_sample_rate) rc=${rc_leg}"
exit "$rc_leg"
