#!/usr/bin/env python3
"""M8 F1 (copied from PF-1, re-pointed). Was: M8 PF-1 (PF-HB's instrument; DECLARATION P10): flags_diff.py's report holds the criterion iff the TU sets are equal and NO TU's flag
list differs (the fix changes no define, option or source list; arm A = bdd797e3).
usage: flags_check.py <flags-diff.txt>"""
import re, sys
t = open(sys.argv[1]).read()
ok = True
for need in ('only before: none', 'only after:  none'):
    if need not in t:
        print('TU SET DIFFERS:', need, 'absent'); ok = False
n = 0
for line in t.splitlines():
    m = re.match(r'^  (src/\S+)  (.*)$', line)
    if m:
        n += 1
        print('FLAG DIFFERENCE:', line); ok = False
print(f'differing TUs {n}; none expected: {ok}')
print('FLAGS_AS_DECLARED' if ok else 'FLAGS_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
