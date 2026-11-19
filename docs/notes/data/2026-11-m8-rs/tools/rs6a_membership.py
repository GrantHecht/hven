#!/usr/bin/env python3
"""RS-6a declared membership: derive, from the BASE tree's CMake source lists and the placement rule of
DECLARATION-RS6a.md, (1) every new executable's source list with its origin attributes and (2) the declared
object map. Run from the repo root of the BASE tree.  usage: rs6a_membership.py {table|objmap}"""
import re, glob, posixpath, sys, json
def block(text, start):
    i = text.index(start); j = text.index('\n)', i); return text[i:j]
def srcs(blk, base):
    out = []
    for m in re.finditer(r'(?<![\w$/.-])((?:\.\./)*[\w./-]+\.cpp)\b', blk):
        p = m.group(1)
        out.append(posixpath.normpath(posixpath.join(base, p)))
    return out
T = 'tests'
root = open(T + '/CMakeLists.txt').read()
old = {}   # source (repo-relative to tests/) -> (old exe, old cmake dir)
def add(exe, d, files):
    for f in files:
        assert f not in old, (f, old[f], exe)
        old[f] = (exe, d)
hl = block(root, 'set(HVEN_TEST_SOURCES')
add('hven_tests', '', [x for x in srcs(hl, '') if '/' in x and not x.startswith('..')] + ['linear/test_symmetric_factor.cpp'])
add('hven_dense_alloc_tests', '', ['linear/test_dense_arm_allocation.cpp'])
add('hven_fault_injection_tests', '', ['linear/test_fault_injection.cpp', 'kkt/test_kkt_factorization_faults.cpp'])
add('hven_ipqp_seam_tests', '', ['qp/test_ipqp_seams.cpp'])
wb = block(root, 'add_executable(hven_qp_walk_seam_tests')
add('hven_qp_walk_seam_tests', '', [x for x in srcs(wb, '') if not x.startswith('/')])
sb = block(root, 'add_executable(hven_sqp_seam_tests')
add('hven_sqp_seam_tests', '', srcs(sb, ''))
for d, e, s in [('ipm', 'hven_interior_tests', 'set(HVEN_INTERIOR_TEST_SOURCES'), ('solver', 'hven_drivers_tests', 'set(HVEN_DRIVERS_TEST_SOURCES'),
                ('model', 'hven_model_tests', 'set(HVEN_MODEL_TEST_SOURCES'), ('sqp', 'hven_sqp_tests', 'set(HVEN_SQP_TEST_SOURCES')]:
    txt = open(T + '/' + d + '/CMakeLists.txt').read()
    add(e, d, [posixpath.normpath(posixpath.join(d, x)) for x in srcs(block(txt, s), '')])
# the two spelled-out extras
old['bench/test_snopt_bridge_gate.cpp'] = ('hven_sqp_tests', 'sqp')   # conditional (HVEN_SQP_HAVE_SNOPT)
EXES = 'core linear model kkt qp warmstart solver trace ipm sqp bench'.split()
SEAMS = {  # source -> seam exe
 'linear/test_fault_injection.cpp': 'hven_linear_seam_tests',
 'kkt/test_kkt_factorization_faults.cpp': 'hven_kkt_seam_tests',
 'solver/test_backend_error_seams.cpp': 'hven_solver_seam_tests',
 'solver/test_stop_poll_seams.cpp': 'hven_solver_seam_tests',
 'ipm/test_ipm_dense_guard_seams.cpp': 'hven_ipm_seam_tests',
 'sqp/test_sqp_equality_basis_seams.cpp': 'hven_sqp_seam_tests',
 'sqp/test_sqp_ladder_seams.cpp': 'hven_sqp_seam_tests',
 'qp/test_dense_solve_wound_seams.cpp': 'hven_qp_seam_tests',
 'qp/test_ipqp_seams.cpp': 'hven_qp_seam_tests',
}
for f, (e, d) in old.items():
    if e == 'hven_qp_walk_seam_tests': SEAMS[f] = 'hven_qp_seam_tests'
ALLOC = {'linear/test_dense_arm_allocation.cpp': 'hven_linear_alloc_tests'}
files = sorted(f for d in EXES for f in glob.glob(T + '/' + d + '/*.cpp') + glob.glob(T + '/' + d + '/support/*.cpp'))
files = [f[len(T) + 1:] for f in files]
assert set(files) == set(old), (sorted(set(files) ^ set(old)))
O1 = {'hven_interior_tests', 'hven_drivers_tests', 'hven_model_tests'}
def new_exe(f):
    if f in SEAMS: return SEAMS[f]
    if f in ALLOC: return ALLOC[f]
    return 'hven_%s_tests' % f.split('/')[0]
tab = {}
for f in files:
    tab.setdefault(new_exe(f), []).append(f)
def attrs(f):
    e = old[f][0]
    return ('O1' if e in O1 else '--') + ('U' if e == 'hven_interior_tests' else '-') + ('W' if e == 'hven_sqp_tests' else '-')
def objrel(exe, d, f):
    sub = d
    if exe == 'hven_tests': sub = ''
    rel = posixpath.relpath(f, sub or '.')
    rel = '/'.join('__' if s == '..' else s for s in rel.split('/'))
    return ('tests/' + sub + '/' if sub else 'tests/') + 'CMakeFiles/' + exe + '.dir/' + rel + '.o'
if sys.argv[1] == 'table':
    print('# new executable <- sources, with origin attributes: O1 = compiled -O1 in Release (former interior/drivers/model target); U = unity-batched (former interior);')
    print('# W = former hven_sqp_tests source (kept under if(NOT WIN32)); first column the former executable. SEAM/ALLOC executables are marked [seam]/[alloc].')
    for e in sorted(tab):
        tag = ' [seam]' if '_seam_' in e else (' [alloc]' if '_alloc_' in e else '')
        print('\n%s%s  (%d sources)' % (e, tag, len(tab[e])))
        for f in tab[e]:
            print('  %-48s %-3s  was %s' % (f, attrs(f), old[f][0]))
elif sys.argv[1] == 'objmap':
    for f in files:
        e0, d0 = old[f]
        if e0 in SEAMS.get(f, '') : pass
        if f in SEAMS or f in ALLOC: continue          # seam/alloc sources: objects of executables outside the P-SYM groups
        if e0 == 'hven_interior_tests': continue        # unity class: compared by psym_union
        if f == 'bench/test_snopt_bridge_gate.cpp': continue   # only with SNOPT detected, which this box lacks
        newd = f.split('/')[0]
        before = objrel(e0, d0, f); after = objrel('hven_%s_tests' % newd, newd, f)
        if before != after: print('%s => %s' % (before, after))
    # the one non-test source compiled into a test executable
    print('tests/sqp/CMakeFiles/hven_sqp_tests.dir/__/__/bench/ipm_corpus_leg.cpp.o => tests/bench/CMakeFiles/hven_bench_tests.dir/__/__/bench/ipm_corpus_leg.cpp.o')
