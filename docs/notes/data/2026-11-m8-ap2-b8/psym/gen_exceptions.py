#!/usr/bin/env python3
"""AP-2 B8: write psym/exceptions.txt from the DEV comparison's findings (run before the declaration) by
three stated rules, and refuse anything the rules do not cover (exit 1, listed).

  E1 THE DECLARED CODE CHANGES, by object and function (DECLARATION D4): validate(IpmOptions) (the range
     of solve_phase_mode), dump_options(IpmOptions) (the dump's strings), phase_steps() (the one
     SolvePhaseMode -> AlgorithmMode mapping), validate(SqpOptions) (the floor), enter_restoration (the
     re-associated entry test). [DIFFERS].
  E2 A __cxx_global_var_init keyed by the global it initialises, whose key changes ONLY by the namespace
     rename: an ONLY-BEFORE key K and an ONLY-AFTER key K' of the same object with K' == ns(K), ns the
     mangled-level form of the namespace rule (4hven7solvers6detail -> 4hven6detail,
     4hven7solvers -> 4hven, 4hven5utils6detail -> 4hven6detail, 4hven5utils -> 4hven6detail). Both
     sides are excepted, each naming the other. scripts/psym_compare.sh applies no symbol map to an
     identity key, so this pairing is the map for that family.
  E3 THE TEST TUs B8 EDITED (strings re-spelled, one test renamed, one added): any finding in the objects
     of tests/sqp/test_sqp_qn_mode.cpp, tests/sqp/test_sqp_restoration.cpp, tests/drivers/test_limits.cpp,
     tests/drivers/test_options_dump.cpp, and the interior unity object that compiles
     tests/interior/test_ipm_solver_presets.cpp, provided the symbol is a gtest test body, its factory or
     destructor, its vtable / typeinfo / typeinfo name / test_info_ (the test's own and its factory's), the
     TU's static-registration initializer, or a gtest comparison helper instantiated on a string literal's
     array type.
usage: gen_exceptions.py <findings.tsv (build-b8/tools/psym_findings.py output)> <out exceptions.txt>"""
import re, sys
findings, out = sys.argv[1], sys.argv[2]
E1 = {
    ('CMakeFiles/hven.dir/src/drivers/ipm_options.cpp.o', 'DIFFERS', 'hven::validate(hven::IpmOptions const&)'):
        'D4(b): validate() bounds solve_phase_mode by SolvePhaseMode::kSolveEquations (was AlgorithmMode::kInit)',
    ('CMakeFiles/hven.dir/src/drivers/ipm_options.cpp.o', 'DIFFERS',
     'hven::dump_options(hven::IpmOptions const&, std::basic_ostream<char, std::char_traits<char> >&)'):
        'D4(c): the dump prints the renamed keys and enumerators (string literals of new lengths)',
    ('CMakeFiles/hven.dir/src/drivers/ipm_solver.cpp.o', 'DIFFERS', 'hven::IpmSolverImpl::phase_steps() const'):
        'D4(b): the kSolve phase maps its SolvePhaseMode onto the internal AlgorithmMode (to_algorithm_mode)',
    ('CMakeFiles/hven.dir/src/drivers/sqp_options.cpp.o', 'DIFFERS', 'hven::validate(hven::SqpOptions const&)'):
        'D4(a): max_restoration_entries floor >= 0 (was >= 1) and its message',
}
E1_PREFIX = {('CMakeFiles/hven.dir/src/drivers/sqp_solver.cpp.o', 'DIFFERS',
              'hven::SqpSolverImpl::enter_restoration('):
             'D4(a): the entry test re-associated so a cap of 0 refuses the first request (equal for every cap >= 1)'}
E3_OBJ = re.compile(r'(test_sqp_qn_mode|test_sqp_restoration|test_limits|test_options_dump)\.cpp\.o$|'
                    r'hven_interior_tests\.dir/Unity/unity_0_cxx\.cxx\.o$')
E3_SYM = re.compile(r'(^(vtable|typeinfo|typeinfo name) for (testing::internal::TestFactoryImpl<)?\w+_Test>?$|_Test::test_info_$|_Test::TestBody\(\)|_Test::~\w+_Test\(\)|TestFactoryImpl<\w+_Test>::(~TestFactoryImpl|CreateTest)\(\)|'
                    r'^_GLOBAL__sub_I_\w+\.cpp$|^testing::AssertionResult testing::internal::CmpHelperEQFailure<.*char \[\d+\]>)')
NS = [('4hven7solvers6detail', '4hven6detail'), ('4hven7solvers', '4hven'),
      ('4hven5utils6detail', '4hven6detail'), ('4hven5utils', '4hven6detail')]


def ns(k):
    for a, b in NS:
        k = k.replace(a, b)
    return k


rows = [l.rstrip('\n').split('\t') for l in open(findings) if l.strip()]
lines, refused = [], []
gvi = {}
for obj, kind, sym in rows:
    if sym.startswith('__cxx_global_var_init{'):
        gvi.setdefault(obj, {'ONLY-BEFORE': [], 'ONLY-AFTER': []}).setdefault(kind, []).append(sym)
        continue
    key = (obj, kind, sym)
    if key in E1:
        lines.append(f'{obj}::{sym} [{kind}] ## E1 {E1[key]}')
        continue
    hit = [r for (o, k, p), r in E1_PREFIX.items() if o == obj and k == kind and sym.startswith(p)]
    if hit:
        lines.append(f'{obj}::{sym} [{kind}] ## E1 {hit[0]}')
        continue
    if E3_OBJ.search(obj) and E3_SYM.search(sym):
        lines.append(f'{obj}::{sym} [{kind}] ## E3 a test TU B8 edited (strings re-spelled / a test renamed or added)')
        continue
    refused.append((obj, kind, sym))
for obj, d in gvi.items():
    before, after = d.get('ONLY-BEFORE', []), list(d.get('ONLY-AFTER', []))
    for k in before:
        t = ns(k)
        if t in after:
            after.remove(t)
            lines.append(f'{obj}::{k} [ONLY-BEFORE] ## E2 namespace-only identity change; after-arm key {t}')
            lines.append(f'{obj}::{t} [ONLY-AFTER] ## E2 namespace-only identity change; before-arm key {k}')
        else:
            refused.append((obj, 'ONLY-BEFORE', k))
    for t in after:
        refused.append((obj, 'ONLY-AFTER', t))
# E4 COMPILER-LOCAL LABEL FAMILIES (the SYMBOLS coverage block of compare.txt, argv[3]):
#   (a) `.Lswitch.table.<mangled>` -- a switch table named after its function: a before-only and an
#       after-only family of one object pair when their functions' llvm-cxxfilt forms are equal under the
#       namespace and type rule (gen_maps.py's rewrite);
#   (b) a `.L.str`, `.LCPI` or `GCC_except_table` COUNT change, only in an object whose TU B8 edited: the
#       dump's keys (ipm_options.cpp), gtest's stringized assertion arguments naming a renamed member
#       (test_aggregate_contract, test_limits, test_options_dump, test_sqp_qn_mode), the E3 TUs' new or
#       re-spelled tests (test_sqp_restoration, the interior unity object of test_ipm_solver_presets).
import importlib.util, os, subprocess
spec = importlib.util.spec_from_file_location('gm', os.path.join(os.path.dirname(os.path.abspath(__file__)), 'gen_maps_rewrite.py'))
E4B_OBJ = re.compile(r'((src/drivers/ipm_options|test_aggregate_contract|test_limits|test_options_dump|test_sqp_qn_mode|test_sqp_restoration)\.cpp|hven_interior_tests\.dir/Unity/unity_0_cxx\.cxx)\.o$')
if len(sys.argv) > 3:
    gm = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(gm)
    fam = {}
    obj = None
    for line in open(sys.argv[3]):
        m = re.match(r'^SYMBOLS\s+(\S+): COVERAGE FINDINGS', line)
        if m:
            obj = m.group(1)
            continue
        m = re.match(r'^              LOCAL-FAMILY  (\S+): (\d+) before, (\d+) after', line)
        if m and obj:
            fam.setdefault(obj, []).append((m.group(1), int(m.group(2)), int(m.group(3))))
        elif not line.startswith('              '):
            obj = None if not line.startswith('SYMBOLS') else obj
    for o, fs in fam.items():
        sw_b = [f for f, b, a in fs if f.startswith('.Lswitch.table.') and b > 0 and a == 0]
        sw_a = [f for f, b, a in fs if f.startswith('.Lswitch.table.') and b == 0 and a > 0]
        dem = lambda names: subprocess.run(['llvm-cxxfilt'], input='\n'.join(n[len('.Lswitch.table.'):] for n in names) + '\n',
                                           capture_output=True, text=True).stdout.split('\n')[:len(names)]
        da = dict(zip(dem(sw_a), sw_a))
        for f, d in zip(sw_b, dem(sw_b)):
            t = gm.rewrite(d)
            if t in da:
                lines.append(f'{o}::{f} [LOCAL-FAMILY] ## E4(a) switch table of a renamed function; after-arm {da[t]}')
                lines.append(f'{o}::{da[t]} [LOCAL-FAMILY] ## E4(a) switch table of a renamed function; before-arm {f}')
                del da[t]
            else:
                refused.append((o, 'LOCAL-FAMILY', f))
        for f in da.values():
            refused.append((o, 'LOCAL-FAMILY', f))
        for f, b, a in fs:
            if f.startswith('.Lswitch.table.'):
                continue
            if f in ('.L.str', '.LCPI', 'GCC_except_table') and E4B_OBJ.search(o):
                lines.append(f'{o}::{f} [LOCAL-FAMILY] ## E4(b) {b} -> {a}: string literals B8 re-spelled in this TU')
            else:
                refused.append((o, 'LOCAL-FAMILY', f'{f} {b}->{a}'))
lines = list(dict.fromkeys(lines))
refused = list(dict.fromkeys(refused))
open(out, 'w').write('\n'.join(lines) + '\n')
print(f'exceptions {len(lines)}; refused {len(refused)}')
for r in refused:
    print('REFUSED', *r, sep='\t')
sys.exit(1 if refused else 0)
