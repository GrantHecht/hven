#!/usr/bin/env bash
# QPC main driver (runner glue; see gate-QPC/DEVIATIONS.md). Freeze order (DECLARATION section 4/6, C583):
#   leg B -> BINARIES.txt + cells commit -> E-R (census copy verified, baseline certificates, tiers, both ens.py freezes,
#   commit) -> [fork driverY: C, N1, M, M1R, Sm] -> N0 x3 -> Q x3 -> member-0 development runs (DEV store) -> N2 x3 ->
#   P-POWER / P-GATED -> P-GATES.md commit -> fork one E-A driver per arm.  No arm row on TRAIN/CAP/NLP exists before E-R.
# Every solver leg takes its own box-lane acquisition (i19_run.py calls box-lane itself).  A STOP writes $RUN/STOP.txt, exits 3.
set -o pipefail
source "$(dirname "$0")/env.sh"
mkdir -p $RUN/logs $G/cells $G/gate-QPC
cd $HB || exit 1
echo "X start $(date -Is) pid=$$"
step() { echo "== $* $(date -Is)"; }

# ---- leg B
if [ ! -f $RUN/B.done ]; then
  step "leg B"
  ~/.local/bin/box-lane -- bash $G/legs/legB.sh $HB $B $RUN/B-out QPC=$H_PIN QPC-C1=$C1_PIN QPC-C2=$C2_PIN > $RUN/logs/legB.log 2>&1 \
    || stop "leg B failed (see $RUN/logs/legB.log)"
  tail -3 $RUN/logs/legB.log | grep -q "LEG B done" || stop "leg B did not finish"
  touch $RUN/B.done
fi

# ---- BINARIES.txt, lane shas, cells
if [ ! -f $RUN/BIN.done ]; then
  step "BINARIES"
  H0LINE="declare H0     prefix=/home/ghecht/Projects/hven/.claude/worktrees/census/.scratch/install 99712ec324716e6d08ee0545a43d595ca15eec3f 7d2f9f376d6f9f82d554efc2a41a1e52a36769a061d02060c5a765d236c573ca"
  { echo "# BINARIES.txt for DECLARATION-QPC (section 2), written from leg B's printed lines ($(date -Is)); the reader reads only declare lines."
    echo "$H0LINE"; cat $RUN/B-out/binaries-lines.txt; } > $BIN
  [ "$(grep -c '^declare ' $BIN)" = 4 ] || stop "BINARIES.txt does not carry 4 declare lines"
  [ "$(grep -c '^QPC grid ' $BIN)" = 4 ] || stop "BINARIES.txt does not carry 4 grid lines"
  for a in QPC QPC-C1 QPC-C2; do
    L=$(arm_lane $a); want=$(grep "^declare $a " $BIN | awk '{print $NF}'); got=$(sha256sum $L | cut -d' ' -f1)
    [ "$want" = "$got" ] || stop "lane sha mismatch for $a: declared $want actual $got"
    [ "$(cat $L.lane-src)" = 4319ab1512e2de6dbc385eb964be182f0af4eb70 ] || stop "lane-src mismatch for $a"
    grep -q "^HVEN_PIN_SHA $(arm_pin $a)" $(arm_inst $a)/HVEN_PIN.txt || stop "install pin mismatch for $a"
    gw=$(grep "^QPC grid $a " $BIN | awk '{print $NF}'); gg=$(sha256sum $(arm_grid $a) | cut -d' ' -f1)
    [ "$gw" = "$gg" ] || stop "grid sha mismatch for $a"
  done
  gw=$(grep "^QPC grid R " $BIN | awk '{print $NF}'); [ "$gw" = "$(sha256sum $B/R-grid/hven_qp_dump_grid | cut -d' ' -f1)" ] || stop "R grid sha mismatch"
  $PY - <<PY || stop "reader cannot parse BINARIES.txt"
import sys; sys.path.insert(0,"$HB/tu"); import i19_cells
d=i19_cells.read_declared_binaries("$BIN"); assert sorted(d)==["H0","QPC","QPC-C1","QPC-C2"], sorted(d); print("declared arms", sorted(d))
PY
  ls $B/QPC/src/tests/qp/fixtures/e2-d4/*.txt | xargs -n1 basename | grep -v '^FORMAT.txt$' | sort > $G/cells/sens_q_cells.txt
  [ "$(wc -l < $G/cells/sens_q_cells.txt)" = 67 ] || stop "SENS-Q cell list is not 67"
  (cd $G/cells && sha256sum sens_q_cells.txt > SHA256SUMS)
  for a in QPC QPC-C1 QPC-C2; do cp $RUN/B-out/diffstat-$a.txt $G/gate-QPC/diffstat-$a.txt; done
  mkdir -p $G/gate-QPC/B; cp $RUN/B-out/binaries-lines.txt $RUN/B-out/build-*.log $G/gate-QPC/B/ 2>/dev/null
  for a in QPC QPC-C1 QPC-C2; do cp $B/$a/install/HVEN_PIN.txt $G/gate-QPC/B/HVEN_PIN-$a.txt; done
  cp /home/ghecht/Projects/hven/.claude/worktrees/census/.scratch/install/HVEN_PIN.txt $G/gate-QPC/B/HVEN_PIN-R.txt
  gcommit "docs(qpc): BINARIES.txt (lane and grid shas from leg B), SENS-Q cell list, leg B evidence -- before any QPC row" \
    docs/notes/data/2026-11-m8-qpc/BINARIES.txt docs/notes/data/2026-11-m8-qpc/cells docs/notes/data/2026-11-m8-qpc/gate-QPC/B \
    docs/notes/data/2026-11-m8-qpc/gate-QPC/diffstat-QPC.txt docs/notes/data/2026-11-m8-qpc/gate-QPC/diffstat-QPC-C1.txt docs/notes/data/2026-11-m8-qpc/gate-QPC/diffstat-QPC-C2.txt \
    || stop "commit of BINARIES failed"
  touch $RUN/BIN.done
fi

# ---- E-R: the census rows are the baseline; nothing of an arm exists yet
if [ ! -f $RUN/ER.done ]; then
  step "E-R"
  CS=/home/ghecht/Projects/hven-bench-ens/.scratch/census-m8/store
  diff -rq $CS $STORE > $RUN/logs/ER-diff.txt 2>&1
  grep -v "tier-assignment" $RUN/logs/ER-diff.txt | grep -q . && stop "census store and QPC store differ before E-R: $(head -3 $RUN/logs/ER-diff.txt)"
  # manifest of every H0 member directory (census vs copy), committed as cells/H0-copy.sha256
  mf() { (cd $1 && find . -type f -path '*/H0*' -print0 | sort -z | xargs -0 sha256sum); }
  mf $CS > $RUN/census-H0.sha256; mf $STORE > $RUN/qpc-H0.sha256
  cmp $RUN/census-H0.sha256 $RUN/qpc-H0.sha256 || stop "H0 manifest of the copy differs from the census store"
  cp $RUN/qpc-H0.sha256 $G/cells/H0-copy.sha256
  # baseline certificate leg: already complete in the census (certs-done.json); explicit arm names (the harness's no-name
  # form crashes on a store whose points/ directory exists, i19_run.member_stores); a no-op here, recorded.
  H0ARMS="H0 H0@x1 H0@x2 H0@x3 H0@x4 H0@x5 H0@t1 H0@t2 H0@t3 H0@t4 H0@t5"
  $PY tu/i19_run.py certs --store $STORE --binaries $BIN $H0ARMS > $RUN/logs/ER-certs.txt 2>&1 || stop "baseline certificate leg failed: $(tail -3 $RUN/logs/ER-certs.txt)"
  $PY - <<PY > $G/gate-QPC/ER-certificates-H0.txt || stop "false certificate under H0"
import csv,collections,sys
rows=[r for r in csv.DictReader(open("$STORE/certificates.csv")) if r["arm"].split("@")[0]=="H0"]
c=collections.Counter(r["classification"] for r in rows); print("H0 certificate rows",len(rows),dict(c))
print("not reproduced", sum(1 for r in rows if r["reproduced"] not in ("True","true","1","yes")))
sys.exit(1 if any(k not in ("STATIONARY","APPROXIMATELY STATIONARY") for k in c) else 0)
PY
  $PY tu/i19_run.py tiers --store $STORE > $G/gate-QPC/ER-tiers.txt 2>&1 || stop "tier assignment failed"
  cp $STORE/NLP-46/tier-assignment.csv $G/cells/tier-assignment.csv
  $PY tu/ens.py freeze --store $STORE --arm H0 --corpora TRAIN-238,CAP-238-1,CAP-238-01 --m 5 --kind x0 --out $G/cells/FREEZE-TRAIN.json > $G/gate-QPC/ER-freeze-train.txt 2>&1 || stop "freeze TRAIN failed"
  $PY tu/ens.py freeze --store $STORE --arm H0 --corpora NLP-46 --out $G/cells/FREEZE-NLP46.json > $G/gate-QPC/ER-freeze-nlp46.txt 2>&1 || stop "freeze NLP-46 failed"
  cp $STORE/certificates.csv $G/cells/H0-certificates.csv
  gcommit "docs(qpc): E-R class freeze (P-CLASS) -- census H0 copy verified byte-identical, baseline certificate leg, NLP-46 tiers, FREEZE-TRAIN.json and FREEZE-NLP46.json; committed before any arm member-0 row" \
    docs/notes/data/2026-11-m8-qpc/cells docs/notes/data/2026-11-m8-qpc/gate-QPC/ER-certificates-H0.txt docs/notes/data/2026-11-m8-qpc/gate-QPC/ER-tiers.txt \
    docs/notes/data/2026-11-m8-qpc/gate-QPC/ER-freeze-train.txt docs/notes/data/2026-11-m8-qpc/gate-QPC/ER-freeze-nlp46.txt || stop "commit of E-R failed"
  rm -rf $DEV; cp -a $STORE $DEV     # member-0 development store (H0 dirs + tier assignment), made before any arm row
  touch $RUN/ER.done
fi

# ---- driver Y (non-ensemble legs) and the SENS-Q neutrality legs
if [ ! -f $RUN/Y.forked ]; then
  setsid nohup bash $G/gate-QPC/chain/driverY.sh > $RUN/logs/driverY.log 2>&1 < /dev/null &
  echo $! > $RUN/driverY.pid; touch $RUN/Y.forked
fi
if [ ! -f $RUN/N0.done ]; then
  for a in QPC QPC-C1 QPC-C2; do
    step "N0 $a"
    ~/.local/bin/box-lane -- bash $G/legs/legN0.sh $B/R-grid/hven_qp_dump_grid $(arm_grid $a) $B/QPC/src $RUN/N0/$a > $RUN/logs/N0-$a.log 2>&1; rc=$?
    tail -4 $RUN/logs/N0-$a.log
    [ $rc -le 1 ] || stop "N0 $a rc=$rc"
    grep -q "^N0 worst=0" $RUN/logs/N0-$a.log || stop "N0 $a: a difference outside the declared-mover rule (rc=$rc)"
    step "Q $a"
    bash $G/legs/legQ.sh $RUN/N0/$a $RUN/Q/$a > $RUN/logs/Q-$a.log 2>&1; rc=$?
    cat $RUN/logs/Q-$a.log | tail -12
    [ $rc -eq 0 ] || stop "Q $a: V-QP / V-HONEST failure under $a (rc=$rc) -- a false certificate or a refused honest point; see $RUN/logs/Q-$a.log"
  done
  mkdir -p $G/gate-QPC/N0 $G/gate-QPC/Q
  for a in QPC QPC-C1 QPC-C2; do mkdir -p $G/gate-QPC/N0/$a $G/gate-QPC/Q/$a; cp $RUN/N0/$a/* $G/gate-QPC/N0/$a/; cp $RUN/Q/$a/* $G/gate-QPC/Q/$a/; cp $RUN/logs/N0-$a.log $RUN/logs/Q-$a.log $G/gate-QPC/; done
  gcommit "docs(qpc): legs N0 (SENS-Q neutrality) and Q (V-QP, V-HONEST) for QPC, QPC-C1, QPC-C2" docs/notes/data/2026-11-m8-qpc/gate-QPC/N0 docs/notes/data/2026-11-m8-qpc/gate-QPC/Q \
    docs/notes/data/2026-11-m8-qpc/gate-QPC/N0-QPC.log docs/notes/data/2026-11-m8-qpc/gate-QPC/N0-QPC-C1.log docs/notes/data/2026-11-m8-qpc/gate-QPC/N0-QPC-C2.log \
    docs/notes/data/2026-11-m8-qpc/gate-QPC/Q-QPC.log docs/notes/data/2026-11-m8-qpc/gate-QPC/Q-QPC-C1.log docs/notes/data/2026-11-m8-qpc/gate-QPC/Q-QPC-C2.log || stop "commit of N0/Q failed"
  touch $RUN/N0.done
fi

# ---- member-0 development runs (DEV store), one process tree per arm, then N2
if [ ! -f $RUN/DEV.done ]; then
  step "member-0 development runs"
  pids=""
  for a in QPC QPC-C1 QPC-C2; do
    ( $PY $HB/tu/i19_run.py arm $a --store $DEV --binary $(arm_lane $a) --hven-sha $(arm_pin $a) --hven-prefix $(arm_inst $a) --binaries $BIN \
        --corpora TRAIN-238,CAP-238-1,CAP-238-01,NLP-46 --phase all > $RUN/logs/dev-m0-$a.log 2>&1; echo $? > $RUN/dev-m0-$a.rc ) &
    pids="$pids $!"
  done
  wait $pids
  for a in QPC QPC-C1 QPC-C2; do [ "$(cat $RUN/dev-m0-$a.rc)" = 0 ] || stop "member-0 development run $a rc=$(cat $RUN/dev-m0-$a.rc) (see $RUN/logs/dev-m0-$a.log)"; done
  touch $RUN/DEV.done
fi
if [ ! -f $RUN/N2.done ]; then
  mkdir -p $G/gate-QPC/N2
  for a in QPC QPC-C1 QPC-C2; do
    step "N2 $a"
    $PY $G/legs/n2_qpc.py $HB $DEV $BIN $a > $G/gate-QPC/N2/N2-$a.txt 2>&1; rc=$?
    tail -3 $G/gate-QPC/N2/N2-$a.txt
    [ $rc -eq 0 ] || { gcommit "docs(qpc): N2 STOP evidence ($a)" docs/notes/data/2026-11-m8-qpc/gate-QPC/N2; stop "N2 $a: difference outside the declared-mover rule"; }
  done
  touch $RUN/N2.done
fi

# ---- P-POWER / P-GATED -> gate-QPC/P-GATES.md, committed before any E-A row
if [ ! -f $RUN/GATES.done ]; then
  step "P-GATES"
  OUT=$G/gate-QPC/P-GATES.md
  { echo "# P-GATES -- P-POWER and P-GATED readings (DECLARATION-QPC section 4), written $(date -Is) before any E-A row"
    echo; echo "Source: the member-0 DEVELOPMENT store $DEV (NOT ensemble rows), classes from cells/FREEZE-*.json (E-R)."
    for a in QPC QPC-C1 QPC-C2; do
      echo; echo "## P-POWER $a (TRAIN-238 + CAP-238-1 + CAP-238-01)"; echo '```'
      $PY $G/legs/qpc_ens_extra.py power $HB $DEV $BIN $G/cells/FREEZE-TRAIN.json $a 2>&1; echo '```'
      echo; echo "## P-POWER $a (NLP-46)"; echo '```'
      $PY $G/legs/qpc_ens_extra.py power $HB $DEV $BIN $G/cells/FREEZE-NLP46.json $a 2>&1; echo '```'
    done
    echo; echo "## SENS-Q: P-POWER and P-GATED (legs N0 CSVs; gate-QPC/chain/p_gates.py)"; echo '```'
    $PY $G/gate-QPC/chain/p_gates.py $RUN/N0 $G/cells/sens_q_gated.tsv 2>&1; echo '```'
    echo; echo "CENSUS-57 P-POWER: NOT COMPUTED (leg N3 has no script in legs/; see DEVIATIONS.md)."
    echo; echo "Rule (DECLARATION section 4): a component with fewer than 10 R-STABLE solved exercised cells in the live corpora is corpus-EXPLORATORY."
  } > $OUT 2>&1
  gcommit "docs(qpc): P-GATES.md (P-POWER, P-GATED) and cells/sens_q_gated.tsv -- before any E-A row" docs/notes/data/2026-11-m8-qpc/gate-QPC/P-GATES.md docs/notes/data/2026-11-m8-qpc/cells/sens_q_gated.tsv \
    docs/notes/data/2026-11-m8-qpc/gate-QPC/N2 || stop "commit of P-GATES failed"
  touch $RUN/GATES.done
fi

# ---- E-A: one driver per arm
for a in QPC QPC-C1 QPC-C2; do
  if [ ! -f $RUN/EA-$a.forked ]; then
    setsid nohup bash $G/gate-QPC/chain/driverEA.sh $a > $RUN/logs/driverEA-$a.log 2>&1 < /dev/null &
    echo $! > $RUN/driverEA-$a.pid; touch $RUN/EA-$a.forked
  fi
done
echo "X done $(date -Is)"
