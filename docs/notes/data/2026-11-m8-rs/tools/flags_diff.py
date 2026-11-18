#!/usr/bin/env python3
"""RS compile-flags diff over EVERY translation unit (library, tests, bench, examples).

usage: flags_diff.py BEFORE.json AFTER.json --root WT [--map MAPFILE ...] [--expect-removed TOKEN ...]
Each compile command is reduced to its flag multiset: compiler, source file, -o/-MF/-MT/-MQ values and -c/-MD
tokens are dropped (per-file spellings).  The worktree root is rewritten to '@' in every remaining token, and every
'@/<path>' that names a moved/renamed source (MAPFILE lines `old -> new`) is rewritten to its new path in the BEFORE
arm, so a define such as HVEN_SQP_SOLVER_SOURCE=@/src/drivers/sqp_solver.cpp compares equal to its re-pointed
spelling.  A TU is keyed by (mapped source, CMake target dir of its -o).  Deleted sources are listed.
Provenance stamps (-DHVEN_*_GIT_DESCRIBE=..., -DHVEN_RIG_COMMIT=...: the build tree's git revision) are compared as
'<git-stamp>': they move on every commit by construction (and the object bytes they reach are .rodata strings).
--expect-removed: a literal token (after '@' rewriting) allowed to be present only in BEFORE, e.g. -I@/src/interior/utils.
Exit 0 iff the TU sets match (modulo deletions) and every flag list is equal modulo the expected removals."""
import json, shlex, sys, re, argparse

ap = argparse.ArgumentParser()
ap.add_argument('before'); ap.add_argument('after')
ap.add_argument('--root', required=True)
ap.add_argument('--map', action='append', default=[])
ap.add_argument('--expect-removed', action='append', default=[])
a = ap.parse_args()
root = a.root.rstrip('/')
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
            if is_before:
                for m in re.finditer(r'@/([\w./+-]+)', t):
                    p0 = m.group(1)
                    if p0 in moves:
                        t = t.replace('@/' + p0, '@/' + moves[p0])
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
    unexpl_removed = [t for t in removed if t not in exp]
    # multiset compare of the rest
    rest_b = sorted(t for t in fb if t not in exp or t in fa)
    if unexpl_removed or added or rest_b != fa:
        differ.append((k, removed, added))
nexp = sum(1 for k in set(b) & set(a_) if any(t in b[k][1] and t not in a_[k][1] for t in exp))
print('TUs before %d (after %d deleted sources removed: %d), after %d, in both %d' % (len(b) + len(dele), len(b), len(dele), len(a_), len(set(b) & set(a_))))
print('deleted sources: %s' % ([b_ for b_ in sorted(deletes) if b_.endswith(('.cpp', '.h'))] and sorted(k[0] for k in dele) or 'none'))
print('only before: %s' % (only_b or 'none'))
print('only after:  %s' % (only_a or 'none'))
print('TUs with an expected removal applied: %d (tokens %s)' % (nexp, sorted(exp) or 'none'))
print('TUs whose flag list differs beyond the expected: %d' % len(differ))
for k, r, ad in differ:
    print('  %s [%s] -%s +%s' % (k[0], k[1], r, ad))
ok = not only_b and not only_a and not differ
print('RESULT: %s' % ('IDENTICAL' if ok else 'DIFFERENT'))
sys.exit(0 if ok else 1)
