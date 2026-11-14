#!/usr/bin/env python3
"""M8 AP-5 G1 -- THE DECLARED COLUMN MAP (../DECLARATION.txt, section COLUMN MAP).

Rewrites the `stop_reason` column of an interior-point corpus CSV (the corpus
binary's --engine interior schema, 31 columns) from the retired
to_string(IpmStopReason) spelling to the to_string(StopReason) spelling G1 ships,
and copies every other byte unchanged (comment lines, header, every other
column). The map is total over the old vocabulary; a value outside it is an
error (fail closed), never passed through.

usage: stop_reason_map.py IN.csv OUT.csv
"""
import sys

MAP = {
    'none': 'none',
    'iteration_cap': 'none',          # the status (max_iter) names it
    'work_limit': 'none',             # the status names it
    'time_limit': 'none',             # the status names it
    'interrupted': 'callback_stop',   # IpmStopReason::kInterrupted -> StopReason::kCallbackStop
    'restoration_locally_infeasible': 'restoration_locally_infeasible',
    'stage_stalled': 'stage_stalled',
    'non_finite_start': 'non_finite_start',
    'non_finite_iterate': 'non_finite_iterate',
    'evaluation_failure': 'evaluation_failure',
    'exception': 'exception',
}


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = sys.argv[1], sys.argv[2]
    out = []
    col = None
    mapped = 0
    for line in open(src, newline=''):
        body = line.rstrip('\n')
        if not body or body.startswith('#'):
            out.append(line)
            continue
        fields = body.split(',')
        if col is None:
            if fields[0] != 'cell_id' or fields.count('stop_reason') != 1:
                sys.exit(f'MAP_FAIL: {src}: header without exactly one stop_reason column')
            col = fields.index('stop_reason')
            out.append(line)
            continue
        old = fields[col]
        if old not in MAP:
            sys.exit(f'MAP_FAIL: {src}: stop_reason value {old!r} outside the declared map')
        if MAP[old] != old:
            mapped += 1
        fields[col] = MAP[old]
        out.append(','.join(fields) + ('\n' if line.endswith('\n') else ''))
    if col is None:
        sys.exit(f'MAP_FAIL: {src}: no header row')
    with open(dst, 'w', newline='') as f:
        f.writelines(out)
    print(f'MAPPED {src} -> {dst}: {mapped} value(s) rewritten')


if __name__ == '__main__':
    main()
