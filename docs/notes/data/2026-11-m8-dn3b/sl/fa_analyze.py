#!/usr/bin/env python3
# M8 DN-3b (../DECLARATION.txt, FA): the forced-arm leg's analysis. Cell parsing (read_cell) and the objective rule
# (close: within 1e-8*max(1,|f|)) are ../../2026-11-m8-dn3a/sl-r2/fa_analyze.py's. The reference rows are DN-3a r2's FA1
# rows (../../2026-11-m8-dn3a/fa-r2/fa1/cells; hven 83c8d3774, whose IPM is ccd2e951f's on Linux: the two later commits
# touch the Accelerate thread pin and a test only). Criteria, verbatim in the DECLARATION:
#   KNEVER_UNMOVED          every head `default` row (238) vs DN-3a r2's: status and iterations equal, objective close.
#                           A mover is a STOP (KNEVER_MOVED_STOP).
#   SQP_DENSE_UNMOVED       every head SQP `dense-default` row (119) vs DN-3a r2's: the same test.
#   BASE_AS_DN3A            every FA2 base IPM `dense-default` row (119) vs DN-3a r2's: the same test (the base lane
#                           binary reproduces kSparseFallback's rows).
#   NO_SPARSE_FALLBACK      every counted head IPM `dense-default` row: dn1_kkt_dense_fallback_count,
#                           dn3b_kkt_dense_retake_count and dn3b_kkt_dense_shift_refactor_count read 0 (columns present).
#   NO_HEAD_FAILURE_ROW     no head IPM `dense-default` row is CAPPED, THREW or NO_ROW unless its base row is too.
#   MOVERS_AS_DECLARED      head IPM kAlways vs base IPM kAlways: the cells whose status differs or whose objective is
#                           not close (either side a success) are exactly DECLARED_MOVERS.
#   S1_AS_DECLARED          head IPM kAlways vs head IPM kNever: the kNever successes not kept (status equal and
#                           objective close) are exactly DECLARED_S1.
#   ACOPP30_KEPT            head IPM ACOPP30 kAlways: status equal to head kNever's and objective close.
# Reported, not gating: per-cell dense counters (singular reports, flags, guard triggers, factorizations, restoration
# entries) on the head rows; iteration-only movers vs base; totals.
# usage: fa_analyze.py EVIDENCE_DIR
import os
import sys

WT = '/home/ghecht/Projects/hven/.claude/worktrees/agent-a24a4442b0864282f'
DN3A = WT + '/docs/notes/data/2026-11-m8-dn3a/fa-r2/fa1/cells'
SUCCESS = ('optimal', 'acceptable')
# Declared in advance (DECLARATION.txt, "FA MOVERS"); filled from the dev probe disclosed there.
DECLARED_MOVERS = {'sd-hven-ipm-BT8', 'sd-hven-ipm-CRESC100', 'sd-hven-ipm-LEWISPOL', 'sd-hven-ipm-MESH'}
DECLARED_S1 = {'sd-hven-ipm-ALLINITC', 'sd-hven-ipm-BT8', 'sd-hven-ipm-CRESC100'}
COUNTERS = ('dn3b_kkt_dense_singular_count', 'dn3b_kkt_dense_flagged_count', 'dn1b_dense_residual_ratio_triggers',
            'dn1_kkt_dense_fallback_count', 'dn3b_kkt_dense_retake_count', 'dn3b_kkt_dense_shift_refactor_count',
            'factorizations', 'dn3b_resto_entries')
ZERO_COLS = ('dn1_kkt_dense_fallback_count', 'dn3b_kkt_dense_retake_count', 'dn3b_kkt_dense_shift_refactor_count')


def read_cell(prefix):
    out = {'_result': open(prefix + '.result').read().strip()}
    out['_capped'] = 'CAPPED' in out['_result']
    if os.path.exists(prefix + '.out'):
        for line in open(prefix + '.out', errors='replace'):
            line = line.rstrip('\n')
            if '=' in line and not line.startswith('CELL_'):
                k, v = line.split('=', 1)
                if k.startswith('side.'):
                    k = k[5:]
                out[k] = v
    if out.get('threw') == '1':
        out.setdefault('status', 'THREW')
    out.setdefault('status', 'CAPPED' if out['_capped'] else 'NO_ROW')
    out['_counted'] = out['status'] not in ('CAPPED', 'NO_ROW', 'THREW')
    return out


def close(a, b):
    try:
        fa, fb = float(a['objective']), float(b['objective'])
    except (KeyError, ValueError):
        return False
    return abs(fa - fb) <= 1e-8 * max(1.0, abs(fa))


def same_row(r, h):
    return (h['status'] == r['status'] and h.get('iterations') == r.get('iterations')
            and (h['status'] not in SUCCESS or close(r, h)))


def brief(c):
    return f'{c["status"]} it={c.get("iterations")} f={c.get("objective")}'


ev = sys.argv[1]
roster = [tuple(l.rstrip('\n').split('\t')) for l in open(os.path.join(ev, 'roster.tsv')) if l.strip()]
verdicts = []
head = {}
base = {}
ref = {}
for pop, lane, prob in roster:
    tag = f'{pop}-{lane}-{prob}'
    for lab in ('default', 'dense-default'):
        head[tag, lab] = read_cell(os.path.join(ev, 'fa1/cells', f'{tag}.{lab}'))
        ref[tag, lab] = read_cell(os.path.join(DN3A, f'{tag}.{lab}'))
    if lane == 'hven-ipm':
        base[tag] = read_cell(os.path.join(ev, 'fa2/base', f'{tag}.dense-default'))
ipm = [f'{p}-{l}-{q}' for p, l, q in roster if l == 'hven-ipm']
sqp = [f'{p}-{l}-{q}' for p, l, q in roster if l == 'hven-sqp']
print(f'cells {len(roster)} (IPM {len(ipm)}, SQP {len(sqp)})')

print('\n== KNEVER: head default vs DN-3a r2 default (238)')
mv = [t for p, l, q in roster for t in [f'{p}-{l}-{q}'] if not same_row(ref[t, 'default'], head[t, 'default'])]
for t in mv:
    print(f'  MOVER {t}: dn3a {brief(ref[t, "default"])} -> head {brief(head[t, "default"])}')
verdicts.append('KNEVER_UNMOVED' if not mv else f'KNEVER_MOVED_STOP({" ".join(mv)})')

print('\n== SQP kAlways: head dense-default vs DN-3a r2 (119)')
mv = [t for t in sqp if not same_row(ref[t, 'dense-default'], head[t, 'dense-default'])]
for t in mv:
    print(f'  MOVER {t}: dn3a {brief(ref[t, "dense-default"])} -> head {brief(head[t, "dense-default"])}')
verdicts.append('SQP_DENSE_UNMOVED' if not mv else f'SQP_DENSE_MOVED({" ".join(mv)})')

print('\n== BASE: FA2 base IPM dense-default vs DN-3a r2 (119)')
mv = [t for t in ipm if not same_row(ref[t, 'dense-default'], base[t])]
for t in mv:
    print(f'  MOVER {t}: dn3a {brief(ref[t, "dense-default"])} -> base {brief(base[t])}')
verdicts.append('BASE_AS_DN3A' if not mv else f'BASE_NOT_AS_DN3A({" ".join(mv)})')

print('\n== IPM kAlways (kReport) counters, head rows')
bad = []
absent = []
tot = {k: 0 for k in COUNTERS}
for t in ipm:
    h = head[t, 'dense-default']
    if not h['_counted']:
        continue
    vals = {k: h.get(k) for k in COUNTERS}
    if any(v is None for v in vals.values()):
        absent.append(t)
        continue
    for k in COUNTERS:
        if k != 'dn3b_resto_entries' or int(vals[k]) > 0:
            tot[k] += int(vals[k])
    if any(vals[k] != '0' for k in ZERO_COLS):
        bad.append(f'{t}:' + '/'.join(vals[k] for k in ZERO_COLS))
    if any(vals[k] != '0' for k in ('dn3b_kkt_dense_singular_count', 'dn3b_kkt_dense_flagged_count',
                                    'dn1b_dense_residual_ratio_triggers')) or int(vals['dn3b_resto_entries']) > 0:
        b = base[t]
        print(f'  {t}: singular={vals["dn3b_kkt_dense_singular_count"]} flagged={vals["dn3b_kkt_dense_flagged_count"]} '
              f'triggers={vals["dn1b_dense_residual_ratio_triggers"]} fact={vals["factorizations"]} '
              f'resto={vals["dn3b_resto_entries"]} | head {brief(h)} | base {brief(b)} '
              f'base_fallback={b.get("dn1_kkt_dense_fallback_count")} base_triggers={b.get("dn1b_dense_residual_ratio_triggers")} '
              f'base_fact={b.get("factorizations")}')
print('  totals: ' + ' '.join(f'{k}={v}' for k, v in tot.items()))
btot = {k: sum(int(base[t].get(k, 0) or 0) for t in ipm if base[t]['_counted'])
        for k in ('dn1_kkt_dense_fallback_count', 'dn1b_dense_residual_ratio_triggers', 'factorizations')}
print('  base totals: ' + ' '.join(f'{k}={v}' for k, v in btot.items()))
if absent:
    verdicts.append(f'COLUMNS_ABSENT({len(absent)}: {" ".join(absent)})')
verdicts.append('NO_SPARSE_FALLBACK' if not bad and not absent else f'SPARSE_FALLBACK_TAKEN({" ".join(bad)})')

fail_rows = [t for t in ipm if not head[t, 'dense-default']['_counted'] and base[t]['_counted']]
for t in fail_rows:
    print(f'  HEAD FAILURE ROW {t}: {head[t, "dense-default"]["_result"]}')
verdicts.append('NO_HEAD_FAILURE_ROW' if not fail_rows else f'HEAD_FAILURE_ROWS({" ".join(fail_rows)})')

print('\n== IPM movers: head kAlways (kReport) vs base kAlways (kSparseFallback)')
movers = set()
itonly = []
for t in ipm:
    h, b = head[t, 'dense-default'], base[t]
    moved = h['status'] != b['status'] or ((h['status'] in SUCCESS or b['status'] in SUCCESS) and not close(b, h))
    if moved:
        movers.add(t)
        print(f'  MOVER {t}: base {brief(b)} -> head {brief(h)}')
    elif h.get('iterations') != b.get('iterations') or h.get('objective') != b.get('objective'):
        itonly.append(f'{t.split("-")[-1]}: it {b.get("iterations")}->{h.get("iterations")}')
print(f'  status/objective movers {len(movers)}; iteration/last-digit only {len(itonly)}: {"; ".join(itonly)}')
undeclared = sorted(movers - DECLARED_MOVERS)
missing = sorted(DECLARED_MOVERS - movers)
verdicts.append('MOVERS_AS_DECLARED' if not undeclared and not missing else
                f'MOVERS_NOT_AS_DECLARED(undeclared {" ".join(undeclared)}; declared but unmoved {" ".join(missing)})')

print('\n== S1 (IPM): head kAlways vs head kNever')
miss = set()
nsucc = 0
for t in ipm:
    n, d = head[t, 'default'], head[t, 'dense-default']
    if n['status'] not in SUCCESS:
        continue
    nsucc += 1
    if not (d['status'] == n['status'] and close(n, d)):
        miss.add(t)
        print(f'  S1 miss {t}: {brief(n)} -> {brief(d)}')
print(f'S1 (IPM): {nsucc - len(miss)}/{nsucc}')
verdicts.append(('S1_AS_DECLARED' if miss == DECLARED_S1 else 'S1_NOT_AS_DECLARED')
                + f' {nsucc - len(miss)}/{nsucc} misses {",".join(sorted(miss))}')

t = 'sd-hven-ipm-ACOPP30'
n, d = head[t, 'default'], head[t, 'dense-default']
print(f'\n== ACOPP30: kNever {brief(n)}; kAlways head {brief(d)}; kAlways base {brief(base[t])}')
verdicts.append('ACOPP30_KEPT' if d['status'] == n['status'] and close(n, d) else 'ACOPP30_LOST')

print('\n== VERDICTS')
ok = True
for v in verdicts:
    print(v)
    ok = ok and v.split(' ')[0] in ('KNEVER_UNMOVED', 'SQP_DENSE_UNMOVED', 'BASE_AS_DN3A', 'NO_SPARSE_FALLBACK',
                                     'NO_HEAD_FAILURE_ROW', 'MOVERS_AS_DECLARED', 'S1_AS_DECLARED', 'ACOPP30_KEPT')
print('FA_PASS' if ok else 'FA_NOT_MET')
