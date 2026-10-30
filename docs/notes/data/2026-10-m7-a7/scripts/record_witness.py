# Solve-end reading of the kMeasuredCost witness from committed RETAKE18 rows (record, not a measurement).
import csv
import sys

base, label = sys.argv[1], sys.argv[2]
lines = [l for l in open(f'{base}/run-A-{label}/cutest/counters-hven-sqp.csv') if not l.startswith('#')]
rows = list(csv.reader(lines))
h = rows[0]
for r in rows[1:]:
    d = dict(zip(h, r))
    try:
        F = int(d['factorizations'])
        M = int(d['qp_minor_iters'])
        mj = int(d['major_iters'])
        soc = int(d['soc_steps'])
    except ValueError:
        print(f"{d[h[0]]:40s} null")
        continue
    C = M + mj + soc
    print(f"{d[h[0]]:40s} maj {mj:4d} soc {soc:3d} minors {M:7d} facts {F:7d} "
          f"F/M {F / max(1, M):7.3f} F>M+maj+soc {F > C}")
