#!/usr/bin/env python3
"""M8 AP-5 G-S -- THE DECLARED COLUMN MAP (../DECLARATION.md, section COLUMN MAP).

Rewrites an interior-point corpus CSV (the corpus binary's --engine interior schema) through the declared status event: a row whose
stop_reason is restoration_locally_infeasible and whose status is `stalled` reads status `infeasible`, and the `stalled` verdict token in its
`phases` column (`<label>:<status>:<iterations>`) reads `infeasible`. Every other byte is copied unchanged (comment lines, header, every other
column and row). Fail closed: a restoration_locally_infeasible row whose status is not `stalled`, or whose phases column lacks the token, is an error.
A `stalled` row with any other stop_reason (stage_stalled) is NOT touched and is counted.

usage: status_map.py IN.csv OUT.csv
"""
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    out, col, moved, kept_stalled = [], None, [], 0
    for line in open(src, newline=''):
        body = line.rstrip('\n')
        if not body or body.startswith('#'):
            out.append(line)
            continue
        f = body.split(',')
        if col is None:
            for need in ('status', 'stop_reason', 'phases'):
                if f.count(need) != 1:
                    sys.exit(f'MAP_FAIL: {src}: header without exactly one {need} column')
            col = {k: f.index(k) for k in ('status', 'stop_reason', 'phases')}
            out.append(line)
            continue
        st, sr, ph = f[col['status']], f[col['stop_reason']], f[col['phases']]
        if sr == 'restoration_locally_infeasible':
            if st != 'stalled' or ':stalled:' not in ph:
                sys.exit(f'MAP_FAIL: {src}: row {f[0]} has stop_reason restoration_locally_infeasible with status {st!r}, phases {ph!r}')
            f[col['status']] = 'infeasible'
            f[col['phases']] = ph.replace(':stalled:', ':infeasible:')
            moved.append(f[0])
        elif st == 'stalled':
            kept_stalled += 1
        out.append(','.join(f) + ('\n' if line.endswith('\n') else ''))
    if col is None:
        sys.exit(f'MAP_FAIL: {src}: no header row')
    with open(dst, 'w', newline='') as fh:
        fh.writelines(out)
    print(f'MAPPED {src} -> {dst}: {len(moved)} row(s): ' + ' '.join(moved) + f'; stalled rows left alone: {kept_stalled}')


main()
