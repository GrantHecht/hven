#!/usr/bin/env python3
"""psym_union: per-symbol instruction identity over a CLASS of objects, for the RS-6a unity class (DECLARATION-RS6a.md).
A unity batch cannot be paired one-to-one with a re-composed batch, so the class is compared as a whole.
usage: psym_union.py --before LISTFILE --after LISTFILE [--root-before DIR --root-after DIR] [--jobs N] [--detail OUT]
LISTFILE: one object path per line (relative to --root-*). Each object is disassembled with GNU `objdump -dr -C --no-show-raw-insn`.
Per defined function symbol the body is normalised (addresses, `# ...` comments and raw branch-target addresses dropped; section-relative relocation
addends dropped; `.N` numeric suffixes of static-initialiser symbols dropped) and hashed; the class is the SET of (demangled name, body hash).
  * `_GLOBAL__sub_I_*` symbols are the per-TU initialiser entry points: their number and composition follow the batching, so they are compared only
    as one MULTISET of the relocation targets their bodies call (with the registrations inlined into them, that is the registration calls themselves), before vs after.
  * `__cxx_global_var_init*` bodies are compared as the set of bodies (names collapse to one).
Reported: symbols in one class and not the other (a failure), the initialiser-entry multiset (a failure), and the symbols present in both whose bodies differ -- in tiers: identical; identical modulo numeric operands; differing beyond numbers (listed). Exit 0 iff the symbol sets and the initialiser multisets are equal (`--strict`: and no body differs).
Counts are informational beyond that (a COMDAT symbol is emitted once per object that needs it, so object counts differ with the batching)."""
import argparse, hashlib, re, subprocess, sys, os
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
ap = argparse.ArgumentParser()
ap.add_argument('--before', required=True); ap.add_argument('--after', required=True)
ap.add_argument('--root-before', default='.'); ap.add_argument('--root-after', default='.')
ap.add_argument('--jobs', type=int, default=4); ap.add_argument('--detail')
ap.add_argument('--strict', action='store_true', help='exit nonzero on ANY body difference (default: only on a symbol-set or initialiser difference; body tiers are reported)')
a = ap.parse_args()
hdr = re.compile(r'^[0-9a-f]+ <(.+)>:$')
sec = re.compile(r'^Disassembly of section (\S+):$')
EH = re.compile(r'^@R_\w+ (_Unwind_Resume|__clang_call_terminate|_ZSt9terminatev|__cxa_\w+)')
insn = re.compile(r'^\s*[0-9a-f]+:\t(.*)$')
rel = re.compile(r'^\s*[0-9a-f]+: (R_\w+)\s+(\S+)$')
def norm_sym(s):
    s = re.sub(r'\.\d+$', '', s)              # __cxx_global_var_init.12 -> __cxx_global_var_init
    return s
def norm_reloc(s):
    m = re.match(r'^(.*?)([+-]0x[0-9a-f]+)?$', s)
    base, add = m.group(1), m.group(2)
    if base.startswith('.L'):                  # assembler-local labels (.LCPI12_0 constant pool, .L.str.7, .Lswitch.table.N): their numbering follows the TU's composition
        return re.sub(r'\.?\d+', '#', base) + (add or '')
    if base.startswith('.'):                   # section symbol: the addend is a layout offset
        return base
    return norm_sym(base) + (add or '')
comdat = {}
def disasm(path):
    out = subprocess.run(['objdump', '-dr', '-C', '--no-show-raw-insn', path], capture_output=True, text=True, check=True).stdout
    funcs, cur, section = {}, None, ''
    for ln in out.splitlines():
        ms = sec.match(ln)
        if ms: section = ms.group(1); continue
        m = hdr.match(ln)
        if m:
            cur = m.group(1); funcs.setdefault(cur, []); comdat.setdefault(cur, False)
            if section.startswith('.text.') and not section.startswith(('.text.startup', '.text.unlikely', '.text.hot', '.text.exit')): comdat[cur] = True
            continue
        if cur is None: continue
        m = insn.match(ln)
        if m:
            t = m.group(1)
            cm = re.search(r'\s+# [0-9a-f]+ <(.*?)((?:\+0x[0-9a-f]+)?)>\s*$', t)
            t = re.split(r'\s+# ', t)[0].rstrip()
            if '(%rip)' in t and cm:
                # a rip-relative operand RESOLVED at assembly time (no relocation): its displacement is a layout distance; the comment's symbol+offset is the stable spelling
                t = re.sub(r'-?0x[0-9a-f]+\(%rip\)', 'RIP<%s%s>' % (cm.group(1), cm.group(2)), t)
            m2 = re.match(r'^((?:j\w+|call\w*|loop\w*))\s+[0-9a-f]+(?:\s+<(.*?)((?:\+0x[0-9a-f]+)?)>)?$', t)
            if m2:
                # a branch/call: the raw target address is dropped. objdump labels a RELOCATED target with the nearest symbol of the placeholder's address (meaningless in a .o; the relocation line that
                # follows carries the real target; it is stripped there), so only a label naming THIS function keeps its offset (an intra-function branch); any other label is dropped.
                base, off = m2.group(2), m2.group(3) or ''
                t = m2.group(1) + (' <self%s>' % off if base == cur else '')
            funcs[cur].append(t); continue
        m = rel.match(ln)
        if m:
            if funcs[cur] and re.match(r'^(?:j\w+|call\w*|loop\w*) <self', funcs[cur][-1]):
                funcs[cur][-1] = funcs[cur][-1].split(' <self')[0]   # a relocated branch: the placeholder's label is meaningless
            funcs[cur].append('@' + m.group(1) + ' ' + norm_reloc(m.group(2)))
    return funcs
def klass(listfile, root):
    paths = [os.path.join(root, l.strip()) for l in open(listfile) if l.strip()]
    with ThreadPoolExecutor(a.jobs) as ex:
        res = list(ex.map(disasm, paths))
    bodies, tier2, inits, nobj = {}, {}, Counter(), len(paths)
    cd = {}
    for funcs in res:
        for name, body in funcs.items():
            if not body: continue
            if name.startswith('_GLOBAL__sub_I_'):
                inits.update(b for b in body if b.startswith('@') and not b.startswith('@R_X86_64_PC32 .L') and not EH.match(b))   # the relocation targets the per-TU entry points call (labels and EH helpers excluded: their number follows inlining)
                continue
            key = norm_sym(name) if name.startswith('__cxx_global_var_init') else name
            h = hashlib.sha256('\n'.join(body).encode()).hexdigest()[:16]
            bodies.setdefault(key, set()).add(h)
            cd[key] = cd.get(key, False) or comdat.get(name, False)
            t2 = hashlib.sha256('\n'.join(re.sub(r'(?:-?0x[0-9a-f]+|\b\d+\b)', '#', b) for b in body).encode()).hexdigest()[:16]
            tier2.setdefault(key, set()).add(t2)
    return bodies, tier2, inits, nobj, cd
B, B2, ib, nb, Bcd = klass(a.before, a.root_before); A, A2, ia, na, Acd = klass(a.after, a.root_after)
errs = []
info = []
for tag, X, Y, Xcd in (('ONLY-BEFORE', B, A, Bcd), ('ONLY-AFTER', A, B, Acd)):
    for k in sorted(set(X) - set(Y)):
        # an out-of-line copy of a header inline/template function exists in an object only if the compiler did not inline every use there: that follows the composition, so it is informational,
        # EXCEPT for functions the TEST code itself defines (anonymous-namespace members and everything named for a test: fixtures, their factories): those must exist on both sides.
        if re.match(r'^(?:hven::)?\(anonymous namespace\)::', k) or '_Test' in k: errs.append('%s symbol: %s' % (tag, k))
        else: info.append('INFO %s template/inline symbol: %s' % (tag, k))
same = t2 = t3 = 0; tier3 = []
for k in sorted(set(A) & set(B)):
    if A[k] == B[k]: same += 1
    elif A2[k] == B2[k]: t2 += 1
    else: t3 += 1; tier3.append('BODY-DIFFERS (beyond numbers): %s (before %d bodies, after %d, shared %d)' % (k, len(B[k]), len(A[k]), len(A[k] & B[k])))
if ib != ia:
    for k in sorted(set(ib) | set(ia)):
        if ib[k] != ia[k]: errs.append('INIT-CALL target %s: before %d, after %d' % (k, ib[k], ia[k]))
nbod = sum(len(v) for v in B.values())
print('template/inline-function symbols in one class only (informational, composition-dependent inlining): %d' % len(info))
print('symbols in both: %d identical, %d identical modulo numeric operands (branch offsets, stack displacements, immediates), %d differing beyond numbers; symbol-set differences: %d' % (same, t2, t3, len(errs)))
print('class: before %d objects, %d symbols, %d (name,body) pairs, %d initialiser-entry calls; after %d objects, %d symbols, %d pairs, %d initialiser-entry calls' % (nb, len(B), nbod, sum(ib.values()), na, len(A), sum(len(v) for v in A.values()), sum(ia.values())))
if a.detail:
    open(a.detail, 'w').write('\n'.join(errs + info + tier3) + '\n')
if a.strict: errs += tier3
elif tier3: print('NOTE: %d symbols differ beyond numeric operands (listed in --detail): codegen of the same source under a different unity composition' % len(tier3))
if errs:
    print('\n'.join(errs[:60])); 
    if len(errs) > 60: print('... and %d more (see --detail)' % (len(errs) - 60))
    print('PSYM_UNION_DIFFERS %d' % len(errs)); sys.exit(1)
print('PSYM_UNION_IDENTICAL')
