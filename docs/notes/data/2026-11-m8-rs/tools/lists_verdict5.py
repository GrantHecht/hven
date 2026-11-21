#!/usr/bin/env python3
"""RS-5 declared list delta (DECLARATION-RS5.md G5). usage: lists_verdict5.py BASE_LISTS AFTER_LISTS
hven_tests gtest list: exactly +4 Layering.* names, nothing removed; every other gtest list byte-identical;
ctest -N: exactly +5 (the 4 + HeaderSelfContained), nothing removed, after normalising the byte-dump class
(a gtest name that embeds an object's raw bytes, whose address bytes differ per process: counted, not compared)."""
import os, re, sys, glob
b, a = sys.argv[1:3]
bad = []
for f in sorted(glob.glob(b + '/gtest/*.txt')):
    n = os.path.basename(f)
    x, y = open(f).read().splitlines(), open(a + '/gtest/' + n).read().splitlines()
    if n == 'hven_tests.txt':
        sx, sy = set(x), set(y)
        if sx - sy or len(sy - sx) != 4 or not all(t.startswith('Layering.') for t in sy - sx):
            bad.append((n, sorted(sx - sy), sorted(sy - sx)))
        print('hven_tests: +%s' % sorted(sy - sx))
    elif x != y:
        bad.append((n, 'differs'))
def norm(p):
    L = open(p).read().splitlines()
    dump = [l for l in L if 'byte object' in l]
    return set(l for l in L if 'byte object' not in l), len(dump)
nb, db = norm(b + '/ctest-N.txt'); na, da = norm(a + '/ctest-N.txt')
print('ctest -N byte-dump class: %d before, %d after' % (db, da))
added, removed = sorted(na - nb), sorted(nb - na)
print('ctest -N added:', added); print('ctest -N removed:', removed)
if removed or len(added) != 5 or db != da: bad.append(('ctest-N', added, removed))
if bad: print('LISTS_NOT_AS_DECLARED', bad); sys.exit(1)
print('LISTS_AS_DECLARED')
