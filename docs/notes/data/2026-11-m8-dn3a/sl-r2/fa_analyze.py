#!/usr/bin/env python3
# M8 DN-3a r2 (../DECLARATION-r2.txt, FA): ../sl/fa_analyze.py with the three interim tally columns replaced by the one
# per-result column dn3a_dense_solve_residual_refusals (SqpCounters::dense_solve_residual_refusals; SSN + IPQP, so FA2
# reports it per qp_mode), and a REPORTED comparison of each FA2 head kAlways row against r1's head row
# (../fa/fa2/head). Criteria otherwise r1's, verbatim. Was: M8 DN-3a (../DECLARATION.txt, FA): the forced-arm leg's analysis. Cell parsing from
# ../../2026-11-m8-dn1d-singular/r3/sl/s_analyze.py (read_cell). Criteria are the DECLARATION's, verbatim there:
#   FA1-S1    head kAlways vs head kNever on DN-0's 238 cells: every kNever success keeps status and objective within
#             1e-8*max(1,|f|) EXCEPT exactly IPM ALLINITC, IPM CRESC100, SQP HS91 (DN-1d's final S1): S1_AS_DECLARED.
#   FA1-DN1D  every head row (both labels) vs DN-1d r3's same row (r3/sl/out/s/cells): status and iterations equal,
#             objective within 1e-8*max(1,|f|): FA1_AS_DN1D. Exact-string objective differences are listed.
#   FA1-TALLY the three DN-3a tally columns read 0 on every FA1 row (no checked solve runs at qp_mode kWalk's defaults):
#             TALLY_DEFAULTS_ZERO.
#   FA2-LOSS  per SQP cell and mode (ssn, ipm): a base kAlways success is a head kAlways success with objective within
#             1e-8*max(1,|f|): NO_NEW_FAILURES.
#   FA2-NEVER the tally reads 0 on every head kNever row (sqp-ssn, sqp-ipm): TALLY_KNEVER_ZERO.
#   Columns present on every head row that carries counters (not capped, not a caught exception `threw=1`):
#             TALLY_COLUMNS_PRESENT.
# Reported, not gating: every FA2 status/objective/iteration mover (head kAlways vs base kAlways) with its refusals; the
# refusal totals per mode and site; head kAlways vs head kNever per mode.
# usage: fa_analyze.py EVIDENCE_DIR
import os
import sys

WT = '/home/ghecht/Projects/hven/.claude/worktrees/agent-acfc8f7d568381148'
DN1D = WT + '/docs/notes/data/2026-11-m8-dn1d-singular/r3/sl/out/s/cells'
SUCCESS = ('optimal', 'acceptable')
TALLY = ('dn3a_dense_solve_residual_refusals',)
ZERO = ('0',)
R1_FA2 = WT + '/docs/notes/data/2026-11-m8-dn3a/fa/fa2/head'
DECLARED_S1 = {'sd-hven-ipm-ALLINITC', 'sd-hven-ipm-CRESC100', 'sd-hven-sqp-HS91'}


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
        out.setdefault('status', 'THREW')  # the lane caught a library exception: no counters on the row
    out.setdefault('status', 'CAPPED' if out['_capped'] else 'NO_ROW')
    out['_counted'] = out['status'] not in ('CAPPED', 'NO_ROW', 'THREW')
    return out


def close(a, b):
    try:
        fa, fb = float(a['objective']), float(b['objective'])
    except (KeyError, ValueError):
        return False
    return abs(fa - fb) <= 1e-8 * max(1.0, abs(fa))


def tally(c):
    return tuple(c.get(k, '-') for k in TALLY)


ev = sys.argv[1]
roster = [tuple(l.rstrip('\n').split('\t')) for l in open(os.path.join(ev, 'roster.tsv')) if l.strip()]
verdicts = []

print('== FA1: DN-0 238 cells, default (kNever) and dense-default (kAlways), head lane binary')
rows = []
for pop, lane, prob in roster:
    tag = f'{pop}-{lane}-{prob}'
    n = read_cell(os.path.join(ev, 'fa1/cells', tag + '.default'))
    d = read_cell(os.path.join(ev, 'fa1/cells', tag + '.dense-default'))
    rows.append((tag, lane, n, d))
print(f'cells {len(rows)}')
miss = []
nsucc = 0
for tag, lane, n, d in rows:
    if n['status'] not in SUCCESS:
        continue
    nsucc += 1
    if not (d['status'] == n['status'] and close(n, d)):
        miss.append(tag)
        print(f'  S1 miss {tag}: {n["status"]} f={n.get("objective")} -> {d["status"]} f={d.get("objective")}')
print(f'S1: {nsucc - len(miss)}/{nsucc}')
verdicts.append(('S1_AS_DECLARED' if set(miss) == DECLARED_S1 else 'S1_NOT_AS_DECLARED')
                + f' {nsucc - len(miss)}/{nsucc} misses {",".join(sorted(miss))}')

print('\n== FA1 vs DN-1d r3 (same cell, same label)')
movers = []
strdiff = []
for tag, lane, n, d in rows:
    for lab, h in (('default', n), ('dense-default', d)):
        r = read_cell(os.path.join(DN1D, f'{tag}.{lab}'))
        same = (h['status'] == r['status'] and h.get('iterations') == r.get('iterations')
                and (h['status'] not in SUCCESS or close(r, h)))
        if not same:
            movers.append(f'{tag}.{lab}')
            print(f'  MOVER {tag}.{lab}: dn1d {r["status"]} it={r.get("iterations")} f={r.get("objective")} -> '
                  f'head {h["status"]} it={h.get("iterations")} f={h.get("objective")} tally={tally(h)}')
        elif h.get('objective') != r.get('objective'):
            strdiff.append(f'{tag}.{lab}')
print(f'movers {len(movers)}; exact-string objective differences without a mover: {len(strdiff)} {" ".join(strdiff)}')
verdicts.append('FA1_AS_DN1D' if not movers else f'FA1_DN1D_MOVERS({len(movers)}: {" ".join(movers)})')
kn_movers = [m for m in movers if m.endswith('.default')]
if kn_movers:
    verdicts.append(f'KNEVER_MOVED_STOP({" ".join(kn_movers)})')

nonzero = [f'{tag}.{lab}={tally(c)}' for tag, lane, n, d in rows for lab, c in (('default', n), ('dense-default', d))
           if c['_counted'] and lane == 'hven-sqp' and tally(c) != ZERO]
print(f'\nFA1 tally nonzero or absent (SQP rows): {len(nonzero)} {" ".join(nonzero)}')
verdicts.append('TALLY_DEFAULTS_ZERO' if not nonzero else 'TALLY_DEFAULTS_NONZERO')

print('\n== FA2: 119 SQP cells, qp_mode kSsn and kIpqp; head kAlways vs BASE kAlways (b70e45cfb), head kNever')
lost = []
never_nonzero = []
absent = []
for base in ('sqp-ssn', 'sqp-ipm'):
    print(f'-- {base}')
    tot = [0]
    r1mv = []
    cells_with = []
    mv = []
    hk_kept = hk_succ = 0
    for pop, lane, prob in roster:
        if lane != 'hven-sqp':
            continue
        tag = f'{pop}-{lane}-{prob}'
        hn = read_cell(os.path.join(ev, 'fa2/head', f'{tag}.{base}'))
        hd = read_cell(os.path.join(ev, 'fa2/head', f'{tag}.dense-{base}'))
        bd = read_cell(os.path.join(ev, 'fa2/base', f'{tag}.dense-{base}'))
        for c, lab in ((hn, base), (hd, 'dense-' + base)):
            if c['_counted'] and '-' in tally(c):
                absent.append(f'{tag}.{lab}')
        if hn['_counted'] and tally(hn) != ZERO:
            never_nonzero.append(f'{tag}.{base}')
        t = tally(hd)
        if '-' not in t:
            for i in range(len(TALLY)):
                tot[i] += int(t[i])
            if t != ZERO:
                cells_with.append(f'{prob}:{"/".join(t)}')
        if bd['status'] in SUCCESS and not (hd['status'] == bd['status'] and close(bd, hd)):
            lost.append(f'{tag}.dense-{base}')
            print(f'  LOST {tag}: base {bd["status"]} f={bd.get("objective")} -> head {hd["status"]} '
                  f'f={hd.get("objective")} tally={t}')
        if (bd['status'], bd.get('objective'), bd.get('iterations')) != \
           (hd['status'], hd.get('objective'), hd.get('iterations')):
            mv.append(f'{prob}: {bd["status"]} it={bd.get("iterations")} f={bd.get("objective")} -> '
                      f'{hd["status"]} it={hd.get("iterations")} f={hd.get("objective")} tally={t}')
        r1 = read_cell(os.path.join(R1_FA2, f'{tag}.dense-{base}'))
        if (r1['status'], r1.get('objective'), r1.get('iterations')) != \
           (hd['status'], hd.get('objective'), hd.get('iterations')):
            r1mv.append(f'{prob}: r1 {r1["status"]} it={r1.get("iterations")} f={r1.get("objective")} -> '
                        f'r2 {hd["status"]} it={hd.get("iterations")} f={hd.get("objective")} refusals={t}')
        if hn['status'] in SUCCESS:
            hk_succ += 1
            hk_kept += 1 if (hd['status'] == hn['status'] and close(hn, hd)) else 0
    print(f'  head kAlways vs base kAlways movers (status/objective/iterations): {len(mv)}')
    for m in mv:
        print('    ' + m)
    print(f'  refusals (dense_solve_residual_refusals): {tot[0]} in {len(cells_with)} cells: {" ".join(cells_with)}')
    print(f'  head kAlways r2 vs r1 head kAlways (reported) differences: {len(r1mv)}')
    for m in r1mv:
        print('    ' + m)
    print(f'  head kAlways keeps {hk_kept}/{hk_succ} head kNever successes (reported)')
verdicts.append('NO_NEW_FAILURES' if not lost else f'NEW_FAILURES({" ".join(lost)})')
verdicts.append('TALLY_KNEVER_ZERO' if not never_nonzero else f'TALLY_KNEVER_NONZERO({" ".join(never_nonzero)})')
verdicts.append('TALLY_COLUMNS_PRESENT' if not absent else f'TALLY_COLUMNS_ABSENT({len(absent)})')

print('\n== VERDICTS')
ok = True
for v in verdicts:
    print(v)
    ok = ok and v.split(' ')[0] in ('S1_AS_DECLARED', 'FA1_AS_DN1D', 'TALLY_DEFAULTS_ZERO', 'NO_NEW_FAILURES',
                                     'TALLY_KNEVER_ZERO', 'TALLY_COLUMNS_PRESENT')
print('FA_PASS' if ok else 'FA_NOT_MET')
