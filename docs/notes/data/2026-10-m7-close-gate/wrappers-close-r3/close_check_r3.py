#!/usr/bin/env python3
"""M7 CLOSE GATE r3 -- the census verdict (with the kkt_residual floor fix) and the r2 guard falsifiers.

  census-r3 BASELINE R2CENSUS FRESH MOVERS_R3 EXPDIR   assert the r3 census (exit 1 on any CHECK_FAIL)
  selftest-census ROOT                                 falsifiers for census-r3 (real records + perturbed copies)
  falsify-r2-guards BENCH R2SCRIPT R2LOGDIR OUTDIR     run r2's two guard-replacement assertions on mutated inputs

The r1 checker (wrappers-close/close_check.py) is imported, never edited.  The artifact being fixed: its check_census_gated reads
`MISMATCH` blocks of the census comparator and lists every column whose spelling differs between base and fresh -- including a
kkt_residual that moved inside the comparator's gate (relative 1e-5 with the 1e-13 absolute floor) in a cell that was a MISMATCH only
because of ANOTHER column (f7_n1000_bound_neutral: 6.3e-14 -> 2.3e-16 alongside the declared qp_minors).  Here every residual
comparison goes through the comparator's own `residual_columns_agree`, and the verdict reads the CSVs, not the comparator's text.
"""
import sys, os, re, json, csv, importlib.util, subprocess, tempfile, shutil

HERE = os.path.dirname(os.path.abspath(__file__))
def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path); m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m
R1 = _load('close_check_r1', os.path.join(HERE, '..', 'wrappers-close', 'close_check.py'))
CMP = _load('census_compare', os.path.join(HERE, '..', '..', '..', '..', '..', 'scripts', 'census_compare.py'))
GATE = 1e-5
ASSERTED = 13
RES = 12   # kkt_residual
STATUS = 6; WALL = 13

def load_rows(path):
    rows = {}; hdr = None
    for line in open(path, newline=''):
        line = line.rstrip('\r\n')
        if not line or line.startswith('#'): continue
        f = line.split(',')
        if f[0] == 'cell_id':
            hdr = hdr or f; continue
        if len(f) >= ASSERTED: rows[f[0]] = f
    return hdr, rows

def cols_differing(a, b):
    """Indices (of the 13 asserted) where a and b differ; the residual column counts only OUTSIDE the gate (floor included)."""
    out = []
    for i in range(ASSERTED):
        if a[i] == b[i]: continue
        if i == RES and CMP.residual_columns_agree(a[i], b[i], GATE): continue
        out.append(i)
    return out

def parse_movers(path):
    mv = {}
    for l in open(path):
        l = l.rstrip('\n')
        if not l or l.startswith('#'): continue
        c, tier, commits, why = l.split('|', 3); mv[c] = (tier, commits, why)
    return mv

def check_census_r3(base_p, r2_p, fresh_p, movers_p, expdir):
    bad = []
    bh, base = load_rows(base_p); _, r2 = load_rows(r2_p); fh, fresh = load_rows(fresh_p)
    facts = json.load(open(os.path.join(expdir, 'facts.json'))); hdr = facts['census_asserted_columns']
    if bh[:ASSERTED] != hdr or fh[:ASSERTED] != hdr: bad.append('header disagrees with the declared 13 asserted columns')
    if len(base) != 57: bad.append(f'baseline has {len(base)} cells')
    if set(fresh) != set(base): bad.append(f'roster: missing {sorted(set(base)-set(fresh))} extra {sorted(set(fresh)-set(base))}')
    orig = {}
    for l in open(os.path.join(expdir, 'expected-census-movers.txt')):
        if l.strip(): c, col = l.split(); orig.setdefault(c, set()).add(col)
    mv = parse_movers(movers_p)
    if len(mv) != 20: bad.append(f'declared r3 movers {len(mv)} != 20')
    both = sorted(set(mv) & set(orig))
    if both: bad.append(f'a cell in both the r1 and the r3 mover lists: {both}')
    declared = set(mv) | set(orig)
    if len(declared) != 26: bad.append(f'declared mover cells {len(declared)} != 26')
    for c in declared:
        if c not in base: bad.append(f'declared mover {c} not in the baseline')
        if c not in r2: bad.append(f'declared mover {c} not in the r2 record (no prediction)')
    for c, (tier, _, _) in mv.items():
        if c not in base: continue
        status = base[c][STATUS]; wall = float(base[c][WALL])
        derived = 'T3' if status.startswith('dnf_') else ('T2' if (wall >= 1800 or c == 'f7_n10000_path_physics') else 'T1')
        if derived != tier: bad.append(f'{c}: declared tier {tier} != derived {derived} from the baseline')
    for c in sorted(base):
        if c not in fresh: continue
        b, f = base[c], fresh[c]
        if c in orig:
            und = [hdr[i] for i in cols_differing(b, f) if hdr[i] not in orig[c]]
            if und: bad.append(f'UNDECLARED column mover {c}: {und} (declared {sorted(orig[c])})')
            p = cols_differing(r2[c], f) if c in r2 else [0]
            if p: bad.append(f'{c}: fresh differs from the r2 prediction in {[hdr[i] for i in p]}')
        elif c in mv:
            tier = mv[c][0]
            if tier == 'T3':
                if not f[STATUS].startswith('dnf_'): bad.append(f'{c}: T3 status class dnf_* expected, fresh {f[STATUS]}')
            else:
                p = cols_differing(r2[c], f) if c in r2 else [0]
                if p: bad.append(f'{c}: fresh differs from the r2 prediction in {[hdr[i] for i in p]}')
        else:
            d = cols_differing(b, f)
            if d: bad.append(f'UNDECLARED mover {c}: columns {[hdr[i] for i in d]}')
    return bad

def cli_census(a):
    bad = check_census_r3(*a[:5])
    for b in bad: print('CHECK_FAIL census-r3', b)
    print(f'CHECK_CENSUS_R3_RC={1 if bad else 0}'); return 1 if bad else 0

# ------------------------------------------------------------------------------------------------ selftest
def write_csv(path, hdr, rows):
    with open(path, 'w') as f:
        f.write(','.join(hdr) + '\n')
        for r in rows.values(): f.write(','.join(r) + '\n')

def selftest_census(root):
    rec = os.path.join(root, 'docs/notes/data/2026-10-m7-close-gate')
    exp = os.path.join(rec, 'derivation'); base_p = os.path.join(root, 'bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv')
    r2_p = os.path.join(rec, 'gates/census/walk_census.csv'); mv_p = os.path.join(HERE, 'expected-census-movers-r3.txt')
    fails = 0
    def expect(name, bad, want_bad):
        nonlocal fails
        ok = bool(bad) == want_bad
        print(('SELFTEST_OK   ' if ok else 'SELFTEST_FAIL ') + name + ('  [refused: ' + bad[0][:100] + ']' if bad else '  [accepted]')); fails += 0 if ok else 1
    td = tempfile.mkdtemp(prefix='r3-selftest-')
    hdr, r2 = load_rows(r2_p); _, base = load_rows(base_p)
    def variant(name, mutate, want_bad, r2_mut=None):
        f = {k: list(v) for k, v in r2.items()}; mutate(f)
        p = os.path.join(td, name + '.csv'); write_csv(p, hdr[:ASSERTED], f)
        rp = r2_p
        if r2_mut:
            g = {k: list(v) for k, v in r2.items()}; r2_mut(g); rp = os.path.join(td, name + '-r2.csv'); write_csv(rp, hdr[:ASSERTED], g)
        expect(name, check_census_r3(base_p, rp, p, mv_p, exp), want_bad)
    # THE REAL r2 CENSUS = what a reproducing run looks like
    expect('r3 checker: the real r2 census reproduces its own prediction (non-movers = baseline, movers = r2 row)', check_census_r3(base_p, r2_p, r2_p, mv_p, exp), False)
    # THE OLD CHECKER'S ARTIFACT, on the real r2 compare text: it lists f7_n1000_bound_neutral kkt_residual
    old = R1.check_census_gated(open(os.path.join(rec, 'gates/census/compare-gated.txt')).read(), exp)
    expect('r1 checker on the real r2 compare text lists the floor-artifact cell (the bug is real)', [b for b in old if 'f7_n1000_bound_neutral' in b and 'kkt_residual' in b], True)
    b13 = base['f7_n1000_bound_neutral'][RES]; r13 = r2['f7_n1000_bound_neutral'][RES]
    print(f'NOTE floor artifact: f7_n1000_bound_neutral kkt_residual {b13} -> {r13}; inside the 1e-13 floor: {CMP.residual_columns_agree(b13, r13, GATE)}')
    # non-movers
    nm = 'f7_n20000_bound_activity'
    variant('non-mover: a counter moves', lambda f: f[nm].__setitem__(7, str(int(f[nm][7]) + 1)), True)
    variant('non-mover: status moves', lambda f: f[nm].__setitem__(STATUS, 'MaxIter'), True)
    variant('non-mover: residual moves beyond the gate', lambda f: f[nm].__setitem__(RES, '4.0e-10'), True)
    base_res = float(base[nm][RES])
    variant('non-mover: residual moves INSIDE the 1e-13 floor (accepted)', lambda f: f[nm].__setitem__(RES, '%.9e' % (base_res + 5e-14)), False)
    variant('non-mover: a path cell not on the list', lambda f: f['f7_n800_path_activity'].__setitem__(7, str(int(f['f7_n800_path_activity'][7]) + 1)), True)
    # movers
    variant('mover: factorizations differ from the r2 prediction', lambda f: f['f7_n1000_path_corrupted'].__setitem__(7, '2976'), True)
    variant('mover: qp_minors differ from the r2 prediction', lambda f: f['f7_n800_path_neutral'].__setitem__(8, '1'), True)
    variant('mover: status differs from the r2 prediction (n2000 neutral back to NumericalError)', lambda f: f['f7_n2000_path_neutral'].__setitem__(STATUS, 'NumericalError'), True)
    mres = float(r2['f7_n750_path_neutral_control'][RES])
    variant('mover: residual outside the gate', lambda f: f['f7_n750_path_neutral_control'].__setitem__(RES, '%.9e' % (mres * 1.01 + 1e-9)), True)
    variant('mover: residual inside the gate (accepted)', lambda f: f['f7_n750_path_neutral_control'].__setitem__(RES, '%.9e' % (mres * (1 + 1e-7))), False)
    variant('mover n10000_path_physics: back to Optimal (a convergence the prediction does not hold)', lambda f: f['f7_n10000_path_physics'].__setitem__(STATUS, 'Optimal'), True)
    # T3
    variant('T3 mover: any dnf_* status is the class (dnf_budget accepted)', lambda f: f['f7_n5000_path_corrupted'].__setitem__(STATUS, 'dnf_budget'), False)
    variant('T3 mover: a terminating status is refused', lambda f: f['f7_n5000_path_corrupted'].__setitem__(STATUS, 'Optimal'), True)
    # original six
    variant('r1-declared mover: a column outside the declared coordinates moves vs the baseline (and the prediction)', lambda f: f['f7_n2000_bound_neutral'].__setitem__(7, str(int(f['f7_n2000_bound_neutral'][7]) + 1)), True,
            r2_mut=lambda g: g['f7_n2000_bound_neutral'].__setitem__(7, str(int(g['f7_n2000_bound_neutral'][7]) + 1)))
    variant('r1-declared mover: fresh differs from the r2 prediction', lambda f: f['f7_n5000_bound_neutral'].__setitem__(8, '4'), True)
    # roster / structure
    def drop(f): del f['f7_n750_path_neutral_control']
    variant('roster: a cell missing', drop, True)
    def extra(f): f['zz_extra'] = ['zz_extra'] + list(f['f7_n800_path_activity'])[1:]
    variant('roster: an extra cell', extra, True)
    # mover list integrity
    tmp = os.path.join(td, 'movers-short.txt'); open(tmp, 'w').write(''.join(open(mv_p).readlines()[:-1]))
    expect('mover list: one declared mover removed', check_census_r3(base_p, r2_p, r2_p, tmp, exp), True)
    tmp2 = os.path.join(td, 'movers-wrongtier.txt'); open(tmp2, 'w').write(open(mv_p).read().replace('f7_n5000_path_neutral|T2', 'f7_n5000_path_neutral|T1'))
    expect('mover list: a wrong tier class', check_census_r3(base_p, r2_p, r2_p, tmp2, exp), True)
    shutil.rmtree(td)
    print(f'SELFTEST_FAILS={fails}'); return 1 if fails else 0

# ------------------------------------------------------------------------------------------------ r2 guard falsifiers
def falsify_r2_guards(bench, r2script, r2logdir, outdir):
    """Runs the code of r2's two guard-replacement assertions (extracted from the frozen suitev1_r2.sh, not re-typed) on the real inputs
    (must accept) and on mutated inputs (must refuse)."""
    text = open(r2script).read(); fails = 0
    os.makedirs(outdir, exist_ok=True)
    def expect(name, rc, want_refuse):
        nonlocal fails
        ok = (rc != 0) == want_refuse
        print(('SELFTEST_OK   ' if ok else 'SELFTEST_FAIL ') + name + f'  [rc={rc}]'); fails += 0 if ok else 1
    # (1) the set-pin assertion: the python heredoc, run in a sandbox whose schema/set_pin.py serves FRESH and OLD from files
    m = re.search(r"python3 -B - > \"\$OUT/r2-set-pin-diff.log\" 2>&1 <<'PY'\n(.*?)\nPY\n", text, re.S)
    assert m, 'set-pin heredoc not found in the r2 script'
    code = m.group(1)
    old_real = json.load(open(os.path.join(bench, 'sets/hs-27.json')))
    def run_pin(name, old, fresh, want_refuse):
        sb = tempfile.mkdtemp(prefix='r3-pin-'); os.makedirs(os.path.join(sb, 'schema')); os.makedirs(os.path.join(sb, 'runners'))
        json.dump(old, open(os.path.join(sb, 'old.json'), 'w')); json.dump(fresh, open(os.path.join(sb, 'fresh.json'), 'w'))
        open(os.path.join(sb, 'schema', 'set_pin.py'), 'w').write(
            "import os\n_d=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))\nDEFAULT_BINARY='x'\nPIN_PATH=os.path.join(_d,'old.json')\n"
            "def pin_bytes(b):\n    return open(os.path.join(_d,'fresh.json'),'rb').read()\n")
        r = subprocess.run([sys.executable, '-B', '-I', '-c', 'import sys; sys.path.insert(0, %r)\n' % sb + code], cwd=sb, capture_output=True, text=True)
        open(os.path.join(outdir, 'pin-' + re.sub(r'\W+', '_', name) + '.log'), 'w').write(r.stdout + r.stderr)
        shutil.rmtree(sb); expect('set-pin assertion: ' + name, r.returncode, want_refuse)
    PIN = 'd4b8e84b0283c83e89afa9959544c25577d80519'
    fresh = dict(old_real); fresh['hven_pin_sha'] = PIN
    run_pin('real committed pin vs the regenerated one (differs in exactly hven_pin_sha)', old_real, fresh, False)
    f2 = dict(fresh); f2['set_version'] = 'hs-27 v2'
    run_pin('a second field differs (set_version)', old_real, f2, True)
    f3 = dict(fresh); f3['problems'] = list(fresh['problems'])[:-1]
    run_pin('a problem row missing in the regenerated set', old_real, f3, True)
    f4 = dict(fresh); f4['hven_pin_sha'] = '0' * 40
    run_pin('the regenerated pin is not d4b8e84b', old_real, f4, True)
    o5 = dict(old_real); o5['hven_pin_sha'] = 'deadbeef' + old_real['hven_pin_sha'][8:]
    run_pin('the committed pin is not 3904a477 (the stale-pin premise no longer holds)', o5, fresh, True)
    run_pin('no difference at all (the pin file was regenerated)', fresh, fresh, True)
    # (2) the unittest assertion: the three statements extracted from the r2 script, evaluated against a log
    stm = [l for l in text.splitlines() if l.startswith('fails=') or l.startswith('exp=') or l.startswith('[ "$fails" = "$exp" ]')]
    assert len(stm) == 3, stm
    real_log = open(os.path.join(r2logdir, 'r2-unittest.log')).read()
    def run_ut(name, log, want_refuse):
        d = tempfile.mkdtemp(prefix='r3-ut-'); open(os.path.join(d, 'r2-unittest.log'), 'w').write(log)
        script = 'OUT=%s\nworst=0\n' % d + '\n'.join(stm) + '\nexit $rc\n'   # the extracted line folds rc into worst; the verdict is its own rc
        r = subprocess.run(['bash', '-c', script], capture_output=True, text=True); shutil.rmtree(d)
        expect('unittest assertion: ' + name, r.returncode, want_refuse)
    run_ut('the real r2 log (exactly the two stale-pin tests)', real_log, False)
    fl = [l for l in real_log.splitlines() if l.startswith('FAIL:')]
    run_ut('a third failure appears', real_log + '\nFAIL: test_something_else (test_x.Y.test_something_else)\n', True)
    run_ut('an ERROR appears', real_log + '\nERROR: test_boom (test_x.Y.test_boom)\n', True)
    run_ut('one of the two stale-pin failures is missing', real_log.replace(fl[0], 'ok'), True)
    run_ut('both missing (the suite went green)', real_log.replace(fl[0], 'ok').replace(fl[1], 'ok'), True)
    run_ut('a different test fails in place of one', real_log.replace(fl[1], 'FAIL: test_other (test_x.Y.test_other)'), True)
    print(f'SELFTEST_FAILS={fails}'); return 1 if fails else 0

if __name__ == '__main__':
    a = sys.argv[1:]
    if not a: print(__doc__); sys.exit(2)
    if a[0] == 'census-r3': sys.exit(cli_census(a[1:]))
    if a[0] == 'selftest-census': sys.exit(selftest_census(a[1]))
    if a[0] == 'falsify-r2-guards': sys.exit(falsify_r2_guards(*a[1:5]))
    print('unknown'); sys.exit(2)
