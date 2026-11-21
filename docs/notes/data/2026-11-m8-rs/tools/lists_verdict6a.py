#!/usr/bin/env python3
"""RS-6a list verdict (DECLARATION-RS6a.md G5). usage: lists_verdict6a.py BASE_LISTS AFTER_LISTS
Each *_LISTS dir holds gtest/<exe>.txt (sorted Suite.Test per line) and ctest-N.txt (lists.sh output). Byte-dump names (value-parameterised tests whose parameter has no printer; the
bytes are a row address) are compared with the dump replaced by <bytes>, and their number must match (as lists_compare.py).
Asserts: (1) multiset union over the ordinary base executables == union over the ordinary after executables; (2) the same for the seam+alloc split;
(3) no gtest name is in two after executables (a name is in exactly as many after executables as base ones: counts per name equal in the union, and
disjointness is reported); (4) every executable not retired (golden rig, audit) has an identical list; (5) ctest -N name set identical. Prints each after executable's count."""
import os, re, sys
from collections import Counter
b, a = sys.argv[1:3]
dump = re.compile(r'<[0-9A-Fa-f]{2}(?:[- ][0-9A-Fa-f]{2})*>')
def load(d, exe): return [l.rstrip('\n') for l in open('%s/gtest/%s.txt' % (d, exe))]
def nm(ls): return [dump.sub('<bytes>', l) for l in ls]
BASE_ORD = ['hven_tests', 'hven_interior_tests', 'hven_drivers_tests', 'hven_model_tests', 'hven_sqp_tests']
BASE_SEAM = ['hven_fault_injection_tests', 'hven_ipqp_seam_tests', 'hven_qp_walk_seam_tests', 'hven_sqp_seam_tests', 'hven_dense_alloc_tests']
AFT_ORD = ['hven_%s_tests' % d for d in 'core linear model kkt qp warmstart solver trace ipm sqp bench'.split()]
AFT_SEAM = ['hven_%s_seam_tests' % d for d in 'linear kkt qp solver ipm sqp'.split()] + ['hven_linear_alloc_tests']
KEEP = ['hven_golden_rig', 'hven_golden_rig_audit']
bad = []
def union(d, exes):
    c = Counter(); k = 0
    for e in exes:
        ls = load(d, e); k += sum(1 for l in ls if dump.search(l)); c.update(nm(ls))
    return c, k
for tag, be, ae in (('ordinary', BASE_ORD, AFT_ORD), ('seam+alloc', BASE_SEAM, AFT_SEAM)):
    cb, kb = union(b, be); ca, ka = union(a, ae)
    print('%s: base %d names (%d byte-dump), after %d names (%d byte-dump)' % (tag, sum(cb.values()), kb, sum(ca.values()), ka))
    if cb != ca or kb != ka:
        bad.append((tag, 'union differs', sorted((cb - ca).elements())[:10], sorted((ca - cb).elements())[:10]))
seen = {}
for e in AFT_ORD + AFT_SEAM:
    ls = load(a, e); print('  %-34s %5d tests' % (e, len(ls)))
    for n in nm(ls): seen.setdefault(n, []).append(e)
dup = {n: v for n, v in seen.items() if len(v) > 1}
print('names in more than one after executable: %d' % len(dup))
basedup = Counter()
for e in BASE_ORD + BASE_SEAM:
    for n in set(nm(load(b, e))): basedup[n] += 1
bdup = {n for n, c in basedup.items() if c > 1}
if set(dup) != bdup: bad.append(('duplicate-name set differs from the base', sorted(set(dup) ^ bdup)[:10]))
for e in KEEP:
    if load(b, e) != load(a, e): bad.append((e, 'golden-rig list changed'))
def ctest(d):
    L = open(d + '/ctest-N.txt').read().splitlines()
    return set(dump.sub('<bytes>', l) for l in L), sum(1 for l in L if dump.search(l)), len(L)
sb, db, nb = ctest(b); sa, da, na = ctest(a)
print('ctest -N: base %d names, after %d names; added %s removed %s; byte-dump %d/%d' % (nb, na, sorted(sa - sb)[:5], sorted(sb - sa)[:5], db, da))
if sa != sb or db != da or nb != na: bad.append(('ctest -N differs', sorted(sa - sb)[:10], sorted(sb - sa)[:10]))
if bad: print('LISTS_NOT_AS_DECLARED', bad); sys.exit(1)
print('LISTS_AS_DECLARED')
