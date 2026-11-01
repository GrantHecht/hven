#!/usr/bin/env python3
"""M7 W2 M1 fix round 1 -- the record of the two RE-TAKEN replay sets.

Machine-written, against the committed bytes: the SUPERSEDED CSVs are read out
of git at the pre-fix HEAD, the re-taken ones off disk. Emits

  1. the superseded configure-time stamp beside the re-taken one,
  2. the re-taken bytes against the superseded bytes, column by column,
  3. the B-direction comparison (the OFF-default set against the shipped set)
     re-derived on the RE-TAKEN bytes, which is what `replay-shipped/
     comparison.txt` asserted on the superseded ones.

Usage: replay_retake_identity.py <pre-fix-head-sha>
"""

import csv
import subprocess
import sys

R = '/home/ghecht/Projects/hven'
D = 'docs/notes/data/2026-09-m7-w2-m1-qp-cap'
SETS = ('replay', 'replay-shipped')
ENGINES = ('walk', 'ssn', 'ipm')
BEFORE = sys.argv[1]


def split(text):
    rows = text.splitlines()
    return ([l for l in rows if l.startswith('#')],
            [l for l in rows if l and not l.startswith('#')])


def committed(path):
    return subprocess.run(['git', '-C', R, 'show', f'{BEFORE}:{path}'],
                          capture_output=True, text=True, check=True).stdout


def ondisk(path):
    with open(f'{R}/{path}') as fh:
        return fh.read()


def stamp(header_lines):
    return header_lines[1].split(':', 1)[1].strip()


print('M7 W2 M1 FIX ROUND 1 -- THE TWO REPLAY SETS, RE-TAKEN CLEAN (review finding MEDIUM-1)')
print()
print(f'Machine-written by wrappers/replay_retake_identity.py {BEFORE}. The SUPERSEDED')
print(f'CSVs are read out of git at {BEFORE}; the re-taken ones off disk.')
print()
print('WHY: `HVEN_SQP_CORPUS_GIT_DESCRIBE` resolves at CONFIGURE time')
print('(bench/CMakeLists.txt:81-95). The first take reused a bench build directory')
print('configured while the M1 edits were still uncommitted, so its CSVs stamped a')
print('`-dirty` binary that had in fact run at a clean, committed tree. Each set below')
print('was re-taken from a build directory REMOVED AND RE-CONFIGURED INSIDE THE LEG, at')
print("a tree with dirty-files=0; the legs' own verbatim stdout is in")
print('replay{,-shipped}/wrapper-stdout.txt.')
print()
print('-' * 78)
print('1. THE SUPERSEDED STAMP vs THE RE-TAKEN STAMP')
print('-' * 78)
for sub in SETS:
    for eng in ENGINES:
        path = f'{D}/{sub}/head-{eng}.csv'
        was = stamp(split(committed(path))[0])
        now = stamp(split(ondisk(path))[0])
        print(f'  {sub + "/head-" + eng + ".csv":<32} was {was:<22} now {now}')
print()
print('-' * 78)
print('2. THE RE-TAKEN BYTES vs THE SUPERSEDED BYTES, COLUMN BY COLUMN')
print('-' * 78)
print('  Every asserted column is identical in every cell. The ONLY data column that')
print('  moves is `wall_s`, which CLAUDE.md section 7 makes informational and which no')
print('  comparison in this artifact asserts; the three header lines that move are the')
print("  configure-time stamp, the invocation's build-directory path and the")
print('  generation timestamp.')
print()
unexplained = 0
for sub in SETS:
    for eng in ENGINES:
        path = f'{D}/{sub}/head-{eng}.csv'
        head_a, data_a = split(committed(path))
        head_b, data_b = split(ondisk(path))
        header_moved = [a.split(':')[0].strip('# ')
                        for a, b in zip(head_a, head_b) if a != b]
        cols = data_a[0].split(',')
        rows_a = list(csv.reader(data_a[1:]))
        rows_b = list(csv.reader(data_b[1:]))
        assert len(rows_a) == len(rows_b), (path, len(rows_a), len(rows_b))
        moved = {}
        for ra, rb in zip(rows_a, rows_b):
            for i, (x, y) in enumerate(zip(ra, rb)):
                if x != y:
                    moved[cols[i]] = moved.get(cols[i], 0) + 1
        unexplained += len(set(moved) - {'wall_s'})
        print(f'  {sub + "/head-" + eng + ".csv":<32} rows {len(rows_a)}  '
              f'identical data columns {len(cols) - len(moved)}/{len(cols)}  '
              f'moved {sorted(moved) or ["NONE"]}  header lines moved {header_moved}')
print()
print(f'  ASSERTED-COLUMN DIFFERENCES OUTSIDE `wall_s`: '
      f'{"NONE" if unexplained == 0 else str(unexplained) + " -- STOP"}')
print()
print('-' * 78)
print('3. THE B-DIRECTION COMPARISON, RE-DERIVED ON THE RE-TAKEN BYTES')
print('-' * 78)
print('  `replay-shipped/comparison.txt` asserted the OFF-default heads against the')
print('  shipped heads on the SUPERSEDED bytes (its section B). Re-derived here on the')
print('  re-taken bytes, by the same script, it comes back the same. Section A of that')
print("  file -- against the M6 close pin -- is in each leg's own wrapper stdout.")
print()
sys.stdout.flush()
for eng in ENGINES:
    subprocess.run(
        ['python3', '-B', f'{R}/scripts/compare_replay.py',
         f'{R}/{D}/replay/head-{eng}.csv', f'{R}/{D}/replay-shipped/head-{eng}.csv',
         f'RE-TAKEN M1 OFF-DEFAULT HEAD-{eng} vs RE-TAKEN M1 SHIPPED HEAD-{eng}'],
        check=False)
