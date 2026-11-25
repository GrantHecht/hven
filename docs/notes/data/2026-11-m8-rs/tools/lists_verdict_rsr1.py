#!/usr/bin/env python3
"""RS-R1 list verdict (DECLARATION-RS-R1.md, test-object class). usage: lists_verdict_rsr1.py BASE_LISTS AFTER_LISTS
Each *_LISTS dir holds gtest/<exe>.txt (sorted Suite.Test per line) and ctest-N.txt (lists.sh output); BASE is the b02e88eb4 tree (the RS-6a executables), AFTER is this tree.
The ONLY name map is the declared one: suite prefix SqpDriver -> SqpSolver (plain and parameterised-instantiation spellings), applied to BASE names.
Asserts: (1) the union (multiset) over ALL executables of BASE, mapped, equals the union of AFTER, exactly; (2) per executable the mapped BASE list equals the AFTER list, except the two declared moves
(suite SparseBackendLink core->linear; suite RouteReference qp seam -> qp ordinary), and the moved names are exactly those; (3) no name is in two AFTER executables beyond the base's own duplicates (mapped);
(4) the golden rig lists are unchanged; (5) ctest -N name set, mapped, equals AFTER's, cardinality equal; (6) no AFTER name still contains SqpDriver, and no mapped name collides with an existing SqpSolver* suite."""
import re, sys
from collections import Counter
b, a = sys.argv[1:3]
dump = re.compile(r'<[0-9A-Fa-f]{2}(?:[- ][0-9A-Fa-f]{2})*>')
def load(d, exe): return [l.rstrip('\n') for l in open('%s/gtest/%s.txt' % (d, exe))]
def mp(l): return dump.sub('<bytes>', l).replace('SqpDriver', 'SqpSolver')
EXES = ['hven_%s_tests' % d for d in 'core linear model kkt qp warmstart solver trace ipm sqp bench'.split()] + \
       ['hven_%s_seam_tests' % d for d in 'linear kkt qp solver ipm sqp'.split()] + ['hven_linear_alloc_tests']
KEEP = ['hven_golden_rig', 'hven_golden_rig_audit']
ADDED = Counter({'Layering.NoForwardDeclarationGoesUpTheLayerOrder': 1, 'Layering.KnownForwardDeclarationsStillOccur': 1})  # the declared I7 check (hven_core_tests)
MOVES = {('hven_core_tests', 'hven_linear_tests'): 'SparseBackendLink.', ('hven_qp_seam_tests', 'hven_qp_tests'): 'RouteReference.'}
bad = []
# (6) collision check on the base side
base_names = set(l for e in EXES for l in load(b, e))
if any(n.startswith(('SqpSolver', 'Inst/SqpSolver')) or re.match(r'.*/SqpSolver', n) for n in base_names): bad.append('base already has SqpSolver* suites (collision)')
ub = Counter(); ua = Counter()
for e in EXES:
    ub.update(mp(l) for l in load(b, e)); ua.update(mp(l) for l in load(a, e))
print('union: base %d names, after %d names' % (sum(ub.values()), sum(ua.values())))
if ub + ADDED != ua: bad.append(('union differs beyond the declared additions', sorted((ub + ADDED - ua).elements())[:10], sorted((ua - ub - ADDED).elements())[:10]))
if any('SqpDriver' in l for e in EXES for l in load(a, e)): bad.append('after still has SqpDriver names')
moved = {}
for e in EXES:
    lb = Counter(mp(l) for l in load(b, e)); la = Counter(mp(l) for l in load(a, e))
    rem = lb - la; add = la - lb
    print('  %-34s base %5d after %5d  removed %d added %d' % (e, sum(lb.values()), sum(la.values()), sum(rem.values()), sum(add.values())))
    for (src, dst), pre in MOVES.items():
        if e == src and rem: moved[(src, dst)] = moved.get((src, dst), Counter()) + rem
    for (src, dst), pre in MOVES.items():
        pass
for (src, dst), pre in MOVES.items():
    r = moved.get((src, dst), Counter())
    if not r or any(not n.startswith(pre) for n in r): bad.append(('move %s->%s not exactly %s*' % (src, dst, pre), sorted(r)[:5]))
    ad = Counter(mp(l) for l in load(a, dst)) - Counter(mp(l) for l in load(b, dst))
    if ad != r: bad.append(('added in %s differs from removed in %s' % (dst, src), sorted(ad)[:5], sorted(r)[:5]))
    print('declared move %s -> %s (%s*): %d names' % (src, dst, pre, sum(r.values())))
for e in EXES:
    srcs = [s for (s, d) in MOVES if s == e]; dsts = [d for (s, d) in MOVES if d == e]
    if srcs or dsts: continue
    extra = ADDED if e == 'hven_core_tests' else Counter()
    if Counter(mp(l) for l in load(b, e)) + extra != Counter(mp(l) for l in load(a, e)): bad.append((e, 'list changed beyond the declared map'))
seen = {}
for e in EXES:
    for n in set(mp(l) for l in load(a, e)): seen.setdefault(n, []).append(e)
dup = {n for n, v in seen.items() if len(v) > 1}
bs = {}
for e in EXES:
    for n in set(mp(l) for l in load(b, e)): bs.setdefault(n, []).append(e)
bdup = {n for n, v in bs.items() if len(v) > 1}
print('names in more than one after executable: %d (base %d)' % (len(dup), len(bdup)))
if dup != bdup: bad.append(('duplicate-name set differs', sorted(dup ^ bdup)[:10]))
for e in KEEP:
    if load(b, e) != load(a, e): bad.append((e, 'golden-rig list changed'))
def ctest(d):
    L = open(d + '/ctest-N.txt').read().splitlines(); return L
cb = ctest(b); ca = ctest(a)
sb = Counter(mp(l) for l in cb) + ADDED; sa = Counter(l.replace('<bytes>', '<bytes>') for l in (dump.sub('<bytes>', x) for x in ca))
print('ctest -N: base %d (+2 declared), after %d; added %s removed %s' % (len(cb), len(ca), sorted((sa - sb).elements())[:5], sorted((sb - sa).elements())[:5]))
if sa != sb: bad.append(('ctest -N differs beyond the declared additions', sorted((sa - sb).elements())[:10], sorted((sb - sa).elements())[:10]))
if bad: print('LISTS_NOT_AS_DECLARED', bad); sys.exit(1)
print('LISTS_AS_DECLARED')
