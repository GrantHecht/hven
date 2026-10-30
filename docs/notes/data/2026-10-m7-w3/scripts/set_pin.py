"""Move a scratch clone's hven pin (SCRATCH CLONE ONLY, never pushed). Usage: set_pin.py CLONE SHA BRANCH NOTE"""
import re
import sys

clone, sha, branch, note = sys.argv[1:5]
p = f"{clone}/cmake/hven_pin.cmake"
s = open(p).read()
old = re.search(r'^set\(HVEN_PIN_SHA "[0-9a-f]{40}"\)$', s, re.M).group(0)
assert s.count(old) == 1
s = s.replace(old, f'# SCRATCH CLONE ONLY (hven M7 W3, never pushed): {note}\nset(HVEN_PIN_SHA "{sha}")')
s = re.sub(r'^set\(HVEN_PIN_BRANCH ".*"\)$', f'set(HVEN_PIN_BRANCH "{branch}")', s, flags=re.M)
s = re.sub(r'^set\(HVEN_PIN_DATE ".*"\)$', 'set(HVEN_PIN_DATE "2026-10-28")', s, flags=re.M)
open(p, "w").write(s)
print("pin set", sha, branch)
