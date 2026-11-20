#!/usr/bin/env bash
# usage: job.sh BIN LABEL KIND ARG ENVSPEC  -- one pinned job; KIND = issue|dump|shard; ENVSPEC = comma-separated ENV=VAL (or -)
W=$(cd "$(dirname "$0")/.." && pwd)
bin="$1"; label="$2"; kind="$3"; arg="$4"; envs="$5"
phys=(${BOX_LANE_PHYS:-0 1 2 3}); cpu=${phys[${SLOT:-0}]}
ev=(); [ "$envs" != "-" ] && IFS=, read -ra ev <<< "$envs"
base=(env TMPDIR=$W/.scratch/tmp MKL_NUM_THREADS=1 OMP_NUM_THREADS=1 "${ev[@]}")
O=$W/.scratch/out/$label; mkdir -p $O
case $kind in
 issue) alg=${arg%:*}; f=${arg#*:}; x=(); [ $alg = refac ] && x=(I18_REFAC=1)
   r=$("${base[@]}" I18_THREADS=1 I18_DUMP=$W/tests/qp/fixtures/e2-d4/I18_qp_m137.txt I18_FEAS=$f "${x[@]}" timeout 120 taskset -c $cpu "$bin" --gtest_filter='I18Probe.*' 2>/dev/null | grep '^RES')
   echo "ISSUE $alg $f ${r:-FAIL}" > $O/issue-$alg-$f.txt ;;
 dump) name=$(echo "$arg" | sed "s|$W/||; s|/|__|g")
   r=$("${base[@]}" I18_THREADS=1 I18_DUMP="$arg" timeout 120 taskset -c $cpu "$bin" --gtest_filter='I18Probe.*' 2>/dev/null | grep '^RES' | awk '{print $2,$3,$4,$5,$6,$9}')
   echo "$name ${r:-TIMEOUT_OR_FAIL}" > $O/dump-$name.txt ;;
 shard) "${base[@]}" GTEST_TOTAL_SHARDS=4 GTEST_SHARD_INDEX=$arg timeout 3000 taskset -c $cpu "$bin" --gtest_filter='-I18Probe.*' --gtest_brief=1 > $O/shard-$arg.log 2>&1
   echo "rc=$?" >> $O/shard-$arg.log ;;
esac
