#!/usr/bin/env python3
"""AP-2 B8: generate the P-SYM symbol map mechanically from the two captures' own objects and the B8
manifests (../manifest/). No entry is hand-written.

THE RULE. Every symbol name of the before arm (defined or referenced, `nm`, demangled by `c++filt`
exactly as scripts/psym_compare.sh demangles) is rewritten by
  1. the namespace rule:  hven::solvers::detail:: -> hven::detail::,  hven::solvers:: -> hven::,
     hven::utils::TypeStorage(Base) -> hven::TypeStorage(Base),  hven::utils::detail:: -> hven::detail::,
     hven::utils:: -> hven::detail::;
  2. every `type` entry of ../manifest/text_manifest.tsv, at identifier boundaries.
A demangled name whose rewrite differs is one map line `old => new`.
A name `c++filt` cannot demangle (it stays mangled: C++20 requires-clauses) is compared through
`llvm-cxxfilt` instead: it is mapped to the after-arm mangled name whose llvm-cxxfilt form equals its
own llvm-cxxfilt form rewritten -- only when exactly one after-arm name qualifies.
Member and enumerator renames reach no symbol name.
usage: gen_maps.py <before-capture-dir> <after-capture-dir> <out-dir>"""
import os, re, subprocess, sys
before, after, out = sys.argv[1], sys.argv[2], sys.argv[3]
here = os.path.dirname(os.path.abspath(__file__))
types = []
for line in open(os.path.join(here, '..', 'manifest', 'text_manifest.tsv')):
    if line.startswith('#') or not line.strip():
        continue
    f = line.rstrip('\n').split('\t')
    if f[0] == 'type':
        types.append((re.compile(r'(?<![\w])' + re.escape(f[1]) + r'(?![\w])'), f[2]))
NS = [(re.compile(r'\bhven::solvers::detail::'), 'hven::detail::'),
      (re.compile(r'\bhven::solvers::'), 'hven::'),
      (re.compile(r'\bhven::utils::(TypeStorage(?:Base)?)\b'), r'hven::\1'),
      (re.compile(r'\bhven::utils::detail::'), 'hven::detail::'),
      (re.compile(r'\bhven::utils::'), 'hven::detail::')]


def rewrite(s):
    for rx, n in NS:
        s = rx.sub(n, s)
    for rx, n in types:
        s = rx.sub(n, s)
    return s


def mangled_names(capture):
    names = set()
    for dp, _, fs in os.walk(capture):
        for f in fs:
            if f.endswith('.o'):
                r = subprocess.run(['nm', os.path.join(dp, f)], capture_output=True, text=True)
                for line in r.stdout.splitlines():
                    m = re.match(r'^(?:[0-9a-f]{16}| {16}) [A-Za-z] (\S+)$', line)
                    if m:
                        names.add(m.group(1))
    return sorted(names)


def filt(tool, names):
    r = subprocess.run([tool], input='\n'.join(names) + '\n', capture_output=True, text=True)
    return r.stdout.split('\n')[:len(names)]


bm = mangled_names(before)
am = mangled_names(after)
bd = filt('c++filt', bm)
ad_set = set(filt('c++filt', am))
lines = []
raw_b = [m for m, d in zip(bm, bd) if d == m and m.startswith('_Z')]
for m, d in zip(bm, bd):
    if d == m and m.startswith('_Z'):
        continue
    t = rewrite(d)
    if t != d:
        lines.append(f'{d} => {t}')
raw_a = [m for m, d in zip(am, filt('c++filt', am)) if d == m and m.startswith('_Z')]
nb = filt('llvm-cxxfilt', raw_b)
na = filt('llvm-cxxfilt', raw_a)
by_norm = {}
for m, n in zip(raw_a, na):
    by_norm.setdefault(n, []).append(m)
raw_mapped = raw_unpaired = 0
for m, n in zip(raw_b, nb):
    t = rewrite(n)
    if t == n:
        continue
    cands = by_norm.get(t, [])
    if len(cands) == 1:
        lines.append(f'{m} => {cands[0]}')
        raw_mapped += 1
    else:
        raw_unpaired += 1
        print(f'RAW-UNPAIRED ({len(cands)} candidates) {m}')
os.makedirs(out, exist_ok=True)
open(os.path.join(out, 'symbol-map.txt'), 'w').write('\n'.join(sorted(set(lines))) + '\n')
print(f'before names {len(bm)} after names {len(am)} mapped {len(set(lines))} '
      f'(raw-mangled mapped {raw_mapped}, raw unpaired {raw_unpaired})')
