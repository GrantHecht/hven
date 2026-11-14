#!/usr/bin/env python3
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 / PF-1 (see legs/*.sh line 2), byte for byte below this line.
"""M8 F1 (copied from PF-1, re-pointed). Was: M8 PF-1 (PF-HB's instrument; DECLARATION P9; the B8 riders' instrument, byte for byte below this docstring): compare an Accelerate
sweep listing with the B8 riders' (the pin, argument 1), per FILE and CLASS.
PASS (pass A) iff: no OTHER line; every file present in both has the same set of classes, except that
KNOWN-SIZEOF may be gone from tests/drivers/test_options_dump.cpp (the declared delta: the file is then
CLEAN); a file in B8's listing and not in the riders' (it no longer mentions USE_ACCELERATE_SPARSE) is
listed as LEFT, a new file as NEW -- NEW fails, LEFT is reported. usage: accel_compare.py <b8> <riders> <A|B>"""
import sys, collections
def load(p):
    d = collections.defaultdict(set)
    for line in open(p):
        f = line.rstrip('\n').split('\t')
        if len(f) >= 2 and f[1]:
            d[f[0]].add(f[1])
    return d
b8, rd, tag = load(sys.argv[1]), load(sys.argv[2]), sys.argv[3]
ok = True
if any('OTHER' in c for c in rd.values()):
    ok = False; print(f'PASS {tag}: OTHER lines present:', [f for f, c in rd.items() if 'OTHER' in c])
for f in sorted(set(b8) | set(rd)):
    if f not in rd:
        print(f'PASS {tag}: LEFT {f} (B8 {sorted(b8[f])})'); continue
    if f not in b8:
        print(f'PASS {tag}: NEW {f} {sorted(rd[f])}'); ok = False; continue
    want = set(b8[f])
    if f == 'tests/drivers/test_options_dump.cpp' and want == {'KNOWN-SIZEOF'}:
        want = {'CLEAN'}
    if rd[f] != want:
        print(f'PASS {tag}: CLASS {f} B8 {sorted(b8[f])} riders {sorted(rd[f])}'); ok = False
print(f'PASS_{tag}_AS_DECLARED' if ok else f'PASS_{tag}_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
