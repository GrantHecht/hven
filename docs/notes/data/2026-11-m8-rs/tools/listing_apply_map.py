#!/usr/bin/env python3
"""Apply the move maps to an installed-file listing (paths relative to the install prefix) and compare.
usage: listing_apply_map.py BASELINE.txt AFTER.txt MAP [MAP...]   -> exit 0 iff baseline-with-map == after.
Only include/hven/... lines can move; a DELETE removes the line; dirs are not listed (files only)."""
import sys
base, after, maps = sys.argv[1], sys.argv[2], sys.argv[3:]
mv, dl = {}, set()
for m in maps:
    for ln in open(m):
        ln = ln.rstrip('\n')
        if ' -> ' not in ln: continue
        o, n = ln.split(' -> ', 1)
        if not o.startswith('include/hven/'): continue
        if n.startswith('DELETE'): dl.add(o)
        else: mv[o] = n
want = set()
for ln in open(base):
    p = ln.strip()
    if not p: continue
    if p in dl: continue
    want.add(mv.get(p, p))
got = set(l.strip() for l in open(after) if l.strip())
print('baseline %d -> %d after map; after %d' % (len(set(l.strip() for l in open(base) if l.strip())), len(want), len(got)))
for p in sorted(want - got): print('MISSING', p)
for p in sorted(got - want): print('EXTRA  ', p)
ok = want == got
print('LISTING_EQUALS_BASELINE_WITH_MAP' if ok else 'LISTING_DIFFERS')
sys.exit(0 if ok else 1)
