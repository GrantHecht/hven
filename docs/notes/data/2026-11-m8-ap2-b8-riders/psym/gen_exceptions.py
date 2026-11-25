#!/usr/bin/env python3
"""AP-2 B8 riders: write psym/exceptions.txt for P13 STEP 1 (psym-before vs psym-mid) from the DEV comparison's
findings, by four stated rules, refusing anything they do not cover (exit 1, listed).

  E1 THE DECLARED CODE CHANGE (D4(b)), by object and function: linear::SymmetricFactor's constructor (the
     Decomposition check's message, kind_name() gone) [DIFFERS], and SymmetricFactor::analyze [DIFFERS] whose
     one changed record is the renumbered switch table of its own TU (E4(a) pairs the table).
  E2 A __cxx_global_var_init keyed by the global it initialises, whose key changes ONLY by the detail move at the
     mangled level (N4hven<len><NAME> -> N4hven6detail<len><NAME>, NAME in ../manifest/detail_moved_names.txt):
     an ONLY-BEFORE key K and an ONLY-AFTER key K' of the same object with K' == move(K); both sides excepted.
  E3 THE TEST TUs THE RIDERS EDITED (git diff --name-only BASE..CODE_HEAD under tests/): a finding in such a TU's
     object whose symbol is a gtest test body, its factory or destructor, its vtable / typeinfo / test_info_, the
     TU's static-registration initializer, or a gtest comparison helper on a string literal's array type (the
     re-spelled expectations, the renamed test SymmetricFactor.ADecompositionOutsideTheEnumerationThrows).
  E4 COMPILER-LOCAL LABEL FAMILIES (the SYMBOLS coverage blocks): (a) `.Lswitch.table.<f>` before-only/after-only
     pairs of one object whose functions are equal under rewrite.py, or a renumbered table of an E1 function;
     (b) a `.L.str`, `.LCPI` or `GCC_except_table` COUNT change in an object whose TU the riders edited (string
     literals re-spelled).
usage: gen_exceptions.py <findings.tsv> <compare.txt> <base> <code-head> <out exceptions.txt>"""
import os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rewrite import rewrite, MOVED
findings, cmp_, base, head, out = sys.argv[1:6]
WT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../../..'))
edited = subprocess.run(['git', '-C', WT, 'diff', '--name-only', base, head], capture_output=True, text=True).stdout.split()
edited_tu = {os.path.basename(p)[:-4] for p in edited if p.endswith('.cpp')}
def tu_edited(obj):
    m = re.search(r'/([\w]+)\.cpp\.o$', obj)
    if m:
        return m.group(1) in edited_tu
    # a unity object: edited if any test TU of its directory was edited (reported per object)
    m = re.search(r'tests/(\w+)/CMakeFiles/.*/Unity/', obj)
    return bool(m) and any(p.startswith(f'tests/{m.group(1)}/') and p.endswith('.cpp') for p in edited)
SF = 'CMakeFiles/hven.dir/src/linear/symmetric_factor_mkl.cpp.o'
E1 = {(SF, 'DIFFERS', 'hven::linear::SymmetricFactor::SymmetricFactor(hven::linear::SymmetricFactor::Options)'):
          'D4(b): the Decomposition check throws with the new message; kind_name() is gone',
      (SF, 'DIFFERS', 'hven::linear::SymmetricFactor::analyze(Eigen::SparseMatrix<double, 1, int> const&)'):
          'D4(b): its switch table is renumbered (kind_name()\'s tables are gone from the TU); no instruction moves'}
E3_SYM = re.compile(r'(^(vtable|typeinfo|typeinfo name) for (testing::internal::TestFactoryImpl<)?[\w:() ]+_Test>?$|_Test::test_info_$|_Test::TestBody\(\)|_Test::~\w+_Test\(\)|TestFactoryImpl<[\w:() ]+_Test>::(~TestFactoryImpl|CreateTest)\(\)|'
                    r'^_GLOBAL__sub_I_\w+\.cpp$|^testing::AssertionResult testing::internal::CmpHelperEQFailure<.*char \[\d+\]>)')
MV = sorted(MOVED, key=len, reverse=True)
def move(k):
    for n in MV:
        k = re.sub(r'N4hven' + str(len(n)) + n + r'(?=[A-Z0-9])', 'N4hven6detail' + str(len(n)) + n, k)
    return k
rows = [l.rstrip('\n').split('\t') for l in open(findings) if l.strip()]
lines, refused, gvi = [], [], {}
for obj, kind, sym in rows:
    if sym.startswith('__cxx_global_var_init{'):
        gvi.setdefault(obj, {}).setdefault(kind, []).append(sym); continue
    if (obj, kind, sym) in E1:
        lines.append(f'{obj}::{sym} [{kind}] ## E1 {E1[(obj, kind, sym)]}'); continue
    if tu_edited(obj) and E3_SYM.search(sym):
        lines.append(f'{obj}::{sym} [{kind}] ## E3 a test TU the riders edited (expectations re-spelled / a test renamed)'); continue
    refused.append((obj, kind, sym))
for obj, d in gvi.items():
    after = list(d.get('ONLY-AFTER', []))
    for k in d.get('ONLY-BEFORE', []):
        t = move(k)
        if t in after:
            after.remove(t)
            lines.append(f'{obj}::{k} [ONLY-BEFORE] ## E2 detail-move-only identity change; after-arm key {t}')
            lines.append(f'{obj}::{t} [ONLY-AFTER] ## E2 detail-move-only identity change; before-arm key {k}')
        else:
            refused.append((obj, 'ONLY-BEFORE', k))
    refused += [(obj, 'ONLY-AFTER', t) for t in after]
fam, obj = {}, None
for line in open(cmp_):
    m = re.match(r'^SYMBOLS\s+(\S+): COVERAGE FINDINGS', line)
    if m: obj = m.group(1); continue
    m = re.match(r'^              LOCAL-FAMILY  (\S+): (\d+) before, (\d+) after', line)
    if m and obj: fam.setdefault(obj, []).append((m.group(1), int(m.group(2)), int(m.group(3))))
    elif not line.startswith('              '): obj = None
dem = lambda names: subprocess.run(['llvm-cxxfilt'], input='\n'.join(n[len('.Lswitch.table.'):] for n in names) + '\n',
                                   capture_output=True, text=True).stdout.split('\n')[:len(names)]
for o, fs in fam.items():
    sw_b = [f for f, b, a in fs if f.startswith('.Lswitch.table.') and b > 0 and a == 0]
    sw_a = [f for f, b, a in fs if f.startswith('.Lswitch.table.') and b == 0 and a > 0]
    da = dict(zip(dem(sw_a), sw_a))
    for f, d in zip(sw_b, dem(sw_b)):
        t = rewrite(d)
        if t in da:
            lines.append(f'{o}::{f} [LOCAL-FAMILY] ## E4(a) switch table of a renamed function; after-arm {da[t]}')
            lines.append(f'{o}::{da[t]} [LOCAL-FAMILY] ## E4(a) switch table of a renamed function; before-arm {f}')
            del da[t]
        elif o == SF and any(re.sub(r'\.\d+$', '', x) == re.sub(r'\.\d+$', '', f) for x in da.values()):
            k = next(k for k, x in da.items() if re.sub(r'\.\d+$', '', x) == re.sub(r'\.\d+$', '', f))
            lines.append(f'{o}::{f} [LOCAL-FAMILY] ## E4(a) the E1 function\'s renumbered switch table; after-arm {da[k]}')
            lines.append(f'{o}::{da[k]} [LOCAL-FAMILY] ## E4(a) the E1 function\'s renumbered switch table; before-arm {f}')
            del da[k]
        else:
            refused.append((o, 'LOCAL-FAMILY', f))
    refused += [(o, 'LOCAL-FAMILY', f) for f in da.values()]
    for f, b, a in fs:
        if f.startswith('.Lswitch.table.'): continue
        src_edited = tu_edited(o) or o.startswith('CMakeFiles/hven.dir/src/') and os.path.basename(o)[:-6] in edited_tu
        if f in ('.L.str', '.LCPI', 'GCC_except_table') and src_edited:
            lines.append(f'{o}::{f} [LOCAL-FAMILY] ## E4(b) {b} -> {a}: string literals the riders re-spelled in this TU')
        else:
            refused.append((o, 'LOCAL-FAMILY', f'{f} {b}->{a}'))
lines = list(dict.fromkeys(lines)); refused = list(dict.fromkeys(refused))
open(out, 'w').write('\n'.join(lines) + '\n')
print(f'exceptions {len(lines)}; refused {len(refused)}')
for r in refused: print('REFUSED', *r, sep='\t')
sys.exit(1 if refused else 0)
