#!/usr/bin/env python3
"""M7 CLOSE GATE -- the derivations and the assertions.  One file, subcommands; every assertion returns
nonzero on failure and has a falsifier in `selftest` (a perturbed copy of a real frozen record that it
must refuse).  Nothing here is carried from the M6 close gate: every expected value is DERIVED here from
git or from the frozen records named in env.sh.

  derive  OUTDIR            write the declared expected sets/values (the pre-run derivation)
  suite   LOG INV EXPDIR TAG  assert a ctest run against the declared sets (TAG release|debug|long)
  replay  ...               assert the replay comparisons
  rig     ...               assert the golden-rig results
  census  ...               assert the census result
  configure LOG             exactly one REFERENCE line, no OFF-REFERENCE
  selftest                  the falsifiers
"""
import re, sys, os, collections, subprocess, json

OBJ = re.compile(r'^(.*?/)\(?\d+-byte object.*$')

def norm(name):
    m = OBJ.match(name)
    return m.group(1) + '(<object-bytes>)' if m else name

START = re.compile(r'^\s+Start\s+(\d+): (.*?)\s*$')
NOTRUN = re.compile(r'^\s+\d+ - (.*?) \((Skipped|Disabled)\)\s*$')
SUMMARY = re.compile(r'(\d+)% tests passed, (\d+) tests failed out of (\d+)')

def parse_ctest(text):
    started = []
    skipped, disabled = [], []
    summ = None
    for line in text.splitlines():
        m = START.match(line)
        if m: started.append(norm(m.group(2))); continue
        m = NOTRUN.match(line)
        if m:
            (skipped if m.group(2) == 'Skipped' else disabled).append(m.group(1)); continue
        m = SUMMARY.search(line)
        if m: summ = (int(m.group(2)), int(m.group(3)))
    return sorted(started), sorted(skipped), sorted(disabled), summ

def parse_inventory_total(text):
    m = re.search(r'Total Tests: (\d+)', text)
    return int(m.group(1)) if m else None

def read(p):
    with open(p, errors='replace') as f: return f.read()

def write_list(p, items):
    with open(p, 'w') as f:
        for i in items: f.write(i + '\n')

# ------------------------------------------------------------------ derive
def derive(outdir, root):
    os.makedirs(outdir, exist_ok=True)
    wx2 = os.path.join(root, 'docs/notes/data/2026-10-m7-wx2-gates/gates')
    m6 = os.path.join(root, 'docs/notes/data/2026-09-m6-close-gate')
    facts = {}
    for tag, fn in (('debug', 'suite-debug.txt'), ('release', 'suite-release.txt')):
        st, sk, di, summ = parse_ctest(read(os.path.join(wx2, fn)))
        write_list(os.path.join(outdir, f'expected-started-{tag}.txt'), st)
        write_list(os.path.join(outdir, f'expected-skipped-{tag}.txt'), sk)
        write_list(os.path.join(outdir, f'expected-disabled-{tag}.txt'), di)
        facts[tag] = dict(started=len(st), skipped=len(sk), disabled=len(di), failed=summ[0], out_of=summ[1])
        # passed == started - disabled is a property of this record, checked here, not assumed at run time
        assert summ[1] == len(st) - len(di), (tag, summ, len(st), len(di))
        assert summ[0] == 0
    # the long test: the one Debug name the Debug suite excludes; present in Release's started set, absent from Debug's
    rel = set(read(os.path.join(outdir, 'expected-started-release.txt')).splitlines())
    dbg = set(read(os.path.join(outdir, 'expected-started-debug.txt')).splitlines())
    long_names = sorted(rel - dbg)
    facts['long_test_names_in_release_not_debug'] = long_names
    facts['debug_only'] = sorted(dbg - rel)
    # git: no TEST/TEST_F/TEST_P line added or removed between the WX2 record's gated tree and this pin
    gated = subprocess.run(['git', '-C', root, 'log', '--format=%H', '-1', '--grep=WX2 -- the gates DECLARATION'],
                           capture_output=True, text=True).stdout.strip()
    d = subprocess.run(['git', '-C', root, 'diff', '-U0', gated, 'd4b8e84b0283c83e89afa9959544c25577d80519', '--', 'tests'],
                       capture_output=True, text=True).stdout
    tl = [l for l in d.splitlines() if re.match(r'^[+-]\s*(TEST|TEST_F|TEST_P|TYPED_TEST|INSTANTIATE_\w+)\(', l) or
          re.match(r'^[+-].*\b(DISABLED_|GTEST_SKIP)', l)]
    facts['wx2_declaration_commit'] = gated
    facts['test_line_delta_wx2_gated_to_pin'] = tl
    assert not tl, tl
    # replay: the 43-set (engine, cell, column) from the WX2 record's gated comparison against the M6 pin
    trip = []
    eng = None
    for line in read(os.path.join(wx2, 'replay/comparison.txt')).splitlines():
        m = re.match(r'^=== M6 CLOSE HEAD-(\w+) vs ', line)
        if m: eng = m.group(1); continue
        m = re.match(r'^\s+(\S+)\s+(\S+)\s+A=', line)
        if m and eng: trip.append(f'{eng} {m.group(1)} {m.group(2)}')
    trip = sorted(set(trip))
    write_list(os.path.join(outdir, 'expected-replay-diffs-vs-m6.txt'), trip)
    facts['replay_diffs_vs_m6'] = len(trip)
    c = collections.Counter(t.split()[0] for t in trip)
    facts['replay_diffs_by_engine'] = dict(c)
    # census: the declared movers = the walk-engine triples of the 43-set that name a census cell AND a column among the
    # comparator's 13 asserted ones (cell_id .. kkt_residual).  A mover is declared BY COORDINATE (cell, column).
    blines = [l for l in read(os.path.join(root, 'bench/baselines/2026-08-16-u0-corpus/walk_baseline.csv')).splitlines() if l and not l.startswith('#')]
    hdr13 = blines[0].split(',')[:13]
    base_cells = [l.split(',')[0] for l in blines[1:]]
    facts['census_cells'] = len(base_cells); facts['census_asserted_columns'] = hdr13
    mv = sorted(f"{t.split()[1]} {t.split()[2]}" for t in trip if t.startswith('walk ') and t.split()[1] in base_cells and t.split()[2] in hdr13)
    write_list(os.path.join(outdir, 'expected-census-movers.txt'), mv)
    facts['census_declared_movers'] = mv
    # rig: whole-inventory delta over the Release suite inventory, from M6's own numbers
    rig_full = parse_inventory_total(read(os.path.join(m6, 'rig/rig-inventory-full.out')))
    rel_inv = parse_inventory_total(read(os.path.join(m6, 'suites/suite-inv-release.out')))
    facts['m6_rig_full_inventory'] = rig_full; facts['m6_release_inventory'] = rel_inv
    facts['rig_inventory_delta'] = rig_full - rel_inv
    with open(os.path.join(outdir, 'facts.json'), 'w') as f: json.dump(facts, f, indent=1, sort_keys=True)
    print(json.dumps(facts, indent=1, sort_keys=True))

# ------------------------------------------------------------------ assertions
def lines(p): return [l for l in read(p).splitlines()] if os.path.exists(p) else None

def check_suite(logtext, invtext, expdir, tag):
    """returns list of failure strings"""
    bad = []
    st, sk, di, summ = parse_ctest(logtext)
    if tag == 'long':
        facts = json.load(open(os.path.join(expdir, 'facts.json')))
        if summ is None or summ != (0, 1): bad.append(f'long test: summary {summ}, expected (0 failed, 1)')
        if len(st) != 1 or st[0] not in facts['long_test_names_in_release_not_debug']:
            bad.append(f'long test: started {st}')
        return bad
    exp_st = lines(os.path.join(expdir, f'expected-started-{tag}.txt'))
    exp_sk = lines(os.path.join(expdir, f'expected-skipped-{tag}.txt'))
    exp_di = lines(os.path.join(expdir, f'expected-disabled-{tag}.txt'))
    if summ is None: bad.append('no ctest summary line')
    elif summ[0] != 0: bad.append(f'{summ[0]} failed')
    elif summ[1] != len(exp_st) - len(exp_di): bad.append(f'ran {summ[1]} != declared {len(exp_st) - len(exp_di)}')
    if st != exp_st:
        a, b = collections.Counter(st), collections.Counter(exp_st)
        bad.append(f'started set differs: {sum((a-b).values())} extra {list((a-b))[:3]}, {sum((b-a).values())} missing {list((b-a))[:3]}')
    if sk != exp_sk: bad.append(f'skipped set {sk} != declared {exp_sk}')
    if di != exp_di: bad.append(f'disabled set {di} != declared {exp_di}')
    if re.search(r'\(Failed\)|\(Timeout\)|\(SEGFAULT\)|Subprocess aborted|\(Not Run\)', logtext): bad.append('a non-pass status line present')
    tot = parse_inventory_total(invtext) if invtext is not None else None
    # Debug excludes the long test by -E: its inventory is the full inventory (long test included)
    full = len(exp_st) + (1 if tag == 'debug' else 0)
    if tot != full: bad.append(f'inventory {tot} != declared {full}')
    return bad

def check_configure(text):
    n = text.count('-> REFERENCE (route pins asserted)'); o = text.count('OFF-REFERENCE')
    bad = []
    if n != 1: bad.append(f'REFERENCE line count {n} != 1')
    if o: bad.append('OFF-REFERENCE present')
    if 'EIGEN_MAX_ALIGN_BYTES resolved to 32' not in text: bad.append('align line missing / not 32')
    return bad

RIG_FAIL = 'Arms/InteriorPointTrace.P5_InertiaBeforeFactorizationIsAnExplicitState/sqp-old@mkl'

def check_rig(scoped, full, inv_scoped, inv_full, rel_inv, delta):
    bad = []
    for nm, txt in (('scoped', scoped), ('full', full)):
        m = SUMMARY.search(txt)
        if not m: bad.append(f'{nm}: no summary'); continue
        failed = re.findall(r'^\s*\d+ - (.*?) \((?:Failed|Subprocess aborted|Timeout|Exception[^)]*)\)\s*$', txt, re.M)
        if failed != [RIG_FAIL] or int(m.group(2)) != 1:
            bad.append(f'{nm}: failures {failed} (count {m.group(2)}), expected exactly [{RIG_FAIL}]')
    if parse_inventory_total(inv_scoped) != 84: bad.append(f'scoped inventory {parse_inventory_total(inv_scoped)} != 84')
    ctl = re.findall(r'FailByDesignControl\.\w+', scoped)
    passed_ctl = set(re.findall(r'Test\s+#\d+: (FailByDesignControl\.\w+) \.+\s+Passed', scoped))
    if len(passed_ctl) != 3: bad.append(f'FailByDesignControl passed {sorted(passed_ctl)} != 3')
    exp_full = rel_inv + delta
    if parse_inventory_total(inv_full) != exp_full: bad.append(f'full inventory {parse_inventory_total(inv_full)} != declared {exp_full} (Release {rel_inv} + M6 delta {delta})')
    return bad

def compare_line(text):
    out = []
    for m in re.finditer(r'=== (.*?) === cells compared: (\d+)\s+columns compared: (\d+)\s+differences: (\d+)', text):
        out.append((m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4))))
    return out

def check_replay_exact(text):
    bad = []; c = compare_line(text)
    if len(c) != 3: bad.append(f'{len(c)} comparison headers != 3')
    for lab, cells, cols, diffs in c:
        if cells != 27 or cols != 75: bad.append(f'{lab}: {cells} cells x {cols} cols != 27 x 75')
        if diffs: bad.append(f'{lab}: {diffs} differences')
    return bad

def check_replay_m6set(text, expdir):
    bad = []; c = compare_line(text)
    if len(c) != 3: bad.append(f'{len(c)} comparison headers != 3')
    facts = json.load(open(os.path.join(expdir, 'facts.json')))
    trip = []; eng = None
    for line in text.splitlines():
        m = re.match(r'^=== M6 CLOSE HEAD-(\w+) vs ', line)
        if m: eng = m.group(1); continue
        m = re.match(r'^\s+(\S+)\s+(\S+)\s+A=', line)
        if m and eng: trip.append(f'{eng} {m.group(1)} {m.group(2)}')
    exp = lines(os.path.join(expdir, 'expected-replay-diffs-vs-m6.txt'))
    if sorted(set(trip)) != exp:
        bad.append(f'measured diff set != declared: extra {sorted(set(trip)-set(exp))[:5]} missing {sorted(set(exp)-set(trip))[:5]}')
    for lab, cells, cols, diffs in c:
        if cells != 27 or cols != 75: bad.append(f'{lab}: shape {cells}x{cols}')
    return bad

def census_summary(compare_txt):
    m = re.search(r'(\d+) baseline cells, (\d+) fresh cells, (\d+) mismatches', compare_txt)
    return tuple(int(x) for x in m.groups()) if m else None

def check_census_gated(txt, expdir):
    """The gated comparison is the verdict: every MISMATCH cell may differ only in declared (cell, column) coordinates."""
    bad = []
    s = census_summary(txt)
    if s is None: return ['no closing tally']
    if s[0] != 57 or s[1] != 57: bad.append(f'cells {s[0]}/{s[1]} != 57/57')
    facts = json.load(open(os.path.join(expdir, 'facts.json')))
    hdr = facts['census_asserted_columns']
    declared = set(tuple(x.split()) for x in lines(os.path.join(expdir, 'expected-census-movers.txt')))
    ls = txt.splitlines(); i = 0; found = 0
    while i < len(ls):
        m = re.match(r'^MISMATCH (\S+)$', ls[i])
        if m:
            found += 1; cell = m.group(1)
            b = ls[i+1].split(':', 1)[1].strip().split(',') if i + 2 < len(ls) and ls[i+1].lstrip().startswith('base') else None
            f = ls[i+2].split(':', 1)[1].strip().split(',') if b is not None and ls[i+2].lstrip().startswith('fresh') else None
            if b is None or f is None or len(b) != 13 or len(f) != 13:
                bad.append(f'{cell}: malformed MISMATCH block')
            else:
                cols = [hdr[k] for k in range(13) if b[k] != f[k]]
                und = [c for c in cols if (cell, c) not in declared]
                if und: bad.append(f'UNDECLARED mover {cell}: columns {und}')
            i += 3; continue
        i += 1
    if found != s[2]: bad.append(f'MISMATCH blocks {found} != tally {s[2]}')
    if re.search(r'^(MISSING|EXTRA) CELL ', txt, re.M): bad.append('roster MISSING/EXTRA')
    return bad

# ------------------------------------------------------------------ cli
def main(a):
    if not a: print(__doc__); return 2
    cmd = a[0]
    if cmd == 'derive':
        derive(a[1], a[2]); return 0
    if cmd == 'configure':
        bad = check_configure(read(a[1]))
    elif cmd == 'suite':
        log, inv, expdir, tag = a[1:5]
        bad = check_suite(read(log), read(inv) if os.path.exists(inv) else None, expdir, tag)
    elif cmd == 'replay-exact':
        bad = check_replay_exact(read(a[1]))
    elif cmd == 'replay-m6':
        bad = check_replay_m6set(read(a[1]), a[2])
    elif cmd == 'rig':
        d, expdir, rel_inv_file = a[1], a[2], a[3]
        facts = json.load(open(os.path.join(expdir, 'facts.json')))
        rel_inv = parse_inventory_total(read(rel_inv_file))
        bad = check_rig(read(f'{d}/rig-ctest-scoped.out'), read(f'{d}/rig-ctest-full.out'), read(f'{d}/rig-inventory-scoped.out'),
                        read(f'{d}/rig-inventory-full.out'), rel_inv, facts['rig_inventory_delta'])
    elif cmd == 'census-gated':
        bad = check_census_gated(read(a[1]), a[2])
    elif cmd == 'selftest':
        return selftest(a[1], a[2])
    else:
        print('unknown subcommand'); return 2
    for b in bad: print('CHECK_FAIL', cmd, b)
    print(f'CHECK_{cmd.upper().replace("-", "_")}_RC={1 if bad else 0}')
    return 1 if bad else 0

def selftest(root, expdir):
    """Falsifiers: each assertion is run on a REAL frozen record (must pass) and on a perturbed copy (must refuse)."""
    wx2 = os.path.join(root, 'docs/notes/data/2026-10-m7-wx2-gates/gates')
    m6 = os.path.join(root, 'docs/notes/data/2026-09-m6-close-gate')
    fails = 0
    def expect(name, bad, want_bad):
        nonlocal fails
        ok = bool(bad) == want_bad
        print(('SELFTEST_OK   ' if ok else 'SELFTEST_FAIL ') + name + ('  [refused: ' + bad[0][:90] + ']' if bad else '  [accepted]'))
        if not ok: fails += 1
    dbg = read(os.path.join(wx2, 'suite-debug.txt')); rel = read(os.path.join(wx2, 'suite-release.txt'))
    inv_d = 'Total Tests: %d\n' % (3233 + 1); inv_r = 'Total Tests: %d\n' % 3234
    expect('suite debug: the WX2 record against its own derivation', check_suite(dbg, inv_d, expdir, 'debug'), False)
    expect('suite release: the WX2 record against its own derivation', check_suite(rel, inv_r, expdir, 'release'), False)
    expect('suite: a Start line removed', check_suite(dbg.replace('Start    1: Version.NonNullNonEmptyMatchesProjectVersion', '', 1), inv_d, expdir, 'debug'), True)
    expect('suite: a failed test', check_suite(dbg.replace('0 tests failed out of 3231', '1 tests failed out of 3231'), inv_d, expdir, 'debug'), True)
    expect('suite: a skipped test newly skipped', check_suite(dbg.replace('(Skipped)', '(Skipped)\n\t999 - New.Test (Skipped)', 1), inv_d, expdir, 'debug'), True)
    expect('suite: a test newly disabled', check_suite(rel + '\n\t999 - New.Test (Disabled)\n', inv_r, expdir, 'release'), True)
    expect('suite: inventory off by one', check_suite(dbg, 'Total Tests: 3235\n', expdir, 'debug'), True)
    expect('suite: no summary line', check_suite(dbg.replace('tests passed', 'tests xxxxxx'), inv_d, expdir, 'debug'), True)
    expect('suite: a (Failed) status line', check_suite(dbg + '\n\t12 - X.Y (Failed)\n', inv_d, expdir, 'debug'), True)
    lg = read(os.path.join(wx2, 'suite-debug-long.txt'))
    expect('long: the WX2 record', check_suite(lg, None, expdir, 'long'), False)
    expect('long: a failure', check_suite(lg.replace('0 tests failed', '1 tests failed'), None, expdir, 'long'), True)
    cr = read(os.path.join(wx2, 'configure-release.txt'))
    expect('configure: the WX2 record', check_configure(cr), False)
    expect('configure: OFF-REFERENCE', check_configure(cr.replace('-> REFERENCE (route pins asserted)', '-> OFF-REFERENCE (portable arms)')), True)
    expect('configure: line absent', check_configure(cr.replace('-> REFERENCE (route pins asserted)', '')), True)
    expect('configure: line twice', check_configure(cr + '\n-> REFERENCE (route pins asserted)\n'), True)
    expect('configure: align not 32', check_configure(cr.replace('resolved to 32', 'resolved to 16')), True)
    ex = read(os.path.join(wx2, 'replay/exact-vs-previous-record.txt'))
    expect('replay-exact: the WX2 record', check_replay_exact(ex), False)
    expect('replay-exact: one difference', check_replay_exact(ex.replace('differences: 0', 'differences: 1', 1)), True)
    expect('replay-exact: shape 26 cells', check_replay_exact(ex.replace('cells compared: 27', 'cells compared: 26', 1)), True)
    expect('replay-exact: a comparison missing', check_replay_exact('\n'.join(ex.splitlines()[:2])), True)
    cm = read(os.path.join(wx2, 'replay/comparison.txt'))
    expect('replay-m6: the WX2 record', check_replay_m6set(cm, expdir), False)
    expect('replay-m6: a declared triple vanishes', check_replay_m6set(cm.replace('f7_n1000_bound_neutral           qp_minors                          A=2  B=3\n', '', 1), expdir), True)
    expect('replay-m6: an undeclared triple appears', check_replay_m6set(cm + '    f7_n800_path_warm                factorizations                     A=1  B=2\n', expdir), True)
    cc = read(os.path.join(m6, 'census/compare.txt'))
    expect('census-gated: the M6 record (0 mismatches)', check_census_gated(cc, expdir), False)
    blk = lambda cell, col, bv, fv: ('MISMATCH %s\n  base : %s\n  fresh: %s\n' % (cell, ','.join(bv), ','.join(fv)))
    row = ['c','F7','1000','bound','neutral','0','Optimal','1','2','0','1','1','6.0e-14']
    hdrs = json.load(open(os.path.join(expdir, 'facts.json')))['census_asserted_columns']
    def mism(cell, colname, newv):
        k = hdrs.index(colname); f = list(row); f[k] = newv
        return 'MISMATCH %s\n  base : %s\n  fresh: %s\n' % (cell, ','.join(row), ','.join(f))
    base_ok = cc.replace('0 mismatches', '%d mismatches')
    expect('census-gated: an undeclared cell mismatching', check_census_gated(mism('f7_n1000_path_warm', 'qp_minors', '3') + '57 baseline cells, 57 fresh cells, 1 mismatches\n', expdir), True)
    expect('census-gated: a declared coordinate is tolerated', check_census_gated(mism('f7_n1000_bound_neutral', 'qp_minors', '3') + '57 baseline cells, 57 fresh cells, 1 mismatches\n', expdir), False)
    expect('census-gated: a declared cell, an undeclared column', check_census_gated(mism('f7_n1000_bound_neutral', 'factorizations', '3') + '57 baseline cells, 57 fresh cells, 1 mismatches\n', expdir), True)
    expect('census-gated: roster EXTRA', check_census_gated(cc + '\nEXTRA CELL zz\n', expdir), True)
    expect('census-gated: 56 cells', check_census_gated(cc.replace('57 baseline cells, 57 fresh', '57 baseline cells, 56 fresh'), expdir), True)
    expect('census-gated: tally disagrees with blocks', check_census_gated(cc.replace('0 mismatches', '2 mismatches'), expdir), True)
    # rig
    sc, fu = read(os.path.join(m6, 'rig/rig-ctest-scoped.out')), read(os.path.join(m6, 'rig/rig-ctest-full.out'))
    isc, ifu = read(os.path.join(m6, 'rig/rig-inventory-scoped.out')), read(os.path.join(m6, 'rig/rig-inventory-full.out'))
    reli = parse_inventory_total(read(os.path.join(m6, 'suites/suite-inv-release.out')))
    delta = json.load(open(os.path.join(expdir, 'facts.json')))['rig_inventory_delta']
    expect('rig: the M6 record against the declared shape', check_rig(sc, fu, isc, ifu, reli, delta), False)
    expect('rig: a second failure', check_rig(sc.replace('1 tests failed out of 84', '2 tests failed out of 84'), fu, isc, ifu, reli, delta), True)
    expect('rig: the designed failure passes (no failure at all)', check_rig(sc.replace('(Failed)', '(Passed)'), fu, isc, ifu, reli, delta), True)
    expect('rig: inventory moved', check_rig(sc, fu, isc, ifu.replace('Total Tests: 2628', 'Total Tests: 2629'), reli, delta), True)
    expect('rig: a control missing', check_rig(sc.replace('FailByDesignControl.SqpSeamDeclaresTheInertiaSurfaceTheFailingTraceNeeds ', 'FailByDesignControl.SqpSeamDeclaresTheInertiaSurfaceTheFailingTraceNeeds X '), fu, isc, ifu, reli, delta), True)
    print(f'SELFTEST_FAILS={fails}')
    return 1 if fails else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
