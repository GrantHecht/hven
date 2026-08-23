#!/bin/bash
# Four-arm quiet attribution session: the saved bench binaries, alternated per rep
# (each rep = one pass over all arms in order, arm order rotated per rep), plus the
# identity/discriminator probes. Runs under the box lock; refuses a loaded box.
set -u
R=/home/ghecht/Projects/tycho/.scratch/attr-results; cd "$R" || exit 1
ARMS=(72baafd f07184b d3d4ec2 3f04c0b); REPS=${1:-5}
FILTER='BM_Phase_Transcribe|BM_Phase_Construct|BM_InteriorPointSolver_'
exec 9>/tmp/box-build.lock; flock -w 14400 9 || { echo "lock timeout"; exit 2; }
l=$(cut -d' ' -f1 /proc/loadavg); awk -v l=$l 'BEGIN{exit !(l<0.6)}' || { echo "box not idle: load $l"; exit 3; }
{ echo "session start $(date -Is) load=$(cut -d' ' -f1-3 /proc/loadavg) top=$(ps -eo pcpu,comm --sort=-pcpu | sed -n 2,4p | tr '\n' ',')"; cat provenance-arms.txt 2>/dev/null; } > session-provenance.txt
for rep in $(seq 1 $REPS); do
  for k in $(seq 0 3); do
    a=${ARMS[$(( (k+rep-1) % 4 ))]}
    ./bench_all-$a --benchmark_filter="$FILTER" --benchmark_repetitions=1 \
      --benchmark_out="session-rep$rep-$a.json" --benchmark_out_format=json > "session-rep$rep-$a.txt" 2>&1
    echo "rep $rep arm $a done $(date -Is) load=$(cut -d' ' -f1 /proc/loadavg)" | tee -a session.log
  done
done
for a in "${ARMS[@]}"; do
  [ -x "probe-$a" ] || { echo "probe-$a missing" | tee -a session.log; continue; }
  for p in 1 0; do MKL_NUM_THREADS=1 ./probe-$a $p 3 2>&1 | sed 's/\x1b\[[0-9;]*m//g' | grep -E '^(Brach|PolarLT)' | sed "s/^/arm=$a /" | tee -a session-probes.txt; done
done
echo "session end $(date -Is) load=$(cut -d' ' -f1-3 /proc/loadavg)" >> session-provenance.txt
