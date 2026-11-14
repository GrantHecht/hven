#!/usr/bin/env python3
"""B2-runtime-r5 reading of one mode's leg directory (DECLARATION.txt in this directory).

usage: analyze_r5.py <leg-dir> <mode> | --preflight <table> <expected> | --excluded-count <table>

1. THE STOP CONDITION FIRST: counter identity on every (round, cell) pair. A child's
   --internal-out file is one data row; every field but wall_s (the 14th) must be equal
   as strings. The interior leg's file is a corpus CSV, compared with
   scripts/compare_replay.py (EXACT). Any difference or missing file -> STOP.
   VALIDITY: every process's THROTTLE event count (throttle/) must be 0; else VOID.
2. THE CRITERION: per process, the instructions:u samples (perf record -c 200000) of the
   measured process (comm hven_sqp_corpus), summed over every symbol EXCEPT exactly:
   difftime, difftime@plt, mkl_serv_get_clocks_frequency, __vdso_time,
   __vdso_clock_gettime, time, clock_gettime. Per (arm, cell): the median over the 3
   rounds; per arm: the sum of those medians; B/A in [0.995, 1.005] -> WITHIN.
3. Beside it, never the criterion: the raw perf-stat instructions:u (same aggregation),
   the unfiltered sample sum, and the excluded (wall-clock) samples.
"""
import glob
import os
import re
import statistics
import subprocess
import sys

EXCLUDED = {'difftime', 'difftime@plt', 'mkl_serv_get_clocks_frequency', '__vdso_time',
            '__vdso_clock_gettime', 'time', 'clock_gettime'}
# r3 PARSER FIX: perf appends two '-' columns after the symbol even under -F overhead,sample,sym;
# they are stripped here. Fail closed: a non-blank line that does not match is a parse error.
LINE = re.compile(r'\s*([\d.]+)%\s+(\d+)\s+\[(.)\]\s+(.*?)(?:\s+-\s+-)?\s*$')
here = os.path.dirname(os.path.abspath(__file__))
repo = os.path.abspath(os.path.join(here, '../../../../..'))  # PF-HB: pf-huge-bound/runtime/ is five levels below the root, as b8-riders/runtime/ was (checked by chain1's pre-flight)
compare = os.path.join(repo, 'scripts/compare_replay.py')
ROUNDS = (1, 2, 3)


def syms(path):
    d = {}
    for line in open(path):
        if not line.strip():
            continue
        m = LINE.match(line)
        if not m:
            raise SystemExit('PARSE ERROR (fail closed) %s: %r' % (path, line[:100]))
        d[m.group(4)] = d.get(m.group(4), 0) + int(m.group(2))
    return d


def sums(d):
    tot = sum(d.values())
    ex = sum(v for k, v in d.items() if k in EXCLUDED)
    return tot, ex, tot - ex


if sys.argv[1] == '--excluded-count':
    print(sums(syms(sys.argv[2]))[1])
    sys.exit(0)
if sys.argv[1] == '--preflight':
    tot, ex, fl = sums(syms(sys.argv[2]))
    want = dict(l.strip().split('=') for l in open(sys.argv[3]) if '=' in l and not l.startswith('#'))
    ok = (tot, ex, fl) == (int(want['total']), int(want['excluded']), int(want['filtered']))
    print('FIXTURE PRE-FLIGHT: got total=%d excluded=%d filtered=%d; expected total=%s excluded=%s '
          'filtered=%s -> %s' % (tot, ex, fl, want['total'], want['excluded'], want['filtered'],
                                  'PASS' if ok else 'FAIL'))
    sys.exit(0 if ok else 1)
leg, mode = sys.argv[1], sys.argv[2]


def stat_instructions(path):
    for line in open(path):
        parts = line.strip().split(',')
        if len(parts) >= 3 and parts[2].startswith('instructions'):
            return float(parts[0])
    return float('nan')


tags = sorted({os.path.basename(p)[len('%s-r1-A-' % mode):-4]
               for p in glob.glob(os.path.join(leg, 'syms', '%s-r1-A-*.txt' % mode))})

# 1. the stop condition
mismatch, checked = 0, 0


def data_rows(path):
    return [l.rstrip('\n').split(',') for l in open(path)
            if l.strip() and not l.startswith('#') and not l.startswith('cell_id,')]


for tag in tags:
    for r in ROUNDS:
        a = os.path.join(leg, 'raw', '%s-r%d-A-%s.csv' % (mode, r, tag))
        b = os.path.join(leg, 'raw', '%s-r%d-B-%s.csv' % (mode, r, tag))
        checked += 1
        if not (os.path.exists(a) and os.path.exists(b)):
            print('MISSING CSV', a, b)
            mismatch += 1
            continue
        if mode == 'interior':
            res = subprocess.run([sys.executable, compare, a, b, 'interior r%d' % r],
                                 capture_output=True, text=True)
            head = res.stdout.splitlines()[0] if res.stdout else res.stderr.strip()
            print(head)
            if res.returncode != 0 or 'differences: 0' not in head:
                mismatch += 1
            continue
        ra, rb = data_rows(a), data_rows(b)
        if len(ra) != len(rb) or not ra:
            print('ROW COUNT DIFF', a, len(ra), len(rb))
            mismatch += 1
            continue
        for x, y in zip(ra, rb):
            diffs = [i for i in range(max(len(x), len(y)))
                     if i != 13 and (i >= len(x) or i >= len(y) or x[i] != y[i])]
            if diffs:
                print('COUNTER DIFF %s r%d %s cols %s' % (mode, r, tag, diffs[:8]))
                mismatch += 1
                break
print('STOP CONDITION: %d (round,cell) pairs compared, %d with a difference or missing -> %s'
      % (checked, mismatch, 'COUNTER IDENTITY HOLDS' if mismatch == 0 else 'STOP'))
if mismatch:
    sys.exit(2)

# validity: no throttled process
throttled = []
for p in sorted(glob.glob(os.path.join(leg, 'throttle', '*.txt'))):
    if open(p).read().strip() != '0':
        throttled.append(os.path.basename(p))
nproc = len(glob.glob(os.path.join(leg, 'throttle', '*.txt')))
print('VALIDITY: %d processes, %d throttled -> %s' % (nproc, len(throttled),
      'VALID' if not throttled and nproc == 6 * len(tags) else 'VOID %s' % throttled[:5]))
if throttled or nproc != 6 * len(tags):
    sys.exit(3)

# 2./3. the readings
per = {}  # (arm, tag) -> list over rounds of (filtered, total, excluded, raw)
for tag in tags:
    for arm in 'AB':
        runs = []
        for r in ROUNDS:
            base = '%s-r%d-%s-%s' % (mode, r, arm, tag)
            s = syms(os.path.join(leg, 'syms', base + '.txt'))
            total, excl, _ = sums(s)
            raw = stat_instructions(os.path.join(leg, 'stat', base + '.txt'))
            runs.append((total - excl, total, excl, raw))
        per[(arm, tag)] = runs


# r5 FILTER-LIVENESS GUARD, INTERIOR MODE ONLY (the wall-clock spin sits behind the IPM driver's
# ensure_solver_initialized() call_once, which the SQP modes never reach): per arm, the excluded
# samples summed over all the arm's processes and rounds must be > 0, else the leg is VOID. For
# walk/ssn/ipm there is no guard; the excluded count is reported (expected 0).
for arm in 'AB':
    exsum = sum(run[2] for tag in tags for run in per[(arm, tag)])
    if mode == 'interior':
        print('FILTER LIVENESS arm %s: excluded samples over all processes and rounds = %d -> %s'
              % (arm, exsum, 'LIVE' if exsum > 0 else 'VOID'))
        if exsum <= 0:
            sys.exit(4)
    else:
        print('EXCLUDED COUNT arm %s (no liveness guard in mode %s): %d' % (arm, mode, exsum))


def agg(arm, i):
    return sum(statistics.median(run[i] for run in per[(arm, tag)]) for tag in tags)


names = ['filtered samples (CRITERION)', 'all samples', 'excluded (wall-clock) samples',
         'raw perf-stat instructions:u']
print()
print('mode %s, %d cell(s), 3 rounds (A->B, B->A, A->B); per cell the median over rounds, '
      'summed' % (mode, len(tags)))
for i, name in enumerate(names):
    a, b = agg('A', i), agg('B', i)
    print('  %-32s A=%.6e B=%.6e B/A=%s' % (name, a, b, ('%.6f' % (b / a)) if a else 'n/a'))
print()
print('per-round filtered B/A (informational):')
for k, r in enumerate(ROUNDS):
    a = sum(per[('A', t)][k][0] for t in tags)
    b = sum(per[('B', t)][k][0] for t in tags)
    print('  round %d: B/A=%.6f' % (r, b / a))
worst = max(tags, key=lambda t: abs(statistics.median(x[0] for x in per[('B', t)]) /
                                     statistics.median(x[0] for x in per[('A', t)]) - 1.0))
wa = statistics.median(x[0] for x in per[('A', worst)])
wb = statistics.median(x[0] for x in per[('B', worst)])
print('worst per-cell filtered ratio: %.5f (%s)' % (wb / wa, worst))
spread = max((max(x[0] for x in per[(arm, t)]) - min(x[0] for x in per[(arm, t)])) /
             statistics.median(x[0] for x in per[(arm, t)]) for t in tags for arm in 'AB')
print('largest within-arm round-to-round filtered spread: %.5f' % spread)
ratio = agg('B', 0) / agg('A', 0)
print('MODE %s FILTERED B/A = %.6f -> %s' % (mode, ratio,
      'WITHIN [0.995, 1.005]' if 0.995 <= ratio <= 1.005 else 'OUTSIDE [0.995, 1.005]'))
sys.exit(0 if 0.995 <= ratio <= 1.005 else 1)
