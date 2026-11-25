#!/usr/bin/env bash
# usage: batch.sh BIN SPECFILE -- SPECFILE lines: LABEL ENVSPEC; all jobs of all candidates through one pool of width BOX_LANE_JOBS (pinned per physical core)
W=$(cd "$(dirname "$0")/.." && pwd)
bin="$1"; spec="$2"
[ -x "$bin" ] || { echo NO_BIN; exit 2; }
list=$( { ls $W/tests/qp/fixtures/e2-d4/*.txt | grep -v -e FORMAT -e I18_; find $W/docs/notes/data/2026-09-m7-w2-e2-d4-feasible-qps/dumps -name 'qp*.txt'; } | sort)
[ "$(echo "$list" | wc -l)" -eq 146 ] || { echo CORPUS_NOT_146; exit 3; }
mkdir -p $W/.scratch/out; jobs=$W/.scratch/out/jobs.$$; : > $jobs
while read -r label envs; do
  [ -z "$label" ] && continue
  rm -rf $W/.scratch/out/$label
  for s in 0 1 2 3; do echo "$label shard $s $envs"; done >> $jobs
  for a in border:1e-9 border:1e-10 refac:1e-9 refac:1e-10; do echo "$label issue $a $envs"; done >> $jobs
  for f in $list; do echo "$label dump $f $envs"; done >> $jobs
done < "$spec"
echo "jobs: $(wc -l < $jobs) start $(date -Is)"
xargs -P ${BOX_LANE_JOBS:-4} --process-slot-var=SLOT -L1 sh -c 'exec "$0" "$1" "$2" "$3" "$4" "$5"' $W/.scratch/job.sh "$bin" < $jobs
echo "end $(date -Is)"
