#!/usr/bin/env python3
# M8 DN-1d (../DECLARATION.txt): copied from ../../2026-11-m8-dn1c-sqp-check/ and re-pointed (worktree dn1d, evidence this directory, arm A and arm B trees build-dn1de/, arm A 4776aa2ff rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1c (../DECLARATION.txt): copied from ../../2026-11-m8-dn1b-guard/ and re-pointed (worktree dn1c, evidence this directory, arm A and arm B trees build-dn1ce/, arm A 2199d1a2f rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl (DECLARATION P10): R11's flags_check.py re-pointed, criterion amended for the one added TU (design N1).
"""DN-1c criterion (DECLARATION P10, as DN-1b's): flags_diff.py's report holds iff NO TU present in both builds has a different
flag list and no TU is added or removed (arm A = 2199d1a2f).
usage: flags_check.py <flags-diff.txt>"""
import re, sys
t = open(sys.argv[1]).read()
ok = True
if 'only before: none' not in t:
    print('TU REMOVED'); ok = False
if 'only after:  none' not in t:
    print('TU ADDED'); ok = False
n = 0
for line in t.splitlines():
    m = re.match(r'^  (src/\S+)  (.*)$', line)
    if m:
        n += 1
        print('FLAG DIFFERENCE:', line); ok = False
print(f'differing TUs {n}; none expected: {ok}')
print('FLAGS_AS_DECLARED' if ok else 'FLAGS_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
