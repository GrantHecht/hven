#!/usr/bin/env python3
"""AP-2 B8 (DECLARATION P10): flags_diff.py's report holds the criterion iff the TU sets are equal and every
TU whose flag list differs differs ONLY by -DHVEN_VERSION_STRING="0.1.0" -> "0.2.0" (D7).
usage: flags_check.py <flags-diff.txt>"""
import re, sys
t = open(sys.argv[1]).read()
ok = True
for need in ('only before: none', 'only after:  none'):
    if need not in t:
        print('TU SET DIFFERS:', need, 'absent'); ok = False
want = "-['-DHVEN_VERSION_STRING=\"0.1.0\"'] +['-DHVEN_VERSION_STRING=\"0.2.0\"']"
n = 0
for line in t.splitlines():
    m = re.match(r'^  (src/\S+)  (.*)$', line)
    if m:
        n += 1
        if m.group(2) != want:
            print('OTHER FLAG DIFFERENCE:', line); ok = False
print(f'differing TUs {n}; every one only the version define: {ok}')
print('FLAGS_AS_DECLARED' if ok else 'FLAGS_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
