#!/usr/bin/env bash
# M8 DN-1 M leg (../DECLARATION-dn1legs.txt): hs043 and HS84, lane hven-sqp, on the frozen S binary, serial and
# alone, at two thread settings, kNever against kAlways at the same setting:
#   THREADS=1        labels default / dense-default        (MKL_NUM_THREADS=1 OMP_NUM_THREADS=1, core 2;
#                                                            the lane's common.threads = 1)
#   THREADS=default  labels t0-default / t0-dense-default  (both variables UNSET, unpinned; common.threads = 0)
# Per (cell, setting): one untimed warm-up of each arm (discarded); perf stat rounds in the order N A A N N A
# (3 per arm; instructions:u, all threads); then perf record per arm, DN-0's P-leg repetition rule from that arm's
# stat median T: base = max(1000003, T // 8000) | 1, reps = min(40, ceil(4000 * base / T)), rep k at period
# (base * (1000 + 7k) / 1000) | 1 (all >= 400000), call graph dwarf,16384; each rep's perf script appended to
# one file per arm. Then m_analyze.py -> EV/m-analysis.txt.
# usage: m_run.sh <evidence-dir> <expected S binary sha256>   Run as: flock /tmp/box-build.lock bash m_run.sh ...
set -uo pipefail
WT=/home/ghecht/Projects/hven/.claude/worktrees/dn1
D=$WT/docs/notes/data/2026-11-m8-dn1-impl/dn1legs
EV=${1:?evidence dir}
WANT=${2:?S binary sha256}
p=$(pgrep -a hven_bench_cell); [[ -z "$p" ]] && echo PGREP_EMPTY || { echo "PGREP_BUSY ${p}"; exit 3; }
echo "FOREGROUND_START $(date -Is)"
got=$(sha256sum "$WT/build-dn1r2/dn1bin/hven_bench_cell.S" | cut -d' ' -f1)
[[ "$got" == "$WANT" ]] || { echo "S_BINARY_SHA_MISMATCH $got"; exit 4; }
mkdir -p "$EV/m"
cat /proc/sys/kernel/perf_event_paranoid /proc/sys/kernel/perf_event_max_sample_rate > "$EV/m/perf-sysctl.txt"
stat_instr() { awk -F, '$3 ~ /^instructions/ {print $1}' "$1"; }
for cell in hs043 HS84; do
  for setting in 1 default; do
    if [[ $setting == 1 ]]; then LN=default; LA=dense-default; else LN=t0-default; LA=t0-dense-default; fi
    pre="$EV/m/${cell}.t${setting}"
    bash "$D/dn1_cell.sh" run hven-sqp "$cell" "$LN" "$setting" "${pre}.warm-N" 300 > /dev/null
    bash "$D/dn1_cell.sh" run hven-sqp "$cell" "$LA" "$setting" "${pre}.warm-A" 300 > /dev/null
    k=0
    for arm in N A A N N A; do
      k=$((k + 1))
      lab=$LN; [[ $arm == A ]] && lab=$LA
      bash "$D/dn1_cell.sh" stat hven-sqp "$cell" "$lab" "$setting" "${pre}.stat-${arm}.s${k}" 300
    done
    for arm in N A; do
      lab=$LN; [[ $arm == A ]] && lab=$LA
      T=$(for f in "${pre}".stat-${arm}.s*.stat; do stat_instr "$f"; done | sort -n | sed -n 2p)
      read -r base reps < <(python3 -c "import math,sys; T=int(sys.argv[1]); b=max(1000003, T//8000)|1; print(b, min(40, max(1, math.ceil(4000*b/max(T,1)))))" "$T")
      : > "${pre}.rec-${arm}.runs"
      rm -f "${pre}.rec-${arm}.script"
      for ((r = 0; r < reps; r++)); do
        per=$(( (base * (1000 + 7 * r) / 1000) | 1 ))
        bash "$D/dn1_cell.sh" record hven-sqp "$cell" "$lab" "$setting" "${pre}.rec-${arm}.k${r}" 300 "$per" > /dev/null
        lost=$(perf report -i "${pre}.rec-${arm}.k${r}.perf.data" --stats 2>/dev/null | grep LOST | tr -s ' ' | tr '\n' ';')
        echo "k=${r} period=${per} $(cat "${pre}.rec-${arm}.k${r}.result") lost=[${lost}]" >> "${pre}.rec-${arm}.runs"
        perf script -i "${pre}.rec-${arm}.k${r}.perf.data" --no-inline -F comm,pid,tid,period,ip,sym,dso \
          2>/dev/null >> "${pre}.rec-${arm}.script"
        rm -f "${pre}.rec-${arm}.k${r}.perf.data" "${pre}.rec-${arm}.k${r}.out" "${pre}.rec-${arm}.k${r}.err" \
              "${pre}.rec-${arm}.k${r}.optdump"
      done
      echo "RECORD ${cell} t${setting} ${arm} T=${T} base=${base} reps=${reps}"
    done
  done
done
python3 "$D/m_analyze.py" "$EV/m" > "$EV/m-analysis.txt" 2>&1
echo "analysis rc=$?"
sed -n '/== VERDICTS/,$p' "$EV/m-analysis.txt"
gzip -f "$EV"/m/*.script
echo "FOREGROUND_END $(date -Is)"
