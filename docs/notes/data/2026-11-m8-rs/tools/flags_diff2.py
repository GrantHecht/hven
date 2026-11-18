#!/usr/bin/env python3
"""RS compile-flags diff over EVERY translation unit (library, tests, bench, examples).  [flags_diff2: flags_diff.py + the three RS-2
options below; flags_diff.py itself is unchanged and stays the RS-1 instrument]

usage: flags_diff.py BEFORE.json AFTER.json --root WT [--map MAPFILE ...] [--expect-removed TOKEN ...]
Each compile command is reduced to its flag multiset: compiler, source file, -o/-MF/-MT/-MQ values and -c/-MD
tokens are dropped (per-file spellings).  The worktree root is rewritten to '@' in every remaining token, and every
'@/<path>' that names a moved/renamed source (MAPFILE lines `old -> new`) is rewritten to its new path in the BEFORE
arm, so a define such as HVEN_SQP_SOLVER_SOURCE=@/src/drivers/sqp_solver.cpp compares equal to its re-pointed
spelling.  A TU is keyed by (mapped source, CMake target dir of its -o).  Deleted sources are listed.
Provenance stamps (-DHVEN_*_GIT_DESCRIBE=..., -DHVEN_RIG_COMMIT=...: the build tree's git revision) are compared as
'<git-stamp>': they move on every commit by construction (and the object bytes they reach are .rodata strings).
--expect-removed: a literal token (after '@' rewriting) allowed to be present only in BEFORE, e.g. -I@/src/interior/utils.
RS-2 additions (declared in DECLARATION-RS2.md):
--dir-move OLD=NEW        a DIRECTORY move: a path token under @/OLD is compared as @/NEW in the BEFORE arm (golden/fixture dir defines).
--allow-removed REGEX     a token (after '@' rewriting) allowed to be present only in BEFORE when it fully matches REGEX (the old per-directory test include roots).
--allow-added TOKEN       a literal token allowed to be present only in AFTER (the tests include root).
Every applied removal/addition is tallied per CMake target and printed, so the per-target delta table is part of the output.
Exit 0 iff the TU sets match (modulo deletions) and every flag list is equal modulo the expected removals."""
import json, shlex, sys, re, argparse, posixpath

ap = argparse.ArgumentParser()
ap.add_argument('before'); ap.add_argument('after')
ap.add_argument('--root', required=True)
ap.add_argument('--map', action='append', default=[])
ap.add_argument('--expect-removed', action='append', default=[])
ap.add_argument('--dir-move', action='append', default=[])
ap.add_argument('--allow-removed', action='append', default=[])
ap.add_argument('--allow-added', action='append', default=[])
a = ap.parse_args()
root = a.root.rstrip('/')
dirmoves = []
for d in a.dir_move:
    o_, _, n_ = d.partition('=')
    dirmoves.append((o_.rstrip('/'), n_.rstrip('/')))
allow_removed = [re.compile(r) for r in a.allow_removed]
allow_added = set(a.allow_added)
from collections import Counter
tally = Counter()
moves, deletes = {}, set()
for mf in a.map:
    for ln in open(mf):
        ln = ln.rstrip('\n')
        if ' -> ' not in ln: continue
        o, n = ln.split(' -> ', 1)
        if n.startswith('DELETE'): deletes.add(o)
        else: moves[o] = n

def mapped(path):
    return moves.get(path, path)

def load(p, is_before):
    out = {}
    for e in json.load(open(p)):
        toks = shlex.split(e.get('command') or ' '.join(e['arguments']))
        f = e['file']
        rel = f[len(root) + 1:] if f.startswith(root + '/') else f
        keep, i, obj = [], 1, ''
        while i < len(toks):
            t = toks[i]
            if t in ('-o', '-MF', '-MT', '-MQ'):
                if t == '-o': obj = toks[i + 1]
                i += 2; continue
            if t in ('-c', '-MD', '-MMD'): i += 1; continue
            if t == f: i += 1; continue
            keep.append(t); i += 1
        norm = []
        for t in keep:
            t = t.replace(root, '@')
            # provenance stamps: the git revision of the tree being built is a per-commit value by construction
            t = re.sub(r'^(-DHVEN_[A-Z0-9_]*(?:GIT_DESCRIBE|RIG_COMMIT)=).*$', r'\1<git-stamp>', t)
            # a path spelled with '..' (tests/sqp/../../src/x.cpp) is compared in its normal form
            t = re.sub(r'@/([\w./+-]+)', lambda m: '@/' + posixpath.normpath(m.group(1)), t)
            if is_before:
                for m in re.finditer(r'@/([\w./+-]+)', t):
                    p0 = m.group(1)
                    if p0 in moves:
                        t = t.replace('@/' + p0, '@/' + moves[p0])
                    else:
                        for od, nd in dirmoves:
                            if p0 == od or p0.startswith(od + '/'):
                                t = t.replace('@/' + p0, '@/' + nd + p0[len(od):])
            norm.append(t)
        tdir = re.search(r'CMakeFiles/([^/]+)\.dir', obj)
        tgt = tdir.group(1) if tdir else obj
        src = mapped(rel) if is_before else rel
        key = (src, tgt)
        out[key] = (rel, sorted(norm))
    return out

b, a_ = load(a.before, True), load(a.after, False)
dele = [k for k, (rel, _) in b.items() if rel in deletes]
for k in dele: del b[k]
only_b, only_a = sorted(set(b) - set(a_)), sorted(set(a_) - set(b))
differ = []
exp = set(a.expect_removed)
for k in sorted(set(b) & set(a_)):
    fb = [t for t in b[k][1]]
    fa = a_[k][1]
    removed = [t for t in fb if t not in fa]
    added = [t for t in fa if t not in fb]
    ar = [t for t in removed if t not in exp and any(r.fullmatch(t) for r in allow_removed)]
    aa = [t for t in added if t in allow_added]
    unexpl_removed = [t for t in removed if t not in exp and t not in ar]
    added_unexpl = [t for t in added if t not in aa]
    if ar or aa: tally[(k[1], tuple(sorted(ar)), tuple(sorted(aa)))] += 1
    # multiset compare of the rest
    rest_b = sorted(t for t in fb if (t not in exp and t not in ar) or t in fa)
    rest_a = sorted(t for t in fa if t not in aa or t in fb)
    if unexpl_removed or added_unexpl or rest_b != rest_a:
        differ.append((k, removed, added))
nexp = sum(1 for k in set(b) & set(a_) if any(t in b[k][1] and t not in a_[k][1] for t in exp))
print('TUs before %d (after %d deleted sources removed: %d), after %d, in both %d' % (len(b) + len(dele), len(b), len(dele), len(a_), len(set(b) & set(a_))))
print('deleted sources: %s' % ([b_ for b_ in sorted(deletes) if b_.endswith(('.cpp', '.h'))] and sorted(k[0] for k in dele) or 'none'))
print('only before: %s' % (only_b or 'none'))
print('only after:  %s' % (only_a or 'none'))
print('TUs with an expected removal applied: %d (tokens %s)' % (nexp, sorted(exp) or 'none'))
print('declared deltas applied, per CMake target (TUs, removed, added):')
for (tg, rm, ad), n_ in sorted(tally.items()):
    print('  %-34s %4d  -%s +%s' % (tg, n_, list(rm), list(ad)))
print('TUs whose flag list differs beyond the expected: %d' % len(differ))
for k, r, ad in differ:
    print('  %s [%s] -%s +%s' % (k[0], k[1], r, ad))
ok = not only_b and not only_a and not differ
print('RESULT: %s' % ('IDENTICAL' if ok else 'DIFFERENT'))
sys.exit(0 if ok else 1)
