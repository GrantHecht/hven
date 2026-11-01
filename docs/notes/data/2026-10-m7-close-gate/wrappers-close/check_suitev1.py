#!/usr/bin/env python3
"""Suite v1 end-to-end assertions.  usage: check_suitev1.py RUNDIR_HS RUNDIR_CUTEST HARNESS_ROOT PIN_SHA [--selftest]
Declared (pre-run) criteria, derived from the harness's own split pin and cmake pin -- nothing typed:
  * per set: rows == TRAIN problems x 4 lanes, every (problem, lane) present exactly once;
  * every row: split == TRAIN, hessian_provenance non-empty, options_label == default; no HOLDOUT problem appears;
  * provenance block: schema, harness.sha, hven.pin.sha == the pin, split.policy, mkl.num_threads == 1, run.mode serial,
    apple.accelerate / windows UNOBSERVED;
  * the IPOPT / SNOPT lanes carry no skipped-lane stamp (a skipped lane would be a stamped reason, never rows)."""
import csv, io, json, re, sys, collections, os

LANES = ['hven-ipm', 'hven-sqp', 'ipopt', 'snopt']

def load(path):
    prov, rows, hdr = {}, [], None
    with open(path, newline='') as f:
        text = f.read()
    for line in text.splitlines():
        if line.startswith('#'):
            m = re.match(r'^# ([A-Za-z0-9_.]+): (.*)$', line)
            if m: prov.setdefault(m.group(1), []).append(m.group(2))
    body = '\n'.join(l for l in text.splitlines() if not l.startswith('#'))
    return prov, list(csv.DictReader(io.StringIO(body)))

def assignments(root):
    return json.load(open(os.path.join(root, 'sets/split-v1.json')))['assignments']

def train_problems(root, pset):
    return sorted(x['problem'] for x in assignments(root) if x['problem_set'] == pset and x['split'] == 'TRAIN')

def check(path, pset, root, pin):
    bad = []
    prov, rows = load(path)
    tr = train_problems(root, pset)
    hold = {x['problem'] for x in assignments(root) if x['problem_set'] == pset and x['split'] == 'HOLDOUT'}
    if not tr: bad.append(f'{pset}: the split pin names no TRAIN problem')
    exp = sorted((p, l) for p in tr for l in LANES)
    got = sorted((r['problem'], r['lane']) for r in rows)
    if got != exp:
        a, b = collections.Counter(got), collections.Counter(exp)
        bad.append(f'{pset}: row set != TRAIN x lanes ({len(got)} vs {len(exp)}): missing {sorted((b-a))[:4]} extra {sorted((a-b))[:4]}')
    for r in rows:
        if r['split'] != 'TRAIN': bad.append(f"{r['cell_id']}: split {r['split']}")
        if r['problem'] in hold: bad.append(f"{r['cell_id']}: a HOLDOUT problem")
        if not r['hessian_provenance']: bad.append(f"{r['cell_id']}: empty hessian_provenance")
        if r['options_label'] != 'default': bad.append(f"{r['cell_id']}: options {r['options_label']}")
        if not r['status'] or not r['outcome']: bad.append(f"{r['cell_id']}: no status/outcome")
    def one(k, want=None, pat=None):
        v = prov.get(k)
        if not v: bad.append(f'provenance {k} missing'); return
        if want is not None and v[0] != want: bad.append(f'provenance {k} = {v[0]!r} != {want!r}')
        if pat and not re.match(pat, v[0]): bad.append(f'provenance {k} = {v[0]!r} !~ {pat}')
    one('schema', 'v1'); one('hven.pin.sha', pin); one('harness.sha', pat=r'^[0-9a-f]{40}$'); one('split.policy', 'split-v1')
    one('mkl.num_threads', '1'); one('run.mode', 'serial'); one('apple.accelerate', 'UNOBSERVED'); one('windows', 'UNOBSERVED')
    if any(k.startswith('lane.') for k in prov): bad.append(f"a lane was skipped with a stamped reason: {[k for k in prov if k.startswith('lane.')]}")
    return bad, rows

def summary(rows):
    c = collections.defaultdict(collections.Counter)
    for r in rows: c[r['lane']][r['outcome']] += 1
    return {l: dict(v) for l, v in c.items()}

def main(a):
    if a[0] == '--selftest': return selftest(a[1], a[2])
    hs, cu, root, pin = a[:4]
    worst = 0
    for pset, d in (('hs', hs), ('cutest', cu)):
        bad, rows = check(os.path.join(d, 'results.csv'), pset, root, pin)
        for b in bad: print('CHECK_FAIL suitev1', b)
        print(f'SUITEV1 {pset}: {len(rows)} rows; outcomes by lane {json.dumps(summary(rows), sort_keys=True)}')
        worst |= 1 if bad else 0
    print(f'CHECK_SUITEV1_RC={worst}')
    return worst

def selftest(root, _unused):
    """Falsifiers.  Positive control: a SYNTHETIC four-lane cutest results.csv assembled from two frozen records (the H-REPIN
    hven rows + the H4 external rows), under the H-REPIN provenance; each falsifier perturbs a copy."""
    import tempfile
    rp = os.path.join(root, 'runs/2026-09-m7-w2-h-repin/run-A/cutest/results.csv')
    ex = os.path.join(root, 'runs/2026-09-m7-h4-external/run-A/cutest/results.csv')
    prov, rows = load(rp); _, erows = load(ex)
    text = open(rp).read().splitlines(); head = [l for l in text if l.startswith('#')]
    cols = [l for l in text if l.startswith('cell_id,')][0]
    body = [l for l in text if not l.startswith('#') and not l.startswith('cell_id,') and ('/hven-ipm/' in l or '/hven-sqp/' in l)]
    ebody = [l for l in open(ex).read().splitlines() if not l.startswith('#') and not l.startswith('cell_id,') and ('/ipopt/' in l or '/snopt/' in l)]
    pin = prov['hven.pin.sha'][0]
    fails = 0
    def expect(name, bad, want):
        nonlocal fails
        ok = bool(bad) == want
        print(('SELFTEST_OK   ' if ok else 'SELFTEST_FAIL ') + name + (f'  [refused: {bad[0][:90]}]' if bad else '  [accepted]'))
        fails += 0 if ok else 1
    def write(lines):
        t = tempfile.mkdtemp(); p = os.path.join(t, 'results.csv'); open(p, 'w').write('\n'.join(lines) + '\n'); return p
    good = head + [cols] + body + ebody
    expect('positive control (23 TRAIN x 4 lanes)', check(write(good), 'cutest', root, pin)[0], False)
    expect('a row missing', check(write(head + [cols] + (body + ebody)[1:]), 'cutest', root, pin)[0], True)
    expect('a duplicated row', check(write(good + [good[-1]]), 'cutest', root, pin)[0], True)
    expect('the wrong pin', check(write(good), 'cutest', root, 'f' * 40)[0], True)
    expect('a HOLDOUT-labelled row', check(write([l.replace(',TRAIN,', ',HOLDOUT,', 1) if i == 30 else l for i, l in enumerate(good)]), 'cutest', root, pin)[0], True)
    ci = cols.split(',').index('hessian_provenance')
    def blank(l):
        f = l.split(','); f[ci] = ''; return ','.join(f)
    expect('empty hessian_provenance', check(write([blank(l) if i == len(head) + 5 else l for i, l in enumerate(good)]), 'cutest', root, pin)[0], True)
    expect('stamp line removed', check(write([l for l in good if not l.startswith('# hven.pin.sha')]), 'cutest', root, pin)[0], True)
    expect('the hs set against a cutest file', check(write(good), 'hs', root, pin)[0], True)
    print(f'SELFTEST_FAILS={fails}')
    return 1 if fails else 0

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
