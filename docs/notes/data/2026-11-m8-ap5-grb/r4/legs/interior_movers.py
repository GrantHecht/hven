#!/usr/bin/env python3
"""M8 AP-5 G-S: the interior leg's declared-movers verdict. Compares arm A's and arm B's interior CSVs (every non-wall column, joined on cell_id) and
passes only if the cells that differ are EXACTLY the declared set, each differing in exactly the `status` and `phases` columns, by the declared map
(status stalled -> infeasible; the `:stalled:` token of `phases` -> `:infeasible:`).
usage: interior_movers.py A.csv B.csv OUT.txt cell_id [cell_id ...]"""
import csv
import sys


def rows(path):
    lines = [l for l in open(path, newline='') if not l.startswith('#')]
    r = list(csv.reader(lines))
    return r[0], {x[0]: x for x in r[1:]}


a_path, b_path, out_path, declared = sys.argv[1], sys.argv[2], sys.argv[3], set(sys.argv[4:])
ha, A = rows(a_path)
hb, B = rows(b_path)
msgs, ok = [], True
if ha != hb:
    ok = False
    msgs.append('HEADERS DIFFER')
if set(A) != set(B):
    ok = False
    msgs.append('CELL SETS DIFFER')
wall = ha.index('wall_s')
moved = set()
for cid in sorted(set(A) & set(B)):
    diff = [ha[i] for i in range(len(ha)) if i != wall and A[cid][i] != B[cid][i]]
    if not diff:
        continue
    moved.add(cid)
    exp_phases = A[cid][ha.index('phases')].replace(':stalled:', ':infeasible:')
    good = (set(diff) == {'status', 'phases'} and A[cid][ha.index('status')] == 'stalled' and B[cid][ha.index('status')] == 'infeasible'
            and B[cid][ha.index('phases')] == exp_phases)
    msgs.append(f'{cid}: differs in {diff}: {"as declared" if good else "NOT AS DECLARED"}')
    ok = ok and good
if moved != declared:
    ok = False
    msgs.append(f'MOVER SET MISMATCH: moved {sorted(moved)} declared {sorted(declared)}')
msgs.append(f'cells {len(A)}; movers {len(moved)}; declared {len(declared)}')
msgs.append('MOVERS_VERDICT ' + ('PASS' if ok else 'FAIL'))
open(out_path, 'w').write('\n'.join(msgs) + '\n')
print('\n'.join(msgs))
sys.exit(0 if ok else 1)
