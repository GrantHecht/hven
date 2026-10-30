#!/bin/bash
# Re-derives every comparison transcript in out/ from the committed CSVs (no solver runs).
# Run from the repository root.
set -o pipefail
D=docs/notes/data/2026-10-m7-a1-impl
S=$D/scripts
: > "$D/out/u0-exact-vs-scw-record.txt"
for e in walk ssn ipm; do
    python3 -B scripts/compare_replay.py "docs/notes/data/2026-10-m7-scw-gates/gates/replay/head-$e.csv" \
        "$D/out/u0-head-$e.csv" "SCW RECORD HEAD-$e vs A1 HEAD-$e (EXACT, field kOff)" \
        >> "$D/out/u0-exact-vs-scw-record.txt"
    echo "EXACT_RC_$e=$?" >> "$D/out/u0-exact-vs-scw-record.txt"
done
python3 -B "$S/i2_compare.py" repro docs/notes/data/2026-08-m6-w1-acceptance/a4-gate-mu1e-2.csv \
    "$D/out/i2-off.csv" > "$D/out/i2-off-vs-committed.txt"
: > "$D/out/i2-verdicts.txt"
for a in ON-2:i2-on2.csv ON-2-seam:i2-seam-ON2.csv ON-2X:i2-seam-ON2X.csv ON-1:i2-seam-ON1.csv; do
    python3 -B "$S/i2_compare.py" arm "$D/out/i2-off.csv" "$D/out/${a#*:}" "${a%%:*}" >> "$D/out/i2-verdicts.txt"
    echo >> "$D/out/i2-verdicts.txt"
done
python3 -B "$S/i2_compare.py" same "$D/out/i2-on2.csv" "$D/out/i2-seam-ON2.csv" >> "$D/out/i2-verdicts.txt"
python3 -B "$S/i2_compare.py" same "$D/out/i2-on2.csv" "$D/out/i2-seam-ON2X.csv" >> "$D/out/i2-verdicts.txt"
python3 -B "$S/f7_compare.py" "$D/out/i3b-off.csv" "$D/out/i3b-on.csv" I3b-ON-2 > "$D/out/i3b-verdict.txt"
python3 -B "$S/f7_compare.py" "$D/out/u0-ipm-off-arm.csv" "$D/out/u0-ipm-on.csv" U0-ipm-ON-2 \
    > "$D/out/u0-ipm-on-verdict.txt"
echo SCORED
