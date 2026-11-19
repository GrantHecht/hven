#!/usr/bin/env python3
"""RS-6a compile-flags diff over EVERY translation unit (DECLARATION-RS6a.md G3). Source paths do not change in RS-6a, so a TU is keyed by its source path and its KIND:
  ord    an ordinary TU (library, tests, bench, examples, golden rig, header-selfcheck): compared as a multiset of normalised flag lists per source;
  seam   a TU compiled with -DHVEN_TESTING=1 (a seam executable's recompiled library TU or test source): the SET of distinct flag lists per source, before vs after
         (the retired seam targets and the new ones recompile overlapping library TUs, in a different number of executables);
  unity  a Unity/unity_N_cxx.cxx batch TU: the SET of distinct flag lists over the class (batches re-compose; the files in them are not individually visible).
Normalisation: the worktree root -> '@', every '@/...' path put in normal form (so tests/solver/../../src and tests/trace/../../src spell one path), provenance stamps (-D*GIT_DESCRIBE, -D*RIG_COMMIT) -> '<git-stamp>',
-o/-MF/-MT/-MQ values and -c/-MD dropped, optimisation tokens (-O0..-O3, -Os, -Oz) removed from the multiset compare and instead the EFFECTIVE level (the last such token) must be equal.
A TU's flag list may differ only by the declared token classes, tallied per class: `inc` = `-I@/tests` (the tests include root) added or removed; `def` = a -D token whose NAME is in DECLARED_DEFS (a per-target define that moved to the executable hosting its source).
Anything else is a violation. usage: flags_diff6a.py BEFORE.json AFTER.json --root-before DIR --root-after DIR [--allow-source-only-after REGEX]"""
import json, shlex, sys, re, argparse, posixpath
from collections import Counter, defaultdict
ap = argparse.ArgumentParser()
ap.add_argument('before'); ap.add_argument('after')
ap.add_argument('--root-before', required=True); ap.add_argument('--root-after', required=True)
ap.add_argument('--allow-only-after', action='append', default=[], help='regex of a source allowed to exist only after')
a = ap.parse_args()
DECLARED_DEFS = re.compile(r'^-D(HVEN_VERSION_STRING|HVEN_INCLUDE_ROOT|HVEN_SRC_ROOT|HVEN_DN1_FIXTURES|HVEN_GOLDEN_DIR|HVEN_TRACE_FIXTURE_DIR|HVEN_A4_GATE_CSV|HVEN_A4_GATE_MU1E2_CSV|HVEN_QP_E2D4_FIXTURES|'
                           r'HVEN_SQP_SOLVER_SOURCE|HVEN_SQP_[A-Z0-9_]*_CSV|HVEN_SQP_CORPUS_BINARY)=')
OPT = re.compile(r'^-O([0-3sz]|fast)$')
def load(p, root):
    root = root.rstrip('/')
    out = defaultdict(list)
    for e in json.load(open(p)):
        toks = shlex.split(e.get('command') or ' '.join(e['arguments']))
        f = e['file']
        rel = f[len(root) + 1:] if f.startswith(root + '/') else f
        keep, i = [], 1
        while i < len(toks):
            t = toks[i]
            if t in ('-o', '-MF', '-MT', '-MQ'): i += 2; continue
            if t in ('-c', '-MD', '-MMD'): i += 1; continue
            if t == f: i += 1; continue
            keep.append(t); i += 1
        norm = []
        for t in keep:
            t = t.replace(root, '@')
            t = re.sub(r'^(-DHVEN_[A-Z0-9_]*(?:GIT_DESCRIBE|RIG_COMMIT)=).*$', r'\1<git-stamp>', t)
            t = re.sub(r'@/([\w./+-]+)', lambda m: '@/' + posixpath.normpath(m.group(1)), t)
            norm.append(t)
        eff = [t for t in norm if OPT.match(t)]
        flags = tuple(sorted(t for t in norm if not OPT.match(t)))
        kind = 'unity' if re.search(r'/Unity/unity_\d+_cxx\.cxx$', rel) else ('seam' if '-DHVEN_TESTING=1' in norm else 'ord')
        key = 'UNITY' if kind == 'unity' else rel
        out[(key, kind)].append((flags, eff[-1] if eff else None, rel))
    return out
B, A = load(a.before, a.root_before), load(a.after, a.root_after)
viol, tally, nboth = [], Counter(), 0
def classify(removed, added):
    bad, cls = [], []
    for t in removed + added:
        side = '-' if t in removed else '+'
        if t == '-I@/tests': cls.append('inc' + side)
        elif DECLARED_DEFS.match(t): cls.append('def' + side)
        else: bad.append(side + t)
    return bad, cls
def cmp_lists(key, lb, la, opt_b, opt_a):
    rb, ra = list(lb), list(la)
    removed = [t for t in rb if t not in ra]; added = [t for t in ra if t not in rb]
    # multiset (a token repeated) handled by Counter difference
    cb, ca = Counter(lb), Counter(la)
    removed = list((cb - ca).elements()); added = list((ca - cb).elements())
    bad, cls = classify(removed, added)
    if opt_b != opt_a: bad.append('effective optimisation level %s -> %s' % (opt_b, opt_a))
    return bad, cls
for key in sorted(set(B) | set(A)):
    if key not in B:
        if not any(re.search(r, key[0]) for r in a.allow_only_after): viol.append('ONLY-AFTER %s %s' % key)
        continue
    if key not in A: viol.append('ONLY-BEFORE %s %s' % key); continue
    nboth += 1
    kind = key[1]
    eb, ea = B[key], A[key]
    if kind == 'ord':
        if len(eb) != len(ea): viol.append('COUNT %s: %d before, %d after' % (key[0], len(eb), len(ea))); continue
        ob = sorted(eb, key=lambda x: x[0]); oa = sorted(ea, key=lambda x: x[0])
        # pair greedily by smallest delta
        used = set()
        for fb, optb, _ in ob:
            best = None
            for j, (fa, opta, rel) in enumerate(oa):
                if j in used: continue
                bad, cls = cmp_lists(key, fb, fa, optb, opta)
                score = (len(bad), len(cls))
                if best is None or score < best[0]: best = (score, j, bad, cls, rel)
            used.add(best[1])
            if best[2]: viol.append('FLAGS %s: %s' % (key[0], best[2]))
            for c in best[3]: tally[c] += 1
    else:
        sb = {(f, o) for f, o, _ in eb}; sa = {(f, o) for f, o, _ in ea}
        # every after list must be explained by some before list through declared classes, and vice versa
        for fa, oa_, rel in ea:
            ok = False; keepbad = None
            for fb, ob_, _ in eb:
                bad, cls = cmp_lists(key, fb, fa, ob_, oa_)
                if not bad: ok = True; [tally.update([('%s:%s' % (kind, c))]) for c in cls]; break
                keepbad = keepbad or bad
            if not ok: viol.append('FLAGS(%s) %s: after list not explained by any before list: %s' % (kind, key[0] if kind != 'unity' else rel, keepbad))
        for fb, ob_, rel in eb:
            ok = any(not cmp_lists(key, fb, fa, ob_, oa_)[0] for fa, oa_, _ in ea)
            if not ok: viol.append('FLAGS(%s) %s: before list vanished: %s' % (kind, key[0] if kind != 'unity' else rel, 'no after list matches'))
print('keys in both: %d (before %d, after %d)' % (nboth, len(B), len(A)))
print('declared token classes applied (per TU or per distinct list):')
for c, n in sorted(tally.items()): print('  %-14s %5d' % (c, n))
print('violations: %d' % len(viol))
for v in viol[:80]: print('  ' + v)
print('RESULT: %s' % ('IDENTICAL_MODULO_DECLARED' if not viol else 'DIFFERENT'))
sys.exit(0 if not viol else 1)
