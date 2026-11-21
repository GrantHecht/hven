#!/usr/bin/env python3
# M8 DN-1 impl (DECLARATION P10): R11's flags_check.py re-pointed, criterion amended for the one added TU (design N1).
"""flags_diff.py's report holds the criterion iff NO TU present in both builds has a different flag list, no library
TU is removed, exactly one is added -- src/linear/dense_session.cpp -- and that TU's flag list is identical to the
library baseline TU's (arm A = 02065dac).
usage: flags_check.py <flags-diff.txt>"""
import re, sys
t = open(sys.argv[1]).read()
ok = True
if 'only before: none' not in t:
    print('TU REMOVED'); ok = False
if "only after:  ['src/linear/dense_session.cpp']" not in t:
    print('ADDED TU SET IS NOT EXACTLY src/linear/dense_session.cpp'); ok = False
if 'added TU src/linear/dense_session.cpp: flags identical to the library baseline TU: True' not in t:
    print('ADDED TU FLAGS DIFFER FROM THE BASELINE TU'); ok = False
n = 0
for line in t.splitlines():
    m = re.match(r'^  (src/\S+)  (.*)$', line)
    if m:
        n += 1
        print('FLAG DIFFERENCE:', line); ok = False
print(f'differing TUs {n}; none expected: {ok}')
print('FLAGS_AS_DECLARED' if ok else 'FLAGS_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
