#!/usr/bin/env python3
# M8 DN-1 A legs (../DECLARATION-dn1legs.txt): the replay list -- every (cell, matrix) row of
# ../../2026-11-m8-dn1-design/matrices.csv.gz mapped to DN-0's raw dump, fail closed: the CSV's 5501 rows and
# the dump directory's mat_*.bin files must be the same set.
# usage: a_list.py MATRICES_CSV_GZ DUMP_DIR OUT_LIST
import csv
import glob
import gzip
import os
import sys

csv_path, dump, out = sys.argv[1:4]
rows = list(csv.DictReader(gzip.open(csv_path, 'rt')))
want = sorted((r['cell'], r['matrix']) for r in rows)
have = sorted((os.path.basename(os.path.dirname(p))[:-len('.dump')], os.path.basename(p))
              for p in glob.glob(os.path.join(dump, '*.dump', 'mat_*.bin')))
if len(want) != 5501 or want != have:
    print(f'A_LIST_REFUSED csv {len(want)} dump {len(have)} only-csv {len(set(want) - set(have))} '
          f'only-dump {len(set(have) - set(want))}')
    sys.exit(3)
with open(out, 'w') as f:
    for cell, mat in want:
        f.write(f'{cell}\t{os.path.join(dump, cell + ".dump", mat)}\n')
print(f'A_LIST_OK {len(want)} matrices, {len(set(c for c, _ in want))} cells')
