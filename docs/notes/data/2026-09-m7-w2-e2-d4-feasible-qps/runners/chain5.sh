#!/usr/bin/env bash
# cells under the candidate rules (each cell its own locked leg, via run.sh), then U0 + HS per configuration
D=/home/ghecht/Projects/hven/.scratch/m7w2e2d4
until [ -f $D/logs/chain4.log ] && grep -q DONE $D/logs/chain4.log; do sleep 10; done
CFG_D=structfirst,greys,greyd,unlock,f5,f5s,screenadmit
run() { $D/tools/run.sh "$@" >> $D/logs/cells5.log 2>&1; }
# the combined candidate on every relevant cell (+ the must-stay-optimal cells, baseline and candidate)
for c in YORKNET DALLASM STEENBRB ACOPP57 ACOPR57 READING6 ZAMB2-8 SMBANK; do run ${c}_D $c default HVEN_D4_FIX=$CFG_D; done
run DALLASM_f4_D DALLASM default HVEN_D4_FIX=$CFG_D HVEN_D3_F4=1
for c in READING6 ZAMB2-8 SMBANK; do run ${c}_def $c default; done
# single rules on the cell of their class
run ACOPP57_f5 ACOPP57 default HVEN_D4_FIX=f5,f5s
run ACOPR57_f5 ACOPR57 default HVEN_D4_FIX=f5,f5s
run STEENBRB_sf STEENBRB default HVEN_D4_FIX=structfirst
run ACOPR57_unlock ACOPR57 default HVEN_D4_FIX=unlock
run YORKNET_sa YORKNET default HVEN_D4_FIX=screenadmit
run YORKNET_gg YORKNET default HVEN_D4_FIX=structfirst,greys,greyd,unlock
run SAWPATH_D SAWPATH default HVEN_D4_FIX=$CFG_D
echo CELLS_DONE >> $D/logs/cells5.log
for cfg in $CFG_D f5,f5s structfirst unlock screenadmit structfirst,greys,greyd; do
  t=${cfg//,/+}
  $D/tools/u0.sh "$t" "$cfg" >> $D/logs/u0hs5.log 2>&1
  $D/tools/hs.sh "$t" "$cfg" >> $D/logs/u0hs5.log 2>&1
done
echo ALL_DONE >> $D/logs/u0hs5.log
