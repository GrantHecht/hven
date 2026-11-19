#!/usr/bin/env python3
"""RS-6a object-map generator and checker (DECLARATION-RS6a.md).
usage: rs6a_objmap_check.py BEFORE-MANIFEST AFTER-MANIFEST DECLARED-MAP [EXTRA-DECLARED-MAP ...] --out GENERATED-MAP [--unity-before OUT] [--unity-after OUT]
Source paths do not change in RS-6a, only the CMake target, so a before object is paired with the after object that has the SAME source path (key (source), target ignored).
Writes the generated `old => new` map (only objects whose build-relative path changed), the lists of unity-class objects (before/after, never paired), and requires:
  * (--unity-class-source: a source unity-batched before and plain after joins the AFTER unity class)
  * every non-unity before object has exactly one after object with its source path, and vice versa (no new, no removed object);
  * the generated map equals the union of the declared maps, as a set of lines (both directions)."""
import importlib.util, sys, os, argparse
here = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('rs_object_map', os.path.join(here, '..', '..', '..', '..', '..', 'scripts', 'rs_object_map.py'))
rom = importlib.util.module_from_spec(spec); spec.loader.exec_module(rom)
ap = argparse.ArgumentParser()
ap.add_argument('before'); ap.add_argument('after'); ap.add_argument('declared', nargs='+')
ap.add_argument('--out', required=True); ap.add_argument('--unity-before'); ap.add_argument('--unity-after')
ap.add_argument('--unity-class-source', action='append', default=[], help='a repo-relative source that was unity-batched before and is a plain object after: its AFTER object joins the unity class (compared by psym_union, not paired)')
a = ap.parse_args()
def load(p):
    """Returns ({(target, source): rel}, unity list, other list)."""
    plain, unity, other = {}, [], []
    for raw in open(p):
        rel = raw.strip()
        if not rel or rel == 'libhven.a': continue
        k = rom.key_of(rel)
        if k is None: other.append(rel); continue
        tgt, src = k
        if src.startswith('unity:'): unity.append(rel); continue
        if (tgt, src) in plain: sys.exit('%s: (%s, %s) built twice' % (p, tgt, src))
        plain[(tgt, src)] = rel
    return plain, sorted(unity), other
def pair(b, af):
    """Pair before/after objects: identical (target, source) first (a source compiled into several targets keeps its pairing in the targets that did not change, e.g. bench/ipm_corpus_leg.cpp in
    hven_sqp_corpus), then the leftovers by source alone, which must be unambiguous (a test source moves from one target to another). Returns ({before key: after key}, [errors])."""
    pairs, err = {}, []
    for k in b:
        if k in af: pairs[k] = k
    lb = {}; la = {}
    for k in b:
        if k not in pairs: lb.setdefault(k[1], []).append(k)
    for k in af:
        if k not in pairs.values(): la.setdefault(k[1], []).append(k)
    for src, ks in sorted(lb.items()):
        if src in la and len(ks) == 1 and len(la[src]) == 1: pairs[ks[0]] = la[src][0]
        elif src in la: err.append('AMBIGUOUS %s: before %s after %s' % (src, ks, la[src]))
        else: err.append('REMOVED (before object with no after object): ' + b[ks[0]])
    for src, ks in sorted(la.items()):
        if src not in lb: err.append('NEW (after object with no before object): ' + af[ks[0]])
    return pairs, err
b, ub, ob = load(a.before); af, ua, oa = load(a.after)
for src in a.unity_class_source:
    ks = [k for k in af if k[1] == src]
    if len(ks) != 1: sys.exit('--unity-class-source %s: %d after objects' % (src, len(ks)))
    ua.append(af.pop(ks[0])); 
ua.sort()
pairs, err = pair(b, af)
gen = sorted('%s => %s' % (b[k], af[v]) for k, v in pairs.items() if b[k] != af[v])
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
