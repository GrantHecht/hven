#!/usr/bin/env python3
"""W5 T8.9r-attrib5 -- the row's OWN instructions, with MKL's first-call clock
calibration named and set aside.

`perf stat` counts the process. A single-row interior process pays, once,
MKL's `dsecnd()` first-call clock calibration (hven calls it deliberately at
src/drivers/solver_init.cpp:27, through
InteriorPointSolver::ensure_solver_initialized,
src/drivers/interior_point_solver.cpp:798 and :5535) -- a WALL-TIMED busy-wait
of about 0.95 s.  Its instruction count therefore measures how fast that loop's
own code runs on the day, not any work the solver does.  IN THE SAMPLED PROFILES
BELOW it is 16.7-27.2 % of an n20000 process and 82.4-84.0 % of an n1000 one
(the `cal %` column is the measurement, per round; an earlier draft of this
docstring quoted single figures for both and they were not what the profiles
read -- corrected at fix round 3).  The leg proper pays it ONCE for 43 rows;
this instrument pays it once PER ROW.

This script reads the sampled per-symbol profiles and reports, per arm and row,
the instructions OUTSIDE the two calibration symbols -- which is the row's own
work plus the process's ordinary setup.

-----------------------------------------------------------------------------
FIX ROUND 3 -- THE INPUTS ARE NOW INSIDE THE ARTIFACT (settler ruling R15;
astra's fix2 review, item 5).

The fix2 version shelled out to `perf report -i <scratch>/perfrec/...`, so the
calibration exclusion could only be reproduced by someone who still had that
scratch directory. It is now read from `perf-report/report/<arm>/<row>-r<n>.
report.txt` -- the saved stdout of

    perf report -i <profile> --no-children --sort symbol --stdio

for each of the 27 profiles -- which is RETAINED HERE, beside the raw
`perf.data` files themselves under `perf-report/data/` (each under 300 KB).
Parsing the saved report and parsing `perf report -q`'s live output give the
same event count and the same symbol shares: the `-q` flag suppresses the
comment block and nothing else, and the comment lines cannot match the symbol
regex. **This file's OUTPUT is byte-identical to the fix2 version's**, which is
what lets `record_analyze.out` stand unrewritten.
"""
import glob
import os
import re
import statistics
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPORTS = os.path.join(HERE, 'perf-report', 'report')
CAL = {'difftime', 'mkl_serv_get_clocks_frequency'}
ARMS = ['510a4bb', '9cebbbe', 'control']
ROWS = ['f7_n20000_bound_neutral-MakeParameter',
        'f7_n20000_bound_neutral-MakeConstraint',
        'f7_n1000_bound_physics-MakeConstraint']

EVENT_COUNT = re.compile(r'# Event count \(approx\.\): (\d+)')
SYMBOL = re.compile(r'\s*([\d.]+)%\s+\[[.k]\] (.*?)\s{2,}')


def profile(path):
    """-> (event_count, {symbol: fraction}) from a SAVED `perf report --stdio`."""
    ec = None
    syms = {}
    with open(path, errors='replace') as fh:
        for line in fh:
            m = EVENT_COUNT.match(line)
            if m:
                ec = int(m.group(1))
                continue
            m = SYMBOL.match(line)
            if m:
                syms[m.group(2).strip()] = float(m.group(1)) / 100.0
    if ec is None:
        raise SystemExit('record_analyze: %s carries no event count' % path)
    return ec, syms


print("event: instructions:u, sampled at one sample per 2 000 000 instructions")
print("calibration symbols set aside:", ", ".join(sorted(CAL)))
print()
print(f"{'row':42s} {'arm':9s} {'rd':2s} {'total e9':>9s} {'cal %':>7s} {'non-cal e9':>11s}")
data = {}
for row in ROWS:
    for arm in ARMS:
        for rd in (1, 2, 3):
            p = f'{REPORTS}/{arm}/{row}-r{rd}.report.txt'
            if not os.path.exists(p):
                continue
            ec, syms = profile(p)
            cal = sum(v for k, v in syms.items() if k in CAL)
            noncal = ec * (1.0 - cal)
            data.setdefault((row, arm), []).append(noncal)
            print(f"{row:42s} {arm:9s} {rd:2d} {ec/1e9:9.4f} {cal*100:7.2f} {noncal/1e9:11.4f}")
print()
print(f"{'row':42s} {'parent e9':>10s} {'culprit e9':>11s} {'control e9':>11s} {'c/p':>9s} {'x/p':>9s}")
for row in ROWS:
    p = statistics.median(data[(row, '510a4bb')])
    c = statistics.median(data[(row, '9cebbbe')])
    x = statistics.median(data[(row, 'control')])
    print(f"{row:42s} {p/1e9:10.4f} {c/1e9:11.4f} {x/1e9:11.4f} {c/p:9.5f} {x/p:9.5f}")
