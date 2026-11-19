#!/usr/bin/env python3
"""psym_union: per-symbol instruction identity over a CLASS of objects, for the RS-6a unity class (DECLARATION-RS6a.md).
A unity batch cannot be paired one-to-one with a re-composed batch, so the class is compared as a whole.
usage: psym_union.py --before LISTFILE --after LISTFILE [--root-before DIR --root-after DIR] [--jobs N] [--detail OUT]
LISTFILE: one object path per line (relative to --root-*). Each object is disassembled with GNU `objdump -dr -C --no-show-raw-insn`.
Per defined function symbol the body is normalised (addresses, `# ...` comments and raw branch-target addresses dropped; section-relative relocation
addends dropped; `.N` numeric suffixes of static-initialiser symbols dropped) and hashed; the class is the SET of (demangled name, body hash).
  * `_GLOBAL__sub_I_*` symbols are the per-TU initialiser entry points: their number and composition follow the batching, so they are compared only
    by the TOTAL count of `__cxx_global_var_init*` calls they make, before vs after.
  * `__cxx_global_var_init*` bodies are compared as the set of bodies (names collapse to one).
Every function present in one class and not the other, and every (name, hash) in one and not the other, is printed; exit 0 iff none.
Counts are informational beyond that (a COMDAT symbol is emitted once per object that needs it, so object counts differ with the batching)."""
import argparse, hashlib, re, subprocess, sys, os
from concurrent.futures import ThreadPoolExecutor
ap = argparse.ArgumentParser()
ap.add_argument('--before', required=True); ap.add_argument('--after', required=True)
ap.add_argument('--root-before', default='.'); ap.add_argument('--root-after', default='.')
ap.add_argument('--jobs', type=int, default=4); ap.add_argument('--detail')
a = ap.parse_args()
hdr = re.compile(r'^[0-9a-f]+ <(.+)>:$')
insn = re.compile(r'^\s*[0-9a-f]+:\t(.*)$')
rel = re.compile(r'^\s*[0-9a-f]+: (R_\w+)\s+(\S+)$')
def norm_sym(s):
    s = re.sub(r'\.\d+$', '', s)              # __cxx_global_var_init.12 -> __cxx_global_var_init
    return s
def norm_reloc(s):
    m = re.match(r'^(.*?)([+-]0x[0-9a-f]+)?$', s)
    base, add = m.group(1), m.group(2)
    if base.startswith('.'):                   # section symbol: the addend is a layout offset
        return base
    return norm_sym(base) + (add or '')
def disasm(path):
    out = subprocess.run(['objdump', '-dr', '-C', '--no-show-raw-insn', path], capture_output=True, text=True, check=True).stdout
    funcs, cur = {}, None
    for ln in out.splitlines():
        m = hdr.match(ln)
        if m:
            cur = m.group(1); funcs.setdefault(cur, []); continue
        if cur is None: continue
        m = insn.match(ln)
        if m:
            t = m.group(1)
            t = t.split('#')[0].rstrip()
            t = re.sub(r'^((?:j\w+|call\w*|loop\w*)\s+)[0-9a-f]+(\s+<[^>]*>)?$', lambda x: x.group(1) + (re.sub(r'\.\d+(?=[+>])', '', x.group(2)) if x.group(2) else ''), t)
            funcs[cur].append(t); continue
        m = rel.match(ln)
        if m: funcs[cur].append('@' + m.group(1) + ' ' + norm_reloc(m.group(2)))
    return funcs
def klass(listfile, root):
    paths = [os.path.join(root, l.strip()) for l in open(listfile) if l.strip()]
    with ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(disasm, paths))
    bodies, inits, nobj = {}, 0, len(paths)
    for funcs in res:
        for name, body in funcs.items():
            if not body: continue
            if name.startswith('_GLOBAL__sub_I_'):
                inits += sum(1 for b in body if '__cxx_global_var_init' in b and b.startswith('@') or (b.startswith('call') and '__cxx_global_var_init' in b))
                continue
            key = norm_sym(name) if name.startswith('__cxx_global_var_init') else name
            h = hashlib.sha256('\n'.join(body).encode()).hexdigest()[:16]
            bodies.setdefault(key, set()).add(h)
    return bodies, inits, nobj
B, ib, nb = klass(a.before, a.root_before); A, ia, na = klass(a.after, a.root_after)
errs = []
for k in sorted(set(B) - set(A)): errs.append('ONLY-BEFORE symbol: ' + k)
for k in sorted(set(A) - set(B)): errs.append('ONLY-AFTER symbol: ' + k)
diff = 0
for k in sorted(set(A) & set(B)):
    if A[k] != B[k]:
        diff += 1; errs.append('BODY-SET-DIFFERS: %s (before %d bodies, after %d, shared %d)' % (k, len(B[k]), len(A[k]), len(A[k] & B[k])))
if ib != ia: errs.append('INIT-CALLS differ: before %d, after %d' % (ib, ia))
nbod = sum(len(v) for v in B.values())
print('class: before %d objects, %d symbols, %d (name,body) pairs, %d initialiser calls; after %d objects, %d symbols, %d pairs, %d initialiser calls' % (nb, len(B), nbod, ib, na, len(A), sum(len(v) for v in A.values()), ia))
if a.detail:
    open(a.detail, 'w').write('\n'.join(errs) + '\n')
if errs:
    print('\n'.join(errs[:60])); 
    if len(errs) > 60: print('... and %d more (see --detail)' % (len(errs) - 60))
    print('PSYM_UNION_DIFFERS %d' % len(errs)); sys.exit(1)
print('PSYM_UNION_IDENTICAL')
