#!/usr/bin/env bash
# QPC E-A driver for ONE arm (runner glue): member 0 (all four corpora, afresh -- the development rows are not reused), x0 seeds
# 1-5 on TRAIN-238 / CAP-238-1 / CAP-238-01, seeds 1-2 on NLP-46, tr seeds 1-5 on TRAIN-238 (reported), then the certificate leg
# (explicit arm names: the harness's no-name form crashes on a store with a points/ directory), then the declared readers.
# usage: driverEA.sh <QPC|QPC-C1|QPC-C2>      Resumable: each member leaves $RUN/EA-<arm>-<member>.done.
set -o pipefail
source "$(dirname "$0")/env.sh"
A=${1:?arm}; mkdir -p $RUN/logs $G/gate-QPC/reads
cd $HB || exit 1
echo "EA $A start $(date -Is) pid=$$"
run_member() { # run_member <member arm name> <corpora>
  local m=$1 c=$2 tag; tag=${m//@/_}
  [ -f $RUN/EA-$tag.done ] && { echo "skip $m"; return 0; }
  echo "== $m $c $(date -Is)"
  $PY $HB/tu/i19_run.py arm $m --store $STORE --binary $(arm_lane $A) --hven-sha $(arm_pin $A) --hven-prefix $(arm_inst $A) --binaries $BIN \
    --corpora $c --phase all >> $RUN/logs/EA-$tag.log 2>&1; local rc=$?
  echo "== $m rc=$rc $(date -Is)"
  [ $rc -eq 0 ] || stop "E-A $m rc=$rc (see $RUN/logs/EA-$tag.log)"
  touch $RUN/EA-$tag.done
}
ALL=TRAIN-238,CAP-238-1,CAP-238-01
run_member $A $ALL,NLP-46
for s in 1 2; do run_member $A@x$s $ALL,NLP-46; done
for s in 3 4 5; do run_member $A@x$s $ALL; done
for s in 1 2 3 4 5; do run_member $A@t$s TRAIN-238; done
# certificate leg (V2): a false certificate under the arm FAILS the arm (reported by the reader), but the leg itself stops on one
if [ ! -f $RUN/EA-$A-certs.done ]; then
  echo "== certs $A $(date -Is)"
  $PY $HB/tu/i19_run.py certs --store $STORE --binaries $BIN $A $A@x1 $A@x2 $A@x3 $A@x4 $A@x5 $A@t1 $A@t2 $A@t3 $A@t4 $A@t5 > $RUN/logs/EA-$A-certs.log 2>&1; rc=$?
  echo "== certs $A rc=$rc $(date -Is)"; tail -3 $RUN/logs/EA-$A-certs.log
  cp $RUN/logs/EA-$A-certs.log $G/gate-QPC/reads/certs-$A.log
  [ $rc -eq 0 ] || stop "certificate leg $A rc=$rc (a false certificate under an arm member STOPS: see $RUN/logs/EA-$A-certs.log)"
  touch $RUN/EA-$A-certs.done
fi
# readers (declared commands; outputs are evidence, the decision rule is applied by the reader of RESULTS, not here)
R=$G/gate-QPC/reads; rc=0
$PY tu/ens.py read --store $STORE --base H0 --arm $A --freeze $G/cells/FREEZE-TRAIN.json --binaries $BIN --json $R/read-$A-TRAIN.json > $R/read-$A-TRAIN.txt 2>&1 || echo "reader TRAIN rc=$?" >> $R/read-$A-rc.txt
$PY tu/ens.py read --store $STORE --base H0 --arm $A --freeze $G/cells/FREEZE-NLP46.json --binaries $BIN --json $R/read-$A-NLP46.json > $R/read-$A-NLP46.txt 2>&1 || echo "reader NLP46 rc=$?" >> $R/read-$A-rc.txt
for f in TRAIN NLP46; do
  for c in vstab qm; do
    $PY $G/legs/qpc_ens_extra.py $c $HB $STORE $BIN $G/cells/FREEZE-$f.json $A > $R/$c-$A-$f.txt 2>&1 || echo "$c $f rc=$?" >> $R/read-$A-rc.txt
  done
  $PY $G/legs/qpc_ens_extra.py power $HB $STORE $BIN $G/cells/FREEZE-$f.json $A > $R/power-$A-$f.txt 2>&1
done
echo "EA $A done $(date -Is) (readers: see $R/read-$A-rc.txt if present)"
touch $RUN/EA-$A.ALLDONE
