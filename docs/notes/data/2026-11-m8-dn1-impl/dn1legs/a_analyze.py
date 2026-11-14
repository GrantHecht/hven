#!/usr/bin/env python3
# M8 DN-1 A legs (../DECLARATION-dn1legs.txt): A1-A7 and A-thr from the replay tool's two CSVs joined with
# ../../2026-11-m8-dn1-design/matrices.csv.gz (references, DN-0's columns, MKL's dsytrf reads). Stdlib only.
# The criteria are DECLARATION.txt's (3c3249977), verbatim there; each gated one prints a verdict line.
# usage: a_analyze.py MATRICES_CSV_GZ FULL_CSV DENSE_DEFAULT_THREADS_CSV
import csv
import gzip
import statistics
import sys

S_LIST = (1, 10, 100)
CUTS = (8, 16, 24, 32, 48, 64, 96, 128, None)


def engine_of(cell):
    if '-hven-ipm-' in cell:
        return 'IPM'
    if '-hven-sqp-' in cell:
        return 'SQP'
    raise SystemExit(f'unknown engine in cell {cell}')


def mx(v):
    return max(v) if v else float('nan')


def md(v):
    return statistics.median(v) if v else float('nan')


def i(v):
    return int(v) if v not in ('', None) else None


ref = {(r['cell'], r['matrix']): r for r in csv.DictReader(gzip.open(sys.argv[1], 'rt'))}
full = {(r['cell'], r['matrix']): r for r in csv.DictReader(open(sys.argv[2]))}
dthr = {(r['cell'], r['matrix']): r for r in csv.DictReader(open(sys.argv[3]))}
verdicts = []
print(f'rows: matrices.csv {len(ref)}, full {len(full)}, default-threads {len(dthr)}')
if set(ref) != set(full) or set(ref) != set(dthr) or len(ref) != 5501:
    print('A_INPUT_MISMATCH')
    sys.exit(3)
threw = [k for k, r in full.items() if r['never_threw'] != '0' or r['always_threw'] != '0']
print(f'threw (either arm): {len(threw)} {threw[:10]}')


def reference(key, s):
    m = ref[key]
    if m[f'dec_u{s}'] == '1':
        return int(m['true_neg'])
    if m[f'dec_s{s}'] == '1':
        return int(m['ref_neg_scaled'])
    return None


def dense_taken(r):
    return r['always_status'] == '0' and r['always_arm'] == '1'


keys = sorted(full)
by_engine = {'IPM': [k for k in keys if engine_of(k[0]) == 'IPM'],
             'SQP': [k for k in keys if engine_of(k[0]) == 'SQP']}
print(f'matrices per engine: IPM {len(by_engine["IPM"])}, SQP {len(by_engine["SQP"])}')
taken = [k for k in keys if dense_taken(full[k])]
fallback = [k for k in keys if full[k]['always_arm'] == '0']
print(f'dense-taken {len(taken)}, fallback (arm kSparse) {len(fallback)}, other {len(keys) - len(taken) - len(fallback)}')

# ---- A1
print('\n== A1: dense-arm negative count = reference on the decidable union, dense-taken matrices')
for eng in ('IPM', 'SQP'):
    for s in S_LIST:
        union = [k for k in by_engine[eng] if reference(k, s) is not None]
        dt = [k for k in union if dense_taken(full[k])]
        ok = [k for k in dt if int(full[k]['always_neg']) == reference(k, s)]
        bad = [k for k in dt if k not in set(ok)]
        line = f'A1-{eng} s={s}: {len(ok)}/{len(dt)} (union {len(union)}, of them dense-taken {len(dt)})'
        print(line)
        for k in bad:
            m = ref[k]
            print(f'  WRONG {k}: arm {full[k]["always_neg"]} true_neg {m["true_neg"]} (dec_u{s} {m[f"dec_u{s}"]}) '
                  f'ref_neg_scaled {m["ref_neg_scaled"]} (dec_s{s} {m[f"dec_s{s}"]})')
        if s == 1:
            v = f'A1_{eng}_{"PASS" if dt and len(ok) == len(dt) else "FAIL"} {len(ok)}/{len(dt)}'
            verdicts.append(v)

# ---- A2
print('\n== A2: Pardiso (unperturbed) matches the reference and the arm does not (union, s = 1)')
for eng in ('IPM', 'SQP'):
    hits = []
    for k in by_engine[eng]:
        r = full[k]
        rv = reference(k, 1)
        if rv is None or r['never_status'] != '0' or r['never_state'] != '0':
            continue
        if i(r['never_perturbed']) != 0:
            continue
        arm_neg = i(r['always_neg']) if r['always_status'] == '0' else None
        if int(r['never_neg']) == rv and arm_neg != rv:
            hits.append(k)
    print(f'A2-{eng}: {len(hits)} {hits[:20]}')
    verdicts.append(f'A2_{eng}_{"PASS" if not hits else "FAIL"} {len(hits)}')

# ---- A3
print('\n== A3: every matrix the arm finds exactly singular is the fallback\'s, field for field = kNever')
FIELDS = ('threw', 'status', 'backend_code', 'arm', 'state', 'pos', 'neg', 'zero', 'zero_is_derived',
          'perturbed', 'factor_nonzeros', 'factor_mflops', 'factor_size_bytes', 'dense_factor_entries',
          'factorize_count', 'residual')
equal = 0
for k in fallback:
    r = full[k]
    diffs = [f for f in FIELDS if r['never_' + f] != r['always_' + f]]
    counts_ok = r['always_dense_fallback_count'] == '1' and r['always_dense_factorize_count'] == '0'
    if not diffs and counts_ok:
        equal += 1
    print(f'  {k[0]} {k[1]} n={r["n"]} status {r["always_status"]} neg {r["always_neg"]} perturbed '
          f'{r["always_perturbed"]} x_bitwise {r["always_x_bitwise_equal_never"]} '
          f'{"EQUAL" if not diffs and counts_ok else "DIFFERS " + ",".join(diffs) + ("" if counts_ok else " counters")}')
other = [k for k in keys if k not in set(taken) and k not in set(fallback)]
for k in other:
    print(f'  NEITHER {k}: always_status {full[k]["always_status"]} arm {full[k]["always_arm"]}')
print(f'A3: {equal}/{len(fallback)} fallback matrices field-for-field equal (expected 25/25)')
verdicts.append(f'A3_{"PASS" if fallback and equal == len(fallback) and not other else "FAIL"} {equal}/{len(fallback)}')

# ---- A4
print('\n== A4 (reported): literal agreement with unperturbed Pardiso (kNever replay), dense-taken, n <= cut')
for eng in ('IPM', 'SQP'):
    cells = []
    for cut in CUTS:
        sel = [k for k in by_engine[eng] if dense_taken(full[k]) and full[k]['never_status'] == '0'
               and full[k]['never_state'] == '0' and i(full[k]['never_perturbed']) == 0
               and (cut is None or int(full[k]['n']) <= cut)]
        agree = sum(1 for k in sel if full[k]['never_neg'] == full[k]['always_neg'])
        cells.append(f'n<={cut if cut else "all"} {agree}/{len(sel)}' + (f'={agree / len(sel):.4f}' if sel else ''))
    print(f'A4-{eng}: ' + '; '.join(cells))

# ---- A5
print('\n== A5: arm = MKL dsytrf by det/trace (mkl_bk_neg) on every dense-taken matrix; vs DN-0 dense column')
bk_bad = [k for k in taken if ref[k]['mkl_bk_neg'] == '' or int(ref[k]['mkl_bk_neg']) != int(full[k]['always_neg'])]
tool_bad = [k for k in taken if full[k]['bk_neg'] != full[k]['always_neg']]
dn0_diff = [k for k in taken if ref[k]['dense_neg'] == '' or int(ref[k]['dense_neg']) != int(full[k]['always_neg'])]
print(f'arm vs mkl_bk_neg: {len(taken) - len(bk_bad)}/{len(taken)} equal; differing {bk_bad[:20]}')
print(f'arm vs this tool\'s own dsytrf walk (bk_neg, instrument self-check): {len(taken) - len(tool_bad)}/{len(taken)}')
for k in dn0_diff:
    print(f'  vs DN-0 dense_neg: {k} arm {full[k]["always_neg"]} dense_neg {ref[k]["dense_neg"]!r} '
          f'(dense_status {ref[k]["dense_status"]})')
lakes_ok = (len(dn0_diff) == 14 and all('LAKES' in k[0] for k in dn0_diff)
            and all(ref[k]['dense_neg'] != '' and int(full[k]['always_neg']) - int(ref[k]['dense_neg']) == 6
                    for k in dn0_diff))
print(f'A5: equal to mkl_bk_neg {len(taken) - len(bk_bad)}/{len(taken)}; vs DN-0 dense column differing '
      f'{len(dn0_diff)} (expected exactly the 14 LAKES matrices, +6 each: {lakes_ok})')
verdicts.append(f'A5_{"PASS" if not bk_bad and lakes_ok else "FAIL"} bk {len(taken) - len(bk_bad)}/{len(taken)} '
                f'dn0-diff {len(dn0_diff)}')

# ---- A6
print('\n== A6 (reported): dsytrf_rook')
for eng in ('IPM', 'SQP'):
    union = [k for k in by_engine[eng] if reference(k, 1) is not None]
    nsing = [k for k in union if full[k]['rook_singular'] == '0']
    agree = sum(1 for k in nsing if int(full[k]['rook_neg']) == reference(k, 1))
    sing_all = sum(1 for k in by_engine[eng] if full[k]['rook_singular'] == '1')
    mults = [float(full[k]['rook_max_mult']) for k in by_engine[eng] if full[k]['rook_max_mult'] != '']
    res = [float(full[k]['rook_residual']) for k in by_engine[eng] if full[k]['rook_residual'] != '']
    fi = [int(full[k]['rook_factor_instr']) for k in by_engine[eng] if full[k]['rook_singular'] == '0']
    si = [int(full[k]['rook_solve_instr']) for k in by_engine[eng] if full[k]['rook_solve_instr'] not in ('', '-1')]
    print(f'A6-{eng}: negative count = reference on the union (s = 1, non-singular) {agree}/{len(nsing)}; '
          f'exactly singular {sing_all}; max |multiplier| {mx(mults):.3g}; residual median '
          f'{md(res):.3e} max {mx(res):.3e}; instructions per factorization median '
          f'{md(fi):.0f} max {mx(fi)}, per solve median {md(si):.0f} max {mx(si)}')

# ---- A7
print('\n== A7 (reported): min |D-block eigenvalue| / max |a_ij| over dense-taken factorizations (this tool\'s '
      'dsytrf on the same matrix; its count equals the arm\'s on every dense-taken matrix above)')


def q(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, int(p * (len(v) - 1) + 0.5))]


for eng in ('SQP', 'IPM'):
    for label, pred in (('all', lambda k: True), ('decidable (union)', lambda k: reference(k, 1) is not None),
                        ('undecidable', lambda k: reference(k, 1) is None)):
        v = [float(full[k]['bk_min_eig_rel']) for k in by_engine[eng]
             if dense_taken(full[k]) and pred(k) and full[k]['bk_min_eig_rel'] != '']
        if not v:
            print(f'A7-{eng} {label}: none')
            continue
        print(f'A7-{eng} {label}: taken {len(v)} q01 {q(v, .01):.2g} q05 {q(v, .05):.2g} q25 {q(v, .25):.2g} '
              f'median {statistics.median(v):.2g} <=1e-8 {sum(x <= 1e-8 for x in v)} '
              f'<=1e-12 {sum(x <= 1e-12 for x in v)} <=1e-14 {sum(x <= 1e-14 for x in v)}')

# ---- A-thr
print('\n== A-thr: the arm at the default thread setting (MKL_NUM_THREADS unset) vs MKL_NUM_THREADS=1')
moved = []
for k in keys:
    a, b = full[k], dthr[k]
    for f in ('threw', 'status', 'arm', 'state', 'pos', 'neg', 'zero', 'zero_is_derived', 'perturbed'):
        if a['always_' + f] != b['always_' + f]:
            moved.append((k, f, a['always_' + f], b['always_' + f]))
for k, f, x, y in moved:
    print(f'  DIFF {k} {f}: MKL=1 {x} default {y}')
print(f'A-thr: {len(moved)} field differences over {len(keys)} matrices')
verdicts.append('A_THR_IDENTICAL' if not moved else f'A_THR_DIFFERENCES_LISTED {len(moved)}')

print('\n== VERDICTS')
for v in verdicts:
    print(v)
