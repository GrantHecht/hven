#!/usr/bin/env python3
"""RS-R1 compile-flags diff (DECLARATION-RS-R1.md G3). usage: rsr1_flags.py CC_BASE.json CC_AFTER.json ROOT
Compares configure-time compile databases (no object compiled). Non-test TUs (src/, bench/, examples/) of BASE are paired with AFTER by repo-relative source path (RENAMES applied) and their token lists must be
IDENTICAL after normalising the per-TU output/dependency arguments (-o, -MF, -MT) and the source operand. Test TUs (under tests/) are reported, not asserted: their flags change by owner ruling (RELEASE_O1 per
directory, no unity). Exit 0 iff every non-test TU matches."""
import json, os, re, sys
cb, ca, root = sys.argv[1:4]
RENAMES = {'src/trace/trace_writer.cpp': 'src/trace/json_lines_trace_sink.cpp'}
def load(p):
    out = {}
    for e in json.load(open(p)):
        f = os.path.relpath(e['file'], root); cmd = e.get('command') or ' '.join(e['arguments'])
        toks = cmd.split(); norm = []; i = 0
        while i < len(toks):
            t = toks[i]
            if t in ('-o', '-MF', '-MT'): i += 2; continue
            if t == '-c': i += 2; continue
            norm.append(t); i += 1
        out.setdefault(f, []).append(norm)
    return out
b, a = load(cb), load(ca)
bad = []; n = 0; tests_b = sum(1 for f in b if f.startswith('tests/')); tests_a = sum(1 for f in a if f.startswith('tests/'))
for f, v in sorted(b.items()):
    if f.startswith('tests/'): continue
    t = RENAMES.get(f, f)
    if t not in a: bad.append('missing in after: %s' % f); continue
    n += 1
    ca_ = a[t]
    if len(v) != len(ca_) or any(x != y for x, y in zip(sorted(v), sorted(ca_))): bad.append('flags differ: %s' % f)
extra = [f for f in a if not f.startswith('tests/') and f not in {RENAMES.get(x, x) for x in b}]
print('non-test TUs compared: %d; test TUs: base %d after %d (reported, not asserted); after-only non-test TUs: %s' % (n, tests_b, tests_a, extra))
if bad: print('FLAGS_NOT_AS_DECLARED'); [print('  ' + x) for x in bad]; sys.exit(1)
print('FLAGS_AS_DECLARED')
