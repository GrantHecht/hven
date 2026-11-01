#!/usr/bin/env python3
"""NC2 scratch: corpus table, base vs a tag, per algebra. usage: table.py TAG [BASE=base]"""
import re, sys
S = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a69f46e11796560b9/.scratch/nc2/corpus'
NAMES = {0: 'opt', 1: 'cap', 2: 'inf', 3: 'num'}
tag = sys.argv[1]
base = sys.argv[2] if len(sys.argv) > 2 else 'base'


def key(path):
    m = re.search(r'dumps/([^/]+)/(qp\d+)\.txt', path)
    if m:
        return f'{m.group(1)}/{m.group(2)}'
    m = re.search(r'(Cs_STEENBRB_qp\d)\.txt', path)
    return 'R1_' + m.group(1)


def load(t, alg):
    out = {}
    for lst in ('old36', 'new15'):
        try:
            for line in open(f'{S}/{t}-{alg}-{lst}.txt'):
                parts = line.split()
                d = dict(p.split('=', 1) for p in parts[1:] if '=' in p)
                out[key(parts[0])] = d
        except FileNotFoundError:
            pass
    return out


def cell(d):
    if d is None:
        return '--'
    return f"{NAMES[int(d['status'])]} {d['minors']}"


rows = {}
for alg in ('border', 'refactorize'):
    rows[alg] = (load(base, alg), load(tag, alg))
offb, offt = load(base + 'off', 'border'), load(tag + 'off', 'border')
keys = list(rows['border'][0].keys())
cnt = {}
print(f'| dump | border {base} | border {tag} | refac {base} | refac {tag} | off {base} | off {tag} | f ({tag} border) |')
for k in keys:
    b0, b1 = rows['border'][0].get(k), rows['border'][1].get(k)
    r0, r1 = rows['refactorize'][0].get(k), rows['refactorize'][1].get(k)
    o0, o1 = offb.get(k), offt.get(k)
    moved = any(a is not None and b is not None and (a['status'], a['minors'], a['f']) != (b['status'], b['minors'], b['f']) for a, b in ((b0, b1), (r0, r1), (o0, o1)))
    mark = ' *' if moved else ''
    print(f"| {k}{mark} | {cell(b0)} | {cell(b1)} | {cell(r0)} | {cell(r1)} | {cell(o0)} | {cell(o1)} | {b1['f'] if b1 else '--'} |")
for lab, (a, b) in (('border', rows['border']), ('refac', rows['refactorize']), ('off', (offb, offt))):
    na = sum(1 for d in a.values() if d['status'] == '0')
    nb = sum(1 for d in b.values() if d['status'] == '0')
    print(f'{lab}: kOptimal {na}/{len(a)} -> {nb}/{len(b)}')
