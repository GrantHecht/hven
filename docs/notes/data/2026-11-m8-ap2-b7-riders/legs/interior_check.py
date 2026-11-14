#!/usr/bin/env python3
"""AP-2 B7: the interior leg's difference list and criterion (DECLARATION.txt, P13).

usage: interior_check.py --list  <A.csv> <B.csv>                 (writes the full list to stdout)
       interior_check.py --check <declared.txt> <A.csv> <B.csv>  (exit 0 iff as declared)

A = arm A (the B7 base), B = the B7 head; both corpus interior CSVs (bench --engine interior, the 27
replay cells x the three fixed-variable treatments, at the defaults). Rows are joined on cell_id; every
column but wall_s is compared as a string. The list is COMPLETE (scripts/compare_replay.py prints at most
200 differences; this does not truncate): one line `<cell_id> <column> A=<a> B=<b>` per differing pair.

--check PASSES iff
  * the SET of differing (row, column) pairs is the declared set, exactly -- no pair missing, none extra;
  * every NON-FLOAT column's A and B values are the declared ones, as strings (iteration, factorization
    and solve counts, the phase record, status, stop reason, sizes);
  * the same rows exist in both arms as declared.
The float columns (objective and residual class) are re-derived and recorded; their membership is
asserted, their values are not (CLAUDE.md section 7: residual-class columns are not asserted
cross-process by byte equality).
"""
import sys

FLOATS = {'obj_val', 'kkt_inf', 'barr_inf', 'econ_inf', 'icon_inf', 'stationarity', 'feasibility_e',
          'feasibility_i', 'complementarity'}


def rows(path):
    head, out = None, {}
    for line in open(path):
        line = line.rstrip('\n')
        if not line or line.startswith('#'):
            continue
        f = line.split(',')
        if f[0] == 'cell_id':
            head = f
            continue
        if f[0] in out:
            raise SystemExit('DUPLICATE ROW %s in %s' % (f[0], path))
        out[f[0]] = f
    if head is None:
        raise SystemExit('NO HEADER in %s' % path)
    return head, out


def diff(a_path, b_path):
    ha, ra = rows(a_path)
    hb, rb = rows(b_path)
    if ha != hb:
        raise SystemExit('SCHEMA MISMATCH')
    if sorted(ra) != sorted(rb):
        raise SystemExit('ROW SET MISMATCH only-A %s only-B %s'
                         % (sorted(set(ra) - set(rb)), sorted(set(rb) - set(ra))))
    out = {}
    for k in sorted(ra):
        for i, c in enumerate(ha):
            if c == 'wall_s':
                continue
            if ra[k][i] != rb[k][i]:
                out[(k, c)] = (ra[k][i], rb[k][i])
    return len(ra), len(ha) - 1, out


def main():
    if len(sys.argv) == 4 and sys.argv[1] == '--list':
        n, cols, d = diff(sys.argv[2], sys.argv[3])
        print('# rows %d columns %d differing pairs %d (non-float %d)'
              % (n, cols, len(d), sum(1 for k in d if k[1] not in FLOATS)))
        for (k, c), (a, b) in sorted(d.items()):
            print('%s %s A=%s B=%s' % (k, c, a, b))
        return 0
    if len(sys.argv) == 5 and sys.argv[1] == '--check':
        declared = {}
        for line in open(sys.argv[2]):
            if line.startswith('#') or not line.strip():
                continue
            k, c, a, b = line.rstrip('\n').split(' ', 3)
            declared[(k, c)] = (a[2:], b[2:])
        n, cols, d = diff(sys.argv[3], sys.argv[4])
        missing = sorted(set(declared) - set(d))
        extra = sorted(set(d) - set(declared))
        value = sorted(k for k in set(declared) & set(d)
                       if k[1] not in FLOATS and declared[k] != d[k])
        for k in missing:
            print('MISSING', k, declared[k])
        for k in extra:
            print('EXTRA', k, d[k])
        for k in value:
            print('VALUE', k, 'declared', declared[k], 'observed', d[k])
        ok = not missing and not extra and not value
        print('rows %d columns %d pairs declared %d observed %d missing %d extra %d value %d -> %s'
              % (n, cols, len(declared), len(d), len(missing), len(extra), len(value),
                 'INTERIOR_AS_DECLARED' if ok else 'INTERIOR_DIFFERS_FROM_DECLARATION'))
        return 0 if ok else 1
    raise SystemExit(__doc__)


sys.exit(main())
