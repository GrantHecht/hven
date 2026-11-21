#!/usr/bin/env python3
"""AP-2 B8 riders (DECLARATION P13, step 2): the LAYOUT step. Arm MID (the code head with layout_guard.patch:
`IpmLinearOptions::accelerate_zero_tol` back under USE_ACCELERATE_SPARSE) vs arm AFTER (the code head), both
built in place in build-b8r/psym. The one source difference is that field's declaration (and the three
lines that read it, the test pins that state the layout), so PASS iff every object of compare-step2.txt with a
finding (differing, only-before or only-after > 0) is compiled from a TU whose recorded dependencies
(`ninja -C build-b8r/psym -t deps <obj>`, ninja's own depfile log) name include/hven/drivers/ipm_options.h -- a TU that cannot see the struct cannot change with it. Every such
object and its counts are listed. usage: step2_check.py <compare-step2.txt> <build-dir>"""
import os, re, subprocess, sys
cmp_, build = sys.argv[1], sys.argv[2]
ok, n = True, 0
for line in open(cmp_):
    m = re.match(r'^PER-SYMBOL\s+(\S+?): PERSYM (\d+) identical, (\d+) noise-only, (\d+) differing, (\d+) only-before, (\d+) only-after', line)
    if not m:
        continue
    obj, d, b, a = m.group(1), int(m.group(4)), int(m.group(5)), int(m.group(6))
    if d + b + a == 0:
        continue
    n += 1
    deps = subprocess.run(['ninja', '-C', build, '-t', 'deps', obj], capture_output=True, text=True).stdout
    sees = 'include/hven/drivers/ipm_options.h' in deps
    print(f'{"SEES" if sees else "BLIND"} {obj} differing {d} only-before {b} only-after {a}')
    ok &= sees
print(f'objects with findings {n}; every one sees drivers/ipm_options.h: {ok}')
print('STEP2_AS_DECLARED' if ok else 'STEP2_NOT_AS_DECLARED')
sys.exit(0 if ok else 1)
