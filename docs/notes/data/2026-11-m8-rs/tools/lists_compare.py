#!/usr/bin/env python3
"""Compare two gtest-list captures (dirs of <exe>.txt, plus ctest-N.txt) EXACTLY, except for one declared class of non-determinism:
value-parameterised tests whose parameter has no printer are named with the object's raw bytes, e.g.
  Battery/SqpQnHs.ConvergesWithoutAHessianCall/16-byte object <03-00 00-00 00-00 00-00 91-88 72-07 00-00 00-00>
whose trailing bytes are a ROW ADDRESS inside the test binary: any change to that binary's .rodata layout (a longer string literal elsewhere
in the same executable) moves it.  Such a name is compared with every `<hex-hex ...>` byte dump replaced by `<bytes>`; the NUMBER of names carrying
a dump, and everything else in the name, must match.  Anything else is a difference.
usage: lists_compare.py <before-dir> <after-dir>"""
import os, re, sys
b, a = sys.argv[1], sys.argv[2]
dump = re.compile(r'<[0-9A-Fa-f]{2}(?:[- ][0-9A-Fa-f]{2})*>')
def load(p):
    return [l.rstrip('\n') for l in open(p)]
def norm(ls):
    n = [dump.sub('<bytes>', l) for l in ls]
    k = sum(1 for l in ls if dump.search(l))
    return sorted(n), k
files = sorted(f for f in os.listdir(os.path.join(b, 'gtest')))
afiles = sorted(os.listdir(os.path.join(a, 'gtest')))
ok = True
if files != afiles:
    print('EXECUTABLE SET DIFFERS', sorted(set(files) ^ set(afiles))); ok = False
tot_dump = 0
for f in files:
    if f not in afiles: continue
    nb, kb = norm(load(os.path.join(b, 'gtest', f))); na, ka = norm(load(os.path.join(a, 'gtest', f)))
    tot_dump += kb
    if nb != na or kb != ka:
        ok = False; print('DIFFERS', f, len(nb), len(na), kb, ka)
nb, kb = norm(load(os.path.join(b, 'ctest-N.txt'))); na, ka = norm(load(os.path.join(a, 'ctest-N.txt')))
if nb != na or kb != ka:
    ok = False; print('DIFFERS ctest-N.txt', len(nb), len(na), kb, ka)
raw_same = all(load(os.path.join(b, 'gtest', f)) == load(os.path.join(a, 'gtest', f)) for f in files if f in afiles)
print('executables %d; gtest names with a raw byte dump: %d; gtest lists raw-identical: %s; ctest -N names with a raw byte dump (normalised, equal count): %d' % (len(files), tot_dump, raw_same, kb))
print('LISTS_IDENTICAL' if ok else 'LISTS_DIFFER')
sys.exit(0 if ok else 1)
