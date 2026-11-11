#!/usr/bin/env python3
# M8 DN-1d r2 (../../DECLARATION-r2.txt): r1's copy of this file re-pointed (r2/, build-dn1de2/); older provenance lines below are historical.
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/dn1legs/ and re-pointed (worktree dn1b, evidence this directory, the DN-1b lane binary build-dn1be/dn1bbin/hven_bench_cell); older provenance lines below are historical.
# M8 DN-1 T leg (../DECLARATION-dn1legs.txt): this run's results.csv against PF-1's reading
# (hven-bench runs/2026-11-m8-pf1-train/default/results.csv). Criterion (DECLARATION.txt T, verbatim there):
# >= 16/23 optimal and no cell lost against PF-1's reading. Status rule (owner-accepted): a PF-1 success keeps its
# status and its objective (within 1e-8*max(1,|f|), S1's tolerance, reported); moves among non-success cells are
# reported in full.
# usage: t_compare.py PF1_DIR THIS_DIR
import csv
import sys

SUCCESS = ('optimal', 'acceptable')


def load(d):
    with open(d + '/results.csv') as f:
        rows = list(csv.DictReader(line for line in f if not line.startswith('#')))
    return {r['problem']: r for r in rows}


pf1, cur = load(sys.argv[1]), load(sys.argv[2])
print(f'cells: PF-1 {len(pf1)}, this run {len(cur)}')
if set(pf1) != set(cur) or len(cur) != 23:
    print('T_CELL_SET_MISMATCH', sorted(set(pf1) ^ set(cur)))
    sys.exit(3)
lost, obj_moved, moves = [], [], []
for p in sorted(pf1):
    a, b = pf1[p], cur[p]
    line = (f'{p:10s} PF-1 {a["status"]}/{a["outcome"]}/{a["iterations"]} f={a["objective"]} | '
            f'DN-1b {b["status"]}/{b["outcome"]}/{b["iterations"]} f={b["objective"]} wall {b["wall_s"]}')
    tag = ''
    if a['status'] in SUCCESS:
        if b['status'] != a['status']:
            lost.append(p)
            tag = 'LOST'
        else:
            fa, fb = float(a['objective']), float(b['objective'])
            if abs(fa - fb) > 1e-8 * max(1.0, abs(fa)):
                obj_moved.append(p)
                tag = 'OBJECTIVE MOVED'
            elif a['iterations'] != b['iterations']:
                tag = 'iterations moved (reported)'
    elif (a['status'], a['outcome']) != (b['status'], b['outcome']):
        moves.append(p)
        tag = 'NON-SUCCESS MOVE (reported)'
    print(line + ('  <- ' + tag if tag else ''))
opt = sum(1 for r in cur.values() if r['status'] == 'optimal')
print(f'optimal {opt}/23 (PF-1 {sum(1 for r in pf1.values() if r["status"] == "optimal")}/23); lost {lost}; '
      f'objective moved {obj_moved}; non-success moves {moves}')
# DN-1d (../DECLARATION.txt T): identical to PF-1's reading -- no cell lost AND no objective moved.
print('T_PASS' if opt >= 16 and not lost and not obj_moved else 'T_FAIL')
