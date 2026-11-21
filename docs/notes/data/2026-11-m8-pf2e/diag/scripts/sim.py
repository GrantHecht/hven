#!/usr/bin/env python3
# Simulate the PF-2e contraction probe on diag logs. usage: sim.py [--stag Q] logs...
import sys, os, collections
sys.path.insert(0, os.path.dirname(__file__))
from parse import records, flist
STAG = 0.5; TOL = 1e-6; UND = 1e-3; STEPS = 3
import os as _os
ELIM_ONLY = _os.environ.get('ELIM_ONLY') == '1'
args = sys.argv[1:]
if args and args[0] == '--stag': STAG = float(args[1]); args = args[2:]
def kept(T):
    """rhos of the KEPT iterates (the first solve and each kept step)."""
    r = flist(T['rhos'])
    if T['brk'] == '1': r = r[:-1]
    return r
st = collections.Counter(); bad = []
for rec in records(args):
    C = rec['C']; T = rec.get('test'); K = rec.get('cert')
    if T is None: continue
    f = rec['file']
    # ---- test probe
    tr = kept(T); target = float(T['target'])
    ress = flist(T['ress'])
    if T['brk'] == '1': ress = ress[:-1]
    if T['screen'] == '0' and len(flist(T['rhos'])) > 1 and not (ELIM_ONLY and T['bord'] == '1'):
        st['refining_tests'] += 1
        cut_at = None
        for j in range(1, len(tr)):
            left = STEPS - j
            if left <= 0 or ress[j] <= target: break
            if tr[j] >= STAG * tr[j-1] and tr[j] * STAG**left >= UND:
                cut_at = j; break
        if cut_at is not None:
            st['test_cuts'] += 1
            full_solves = int(T['solves'])
            st['test_solves_saved'] += full_solves - (cut_at + 1)
            # outcome under the cut: independent stall admit; compare with final
            orig_v = C['verdict']; orig_grey = C['grey']
            if (orig_v != '0' and C['struct'] != '1') or orig_grey == '1':
                st['test_cut_VERDICT_CHANGE'] += 1; bad.append(('test', f, T['rhos'], C))
            if C['stall'] == '0': st['test_cut_stall_flag_flip'] += 1
    # ---- re-read probe
    if C['grey'] == '1' and K is not None and not (ELIM_ONLY and T['bord'] == '1'):
        st['greys'] += 1; st['flips'] += int(C['flip'])
        k = int(T['solves']) - 1  # test steps (converged tests never broke)
        kr = flist(K['rhos']); kbrk = K['brk'] == '1'
        kk = kr[:-1] if kbrk else kr
        old_new_solves = (len(kr) - 1) - k   # new solves the old re-read issued after the test's iterate
        cut_new = None
        if k >= 1 and k < STEPS:
            rp, rc = tr[k-1], tr[k]
            if rc >= STAG * rp and rc * STAG**(STEPS - k) > TOL: cut_new = 0
        if cut_new is None:
            for j in range(k + 1, len(kk)):
                left = STEPS - j
                if left <= 0: break
                if kk[j] >= STAG * kk[j-1] and kk[j] * STAG**left > TOL:
                    cut_new = j - k; break
        st['reread_new_solves_old'] += old_new_solves
        st['reread_saved_L1b'] += k
        if cut_new is not None:
            st['reread_cuts'] += 1
            st['reread_saved_probe'] += old_new_solves - cut_new
            if C['flip'] == '1':
                st['reread_cut_LOST_FLIP'] += 1; bad.append(('reread', f, T['rhos'], K['rhos'], C))
print('stag', STAG)
for k, v in sorted(st.items()): print('%-28s %d' % (k, v))
for b in bad[:40]: print('BAD', b)
