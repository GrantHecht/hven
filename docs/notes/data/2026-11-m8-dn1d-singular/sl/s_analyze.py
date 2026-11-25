#!/usr/bin/env python3
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/dn1legs/ and re-pointed (worktree dn1b, evidence this directory, the DN-1b lane binary build-dn1be/dn1bbin/hven_bench_cell); older provenance lines below are historical.
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
# DN-1b (../DECLARATION.txt S1): the S1 misses must lie within the movers declared in advance, and IPM ACOPP30 must
# keep its status and objective; S1_AS_DECLARED / S1_NOT_MET. Trigger counts (IPM
# dn1b_dense_residual_ratio_triggers) per cell, both arms; kNever must read 0 on every IPM row (TRIGGERS_KNEVER_ZERO).
# DN-1c (../DECLARATION.txt): S3 and the trigger section add the SQP column dn1c_dense_condition_fallbacks;
# TRIGGERS_KNEVER_ZERO now needs both columns (IPM dn1b, SQP dn1c) present and 0 on every kNever row.
# DN-1d (../DECLARATION.txt S): the verdict block rewritten for DN-1d's criteria (below); S3 adds the three DN-1d
# columns.
# usage: s_analyze.py ROSTER CELLS_DIR
import os
import sys

SUCCESS = ('optimal', 'acceptable')
S3 = ('iterations', 'factorizations', 'dn1_kkt_dense_factorize_count', 'dn1_kkt_dense_fallback_count',
      'dn1b_dense_residual_ratio_triggers', 'dn1c_dense_condition_fallbacks', 'dn1d_dense_flagged_factorizations',
      'dn1d_dense_singular_refactors', 'dn1d_dense_shifted_solve_refusals',
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

print('\n== DN-1b trigger counts (IPM dn1b_dense_residual_ratio_triggers; kNever -> kAlways), nonzero rows')
TRIG = 'dn1b_dense_residual_ratio_triggers'
knever_nonzero = []
for tag, lane, n, d in rows:
    if lane != 'hven-ipm':
        continue
    a, b = g(n, TRIG), g(d, TRIG)
    if a not in ('0', '-'):
        knever_nonzero.append(tag)
    if a not in ('0', '-') or b not in ('0', '-'):
        print(f'  {tag}: {a} -> {b} [{n["status"]}->{d["status"]}]')
ipm_rows = [r for r in rows if r[1] == 'hven-ipm']
missing = [r[0] for r in ipm_rows if g(r[2], TRIG) == '-' and not r[2]['_capped']]
print(f'IPM rows {len(ipm_rows)}; kNever rows with a nonzero trigger count: {knever_nonzero}; kNever rows '
      f'without the column (uncapped): {missing}')
print(f'kAlways total triggers: {sum(int(g(r[3], TRIG)) for r in ipm_rows if g(r[3], TRIG) != "-")} in '
      f'{sum(1 for r in ipm_rows if g(r[3], TRIG) not in ("-", "0"))} cells')

# DN-1c: the SQP's condition-check fallbacks (SqpCounters::dense_condition_fallbacks), both arms.
print('\n== DN-1c trigger counts (SQP dn1c_dense_condition_fallbacks; kNever -> kAlways), nonzero rows')
TRIG_C = 'dn1c_dense_condition_fallbacks'
for tag, lane, n, d in rows:
    if lane != 'hven-sqp':
        continue
    a, b = g(n, TRIG_C), g(d, TRIG_C)
    if a not in ('0', '-'):
        knever_nonzero.append(tag)
    if a not in ('0', '-') or b not in ('0', '-'):
        print(f'  {tag}: {a} -> {b} [{n["status"]}->{d["status"]}]')
sqp_rows = [r for r in rows if r[1] == 'hven-sqp']
missing += [r[0] for r in sqp_rows if g(r[2], TRIG_C) == '-' and not r[2]['_capped']]
print(f'SQP rows {len(sqp_rows)}; kNever rows with a nonzero count (IPM and SQP): {knever_nonzero}; kNever rows '
      f'without the column (uncapped, IPM and SQP): {missing}')
print(f'kAlways total SQP condition fallbacks: {sum(int(g(r[3], TRIG_C)) for r in sqp_rows if g(r[3], TRIG_C) != "-")} in '
      f'{sum(1 for r in sqp_rows if g(r[3], TRIG_C) not in ("-", "0"))} cells')

# DN-1d (../DECLARATION.txt S): the DN-1b set declared in advance; IPM ACOPP30 and SQP HS90 must keep; SQP ALLINITC
# OPTIMAL at kAlways; last-resort retakes 0 on every SQP cell; suspect_escalations and eqp_residual_refusals on SQP
# success cells (success on both arms) no more than kNever's; C282 (a) per SQP cell flagged == refactors (no retry),
# (c) retries and retakes 0, (b) dense_shifted_solve_refusals reported per cell.
DECLARED = {'sd-hven-ipm-CRESC100', 'sd-hven-ipm-ALLINITC', 'sd-hven-sqp-HS91'}
MUST_KEEP = ('sd-hven-ipm-ACOPP30', 'sd-hven-sqp-HS90')
keep_ok = {}
for mk in MUST_KEEP:
    r = [x for x in rows if x[0] == mk]
    keep_ok[mk] = len(r) == 1 and r[0][2]['status'] in SUCCESS and mk not in s1_fail
undeclared = sorted(set(s1_fail) - DECLARED)
print(f'\nS1 misses {sorted(s1_fail)}; declared movers {sorted(DECLARED)} (moved: {sorted(set(s1_fail) & DECLARED)}); '
      f'undeclared misses {undeclared}; must keep {keep_ok}')

alli = [r for r in rows if r[0] == 'sd-hven-sqp-ALLINITC']
alli_ok = len(alli) == 1 and alli[0][3]['status'] == 'optimal'
print(f'SQP ALLINITC kAlways status: {alli[0][3]["status"] if alli else "MISSING"} (must be optimal: {alli_ok})')

print('\n== DN-1d per SQP cell (kAlways): flagged / refactors / retries / retakes / shifted-solve refusals; nonzero rows')
F, R, X, Q = ('dn1d_dense_flagged_factorizations', 'dn1d_dense_singular_refactors', 'dn1c_dense_condition_fallbacks',
              'dn1d_dense_shifted_solve_refusals')
a_bad, c_bad, missing_d, never_nonzero = [], [], [], []
tot = {F: 0, R: 0, X: 0, Q: 0}
for tag, lane, n, d in sqp_rows:
    for lab, c in (('kNever', n), ('kAlways', d)):
        if c['_capped']:
            continue
        vals = {k: g(c, k) for k in (F, R, X, Q)}
        if '-' in vals.values():
            missing_d.append(f'{tag}.{lab}')
            continue
        vals = {k: int(v) for k, v in vals.items()}
        if lab == 'kNever':
            if any(vals.values()):
                never_nonzero.append(tag)
            continue
        for k in tot:
            tot[k] += vals[k]
        retries = vals[R] - vals[F]
        if vals[F] != vals[R]:
            a_bad.append(tag)
        if retries != 0 or vals[X] != 0:
            c_bad.append(tag)
        if any(vals.values()):
            print(f'  {tag} [{n["status"]}->{d["status"]}]: flagged {vals[F]} refactors {vals[R]} retries {retries} '
                  f'retakes {vals[X]} shifted_solve_refusals {vals[Q]}')
print(f'kAlways totals: flagged {tot[F]}, refactors {tot[R]}, retakes {tot[X]}, shifted-solve refusals {tot[Q]}')
print(f'(a) cells with flagged != refactors: {a_bad}; (c) cells with a retry or a retake: {c_bad}; rows missing a DN-1d '
      f'column (uncapped): {missing_d}; kNever rows with a nonzero DN-1d column: {never_nonzero}')

esc_bad = []
for tag, lane, n, d in sqp_rows:
    if n['status'] not in SUCCESS or d['status'] not in SUCCESS:
        continue
    for k in ('suspect_escalations', 'eqp_residual_refusals'):
        a, b = g(n, k), g(d, k)
        if a == '-' or b == '-' or int(b) > int(a):
            esc_bad.append(f'{tag}:{k}={a}->{b}')
print(f'suspect_escalations / eqp_residual_refusals above kNever on SQP success cells: {esc_bad}')

print('\n== VERDICTS')
s1_ok = not undeclared and all(keep_ok.values())
print(f'S1_{"AS_DECLARED" if s1_ok else "NOT_MET"} {n_succ - len(s1_fail)}/{n_succ}' +
      ('' if not s1_fail else ' misses ' + ','.join(s1_fail)) + ('' if not undeclared else ' UNDECLARED ' + ','.join(undeclared)))
print('ALLINITC_OPTIMAL' if alli_ok else 'ALLINITC_NOT_OPTIMAL')
retake_ok = not c_bad and not missing_d and tot[X] == 0
print('RETAKES_ZERO' if retake_ok else 'RETAKES_NOT_ZERO')
print('ESCALATIONS_NOT_ABOVE_KNEVER' if not esc_bad else 'ESCALATIONS_ABOVE_KNEVER')
print('EVERY_FLAG_ANSWERED' if not a_bad and not missing_d else 'FLAG_NOT_ANSWERED')
print('TRIGGERS_KNEVER_ZERO' if not knever_nonzero and not missing and not never_nonzero else 'TRIGGERS_KNEVER_NONZERO')
print('S2_REPORTED')
print('S3_REPORTED')
ok = s1_ok and alli_ok and retake_ok and not esc_bad and not a_bad and not missing_d and not knever_nonzero \
    and not missing and not never_nonzero
print('S_PASS' if ok else 'S_FAIL')
