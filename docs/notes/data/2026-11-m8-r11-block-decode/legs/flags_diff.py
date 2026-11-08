#!/usr/bin/env python3
# M8 R11: copied from F1 / PF-1 (see legs/*.sh line 2), byte for byte below this line.
"""Compile-flags diff between two builds' compile_commands.json.

usage: flags_diff.py BEFORE/compile_commands.json AFTER/compile_commands.json

For every translation unit under src/ (the library), the full compile command is
reduced to its flag multiset: the compiler, the source path, the object path (-o)
and the -c / dependency-file tokens are dropped, since those are per-file
spellings, not flags. Every other token (defines, include dirs, -O level,
-march, -ffast-math, -mllvm options, -std, PCH use, ...) is compared exactly.
Exit 0 iff every TU present in both builds has an identical flag list and the
only TUs present in one build are listed.
"""
import json
import os
import shlex
import sys


def load(path):
    out = {}
    for e in json.load(open(path)):
        f = e['file']
        toks = shlex.split(e.get('command') or ' '.join(e['arguments']))
        keep = []
        i = 1
        while i < len(toks):
            t = toks[i]
            if t in ('-o', '-MF', '-MT', '-MQ'):
                i += 2
                continue
            if t == '-c' or t == '-MD' or t == '-MMD':
                i += 1
                continue
            if t == f or os.path.abspath(t) == os.path.abspath(f):
                i += 1
                continue
            keep.append(t)
            i += 1
        out[f] = (toks[0], keep)
    return out


def rel(f, root_hint):
    i = f.find('/src/')
    return f[i + 1:] if i >= 0 else f


def main():
    a = load(sys.argv[1])
    b = load(sys.argv[2])

    def lib(d):
        return {rel(f, None): v for f, v in d.items() if '/src/' in f and '/tests/' not in f and '/build-b1/' not in f}

    la, lb = lib(a), lib(b)
    only_a = sorted(set(la) - set(lb))
    only_b = sorted(set(lb) - set(la))
    both = sorted(set(la) & set(lb))
    differ = [f for f in both if la[f] != lb[f]]
    print('library TUs before: %d, after: %d, in both: %d' % (len(la), len(lb), len(both)))
    print('only before: %s' % (only_a or 'none'))
    print('only after:  %s' % (only_b or 'none'))
    print('TUs whose flag list differs: %d' % len(differ))
    for f in differ:
        sa, sb = set(la[f][1]), set(lb[f][1])
        print('  %s  -%s +%s' % (f, sorted(sa - sb), sorted(sb - sa)))
    # every added TU must carry the same flags as an existing library TU
    ref_flags = next(iter(la.values()))[1] if la else []
    for f in only_b:
        same = lb[f][1] == ref_flags
        print('added TU %s: flags identical to the library baseline TU: %s' % (f, same))
    # the full-set statement: all library TUs, before and after, share one flag list?
    uniq_a = {tuple(v[1]) for v in la.values()}
    uniq_b = {tuple(v[1]) for v in lb.values()}
    print('distinct flag lists among library TUs: before %d, after %d' % (len(uniq_a), len(uniq_b)))
    ok = not differ
    print('RESULT: %s' % ('IDENTICAL' if ok else 'DIFFERENT'))
    return 0 if ok else 1


sys.exit(main())
