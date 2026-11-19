#!/usr/bin/env python3
"""RS-6a object-map generator and checker (DECLARATION-RS6a.md).
usage: rs6a_objmap_check.py BEFORE-MANIFEST AFTER-MANIFEST DECLARED-MAP [EXTRA-DECLARED-MAP ...] --out GENERATED-MAP [--unity-before OUT] [--unity-after OUT]
Source paths do not change in RS-6a, only the CMake target, so a before object is paired with the after object that has the SAME source path (key (source), target ignored).
Writes the generated `old => new` map (only objects whose build-relative path changed), the lists of unity-class objects (before/after, never paired), and requires:
  * every non-unity before object has exactly one after object with its source path, and vice versa (no new, no removed object);
  * the generated map equals the union of the declared maps, as a set of lines (both directions)."""
import importlib.util, sys, os, argparse
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('rs_object_map', os.path.join(here, '..', '..', '..', '..', '..', 'scripts', 'rs_object_map.py'))
rom = importlib.util.module_from_spec(spec); spec.loader.exec_module(rom)
ap = argparse.ArgumentParser()
ap.add_argument('before'); ap.add_argument('after'); ap.add_argument('declared', nargs='+')
ap.add_argument('--out', required=True); ap.add_argument('--unity-before'); ap.add_argument('--unity-after')
a = ap.parse_args()
def load(p):
    plain, unity, other = {}, [], []
    for raw in open(p):
        rel = raw.strip()
        if not rel or rel == 'libhven.a': continue
        k = rom.key_of(rel)
        if k is None: other.append(rel); continue
        tgt, src = k
        if src.startswith('unity:'): unity.append(rel); continue
        if src in plain: sys.exit('%s: source %s built twice (%s, %s)' % (p, src, plain[src][1], rel))
        plain[src] = (tgt, rel)
    return plain, sorted(unity), other
b, ub, ob = load(a.before); af, ua, oa = load(a.after)
err = []
for s in sorted(b):
    if s not in af: err.append('REMOVED (before object with no after object): ' + b[s][1])
for s in sorted(af):
    if s not in b: err.append('NEW (after object with no before object): ' + af[s][1])
gen = sorted('%s => %s' % (b[s][1], af[s][1]) for s in b if s in af and b[s][1] != af[s][1])
open(a.out, 'w').write('\n'.join(gen) + '\n')
if a.unity_before: open(a.unity_before, 'w').write('\n'.join(ub) + '\n')
if a.unity_after: open(a.unity_after, 'w').write('\n'.join(ua) + '\n')
decl = set()
for d in a.declared:
    decl |= set(l.strip() for l in open(d) if l.strip() and not l.startswith('#'))
g = set(gen)
for l in sorted(g - decl): err.append('GENERATED but not declared: ' + l)
for l in sorted(decl - g): err.append('DECLARED but not generated: ' + l)
print('before objects %d (+%d unity, +%d other), after objects %d (+%d unity, +%d other); generated map %d lines, declared %d lines' % (len(b), len(ub), len(ob), len(af), len(ua), len(oa), len(gen), len(decl)))
if err:
    print('\n'.join(err)); print('OBJMAP_MISMATCH'); sys.exit(1)
print('OBJMAP_EQUALS_DECLARED')
