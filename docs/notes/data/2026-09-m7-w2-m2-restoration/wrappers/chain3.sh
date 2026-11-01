#!/usr/bin/env bash
# M7 W2 M2 chain 3: THE EXCURSION LADDER over the seven cells, both callback
# arms, one cell at a time, ALONE under the box lock.
#
# TERMS, declared here and repeated in the artifact: one cell per process,
# serial, MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, pinned to one physical core,
# the whole run holding /tmp/box-build.lock -- no timeout, no fallback. Wall is
# INFORMATIONAL (CLAUDE.md section 7) and the 900 s per-leg cap is a SAFETY STOP,
# not a DNF condition: none of these seven is a wall cell in H3's run-A, so a
# leg that hits it is a finding, reported as CENSORED and never as a row.
#
# A foreign `hven_bench_cell` alive while this holds the lock would mean
# something is running outside the lock, so each leg refuses to start on one.
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
LOG=$S/logs/chain3.log
mkdir -p "$S/logs" "$S/out/ladder" "$S/out/ladder-off"
exec >"$LOG" 2>&1
echo "CHAIN3_START $(date -Is)"
flock /tmp/box-build.lock bash -c '
set -o pipefail
ROOT=/home/ghecht/Projects/hven
S=$ROOT/.scratch/m7w2m2
BIN=$S/probebuild/m2_probe
echo "FOREGROUND_START chain3 $(date -Is)"
cd /home/ghecht/Projects/hven-bench || exit 90
echo "probe sha256: $(sha256sum "$BIN")"
echo "hven library sha256: $(sha256sum "$S/install/lib64/libhven.a")"
worst=0
for cell in STEENBRB STEENBRC STEENBRG ZAMB2-8 YORKNET BRITGAS READING6; do
  for arm in on off; do
    foreign=$(pgrep -x hven_bench_cell | tr "\n" " ")
    if [ -n "$foreign" ]; then
      echo "ABORTED before $cell/$arm: a foreign cell binary is alive while this run holds the lock: $foreign"
      exit 95
    fi
    dir=$S/out/ladder; [ "$arm" = off ] && dir=$S/out/ladder-off
    t0=$(date +%s.%N)
    MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 taskset -c 2 timeout 900 \
      "$BIN" --problem "$cell" --callback "$arm" > "$dir/$cell.txt" 2>"$dir/$cell.stderr"
    rc=$?
    t1=$(date +%s.%N)
    printf "LEG %-10s %-3s rc=%-3s wall_s=%.1f (INFORMATIONAL)\n" "$cell" "$arm" "$rc" \
      "$(echo "$t1 - $t0" | bc)"
    if [ "$rc" = 124 ]; then
      echo "    CENSORED: the 900 s safety stop fired -- this is a finding, not a row"
      worst=1
    elif [ "$rc" != 0 ]; then
      echo "    LEG FAILED rc=$rc"; head -5 "$dir/$cell.stderr"
      worst=1
    else
      grep -E "^(status|f|major_iters|restoration_iters|restoration_entries|restoration_h_in|restoration_h_out|restoration_f_in|restoration_f_out)=" "$dir/$cell.txt" | sed "s/^/    /"
    fi
  done
done
echo "LADDER_WORST=$worst"
echo "FOREGROUND_END chain3 $(date -Is) rc=$worst"
exit $worst
'
rc=$?
echo "WRAPPER_RC chain3 rc=${rc}"
echo "--- SUMMARISE (outside the lock: no solver runs, pure file folding)"
python3 -B "$S/summarise.py"
echo "SUMMARISE_RC=$?"
echo "CHAIN3_END $(date -Is)"
exit $rc
