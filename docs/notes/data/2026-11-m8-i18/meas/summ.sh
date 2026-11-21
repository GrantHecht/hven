#!/usr/bin/env bash
# usage: summ.sh LABEL [BASELABEL] -- summary of one candidate vs base
W=$(cd "$(dirname "$0")/.." && pwd); O=$W/.scratch/out
l=$1; b=${2:-base}
cat $O/$l/dump-*.txt | sort > $O/$l.corpus
cat $O/$l/issue-*.txt | awk '{print $2,$3,$5,$6,$7}'
echo "corpus: $(awk '{print $2}' $O/$l.corpus | sort | uniq -c | tr '\n' ' ')"
echo "failed tests: $(cat $O/$l/shard-*.log | grep -c '^\[  FAILED  \]')  rcs: $(grep -h '^rc=' $O/$l/shard-*.log | tr '\n' ' ')"
cat $O/$l/shard-*.log | grep '^\[  FAILED  \]' | sed 's/ (.*//' | sort -u
if [ "$l" != "$b" ] && [ -f $O/$b.corpus ]; then
  echo "movers vs $b (status minors facts):"
  join <(awk '{print $1,$2,$3,$4}' $O/$b.corpus) <(awk '{print $1,$2,$3,$4}' $O/$l.corpus) | awk '$2!=$5{print "  STATUS",$0} $2==$5 && $3!=$6 {m++} END{print "  same-status minors-moved:",m+0}'
fi
