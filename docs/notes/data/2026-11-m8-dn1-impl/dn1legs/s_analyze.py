#!/usr/bin/env python3
# M8 DN-1 S legs (../DECLARATION-dn1legs.txt): S1-S3 from the cell outputs of s_run.sh. Criteria are
# DECLARATION.txt's (3c3249977), verbatim there:
#   S1  every kNever success (optimal or acceptable) keeps its status and objective within 1e-8*max(1,|f|) under
#       kAlways; not met -> reported per cell with its kSuspect-conversion trace (S3's verdict counters), the
#       settler rules.
#   S2  every kNever non-success cell's kAlways status reported; a move to success with objective and terminal KKT
#       residuals; a move between two non-success statuses in the same detail. Not gating.
#   S3  per cell, both arms: iterations, factorizations, dense_factorize_count / dense_fallback_count (IPM:
#       IpmResult::kkt_factor_counters; the SQP does not expose them), inertia_suspect_verdicts,
#       inertia_wrong_verdicts (SQP), suspect_escalations, eqp_residual_refusals, eqp_row_drift_refusals,
#       border_uncertified_fallbacks, dep_inertia_rereads, dep_inertia_reread_flips, zero_motion_exhausted.
#       Every move declared (printed).
# usage: s_analyze.py ROSTER CELLS_DIR
import os
import sys

SUCCESS = ('optimal', 'acceptable')
S3 = ('iterations', 'factorizations', 'dn1_kkt_dense_factorize_count', 'dn1_kkt_dense_fallback_count',
      'dn1_inertia_suspect_verdicts', 'dn1_inertia_wrong_verdicts', 'suspect_escalations', 'eqp_residual_refusals',
      'eqp_row_drift_refusals', 'border_uncertified_fallbacks', 'dep_inertia_rereads', 'dep_inertia_reread_flips',
      'zero_motion_exhausted')
KKT = ('kkt_stationarity', 'kkt_primal_eq', 'kkt_primal_ineq', 'kkt_complementarity')


def read_cell(prefix):
    out = {'_result': open(prefix + '.result').read().strip()}
    out['_capped'] = 'CAPPED' in out['_result']
    if os.path.exists(prefix + '.out'):
        inside = False
        for line in open(prefix + '.out', errors='replace'):
            line = line.rstrip('\n')
            if line == 'CELL_BEGIN':
                inside = True
                continue
            if line == 'CELL_END':
                inside = False
                continue
            if '=' in line:
                k, v = line.split('=', 1)
                if k.startswith('side.'):
                    k = k[5:]
                    if k.startswith('harness_qpsum_'):
                        out.setdefault(k[len('harness_qpsum_'):], v)
                out[k] = v
    out.setdefault('status', 'CAPPED' if out['_capped'] else 'NO_ROW')
    return out


def g(cell, k):
    return cell.get(k, '-')


roster = [tuple(line.rstrip('\n').split('\t')) for line in open(sys.argv[1]) if line.strip()]
cells_dir = sys.argv[2]
rows = []
for pop, lane, prob in roster:
    tag = f'{pop}-{lane}-{prob}'
    n = read_cell(os.path.join(cells_dir, tag + '.default'))
    d = read_cell(os.path.join(cells_dir, tag + '.dense-default'))
    rows.append((tag, lane, n, d))
print(f'cells {len(rows)}')
for lab, idx in (('kNever', 2), ('kAlways', 3)):
    hist = {}
    for r in rows:
        hist[r[idx]['status']] = hist.get(r[idx]['status'], 0) + 1
    print(f'{lab} status histogram: {dict(sorted(hist.items()))}')
labels_ok = all(r[2].get('options_label') == 'default' and r[3].get('options_label') == 'dense-default'
                for r in rows if not r[2]['_capped'] and not r[3]['_capped'])
print(f'options labels as expected on every row: {labels_ok}')

print('\n== S1: every kNever success keeps its status and objective (1e-8*max(1,|f|)) under kAlways')
s1_fail = []
n_succ = 0
for tag, lane, n, d in rows:
    if n['status'] not in SUCCESS:
        continue
    n_succ += 1
    ok = d['status'] == n['status']
    rel = None
    if ok:
        fn, fd = float(n['objective']), float(d['objective'])
        rel = abs(fn - fd) / max(1.0, abs(fn))
        ok = rel <= 1e-8
    if not ok:
        s1_fail.append(tag)
        trace = ' '.join(f'{k}={g(n, k)}->{g(d, k)}' for k in S3 if g(n, k) != '-' or g(d, k) != '-')
        print(f'  NOT MET {tag}: {n["status"]} f={n.get("objective")} -> {d["status"]} f={d.get("objective")} '
              f'(rel {rel}) | trace: {trace}')
print(f'S1: {n_succ - len(s1_fail)}/{n_succ} kNever successes kept')

print('\n== S2 (reported): kNever non-success cells under kAlways')
for tag, lane, n, d in rows:
    if n['status'] in SUCCESS:
        continue
    kind = ('MOVE TO SUCCESS' if d['status'] in SUCCESS else
            'NON-SUCCESS MOVE' if (d['status'], d.get('outcome')) != (n['status'], n.get('outcome')) else 'same')
    print(f'  {tag}: {n["status"]}/{n.get("outcome")} it={g(n, "iterations")} f={n.get("objective")} -> '
          f'{d["status"]}/{d.get("outcome")} it={g(d, "iterations")} f={d.get("objective")} [{kind}]'
          + ('' if kind == 'same' else ' kkt ' + ' '.join(f'{k}={g(n, k)}->{g(d, k)}' for k in KKT)))

print('\n== S3 (reported): per-cell counters, kNever -> kAlways; every move printed')
moves = 0
for tag, lane, n, d in rows:
    parts = []
    for k in S3:
        a, b = g(n, k), g(d, k)
        if a == '-' and b == '-':
            continue
        parts.append(f'{k}={a}' if a == b else f'{k}={a}->{b}*')
        if a != b:
            moves += 1
    print(f'  {tag} [{n["status"]}->{d["status"]}] ' + ' '.join(parts))
print(f'S3: {moves} counter moves over {len(rows)} cells (marked *); SQP dense_factorize_count / '
      f'dense_fallback_count: not exposed by the engine')
flips = [(tag, g(d, 'dep_inertia_reread_flips')) for tag, lane, n, d in rows
         if lane == 'hven-sqp' and g(d, 'dep_inertia_reread_flips') not in ('-', '0')]
print(f'S3: kAlways cells with dep_inertia_reread_flips > 0 (the dense R-C\' flip\'s first observation): {flips}')

print('\n== VERDICTS')
print(f'S1_{"PASS" if not s1_fail else "NOT_MET"} {n_succ - len(s1_fail)}/{n_succ}' +
      ('' if not s1_fail else ' ' + ','.join(s1_fail)))
print('S2_REPORTED')
print('S3_REPORTED')
