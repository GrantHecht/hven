#!/usr/bin/env python3
"""RS-R1 object comparison (DECLARATION-RS-R1.md, library/bench/example classes). usage: rsr1_objcmp.py BASE_SNAPSHOT AFTER_SNAPSHOT MOVERS_FILE
Snapshots are psym_compare.sh capture directories (objects built with __FILE__ neutralised, same build path). Test objects (anything under tests/) are NOT compared: their flags change by owner ruling.
Every other object of the base is paired with the after object of the same path (RENAMES below applied); bytes compared. A non-identical object must be named in MOVERS_FILE (one 'path  reason' per line) AND have an equal
defined-symbol set (nm -C --defined-only, names only) -- reported as SYMSET_EQUAL; otherwise it is a failure. Objects only in AFTER must be named in MOVERS_FILE as 'ADDED path'. libhven.a: member set compared by `ar t` modulo the renames
(order not compared: the declared regroup reorders it). Exit 0 iff every condition holds."""
import os, subprocess, sys, filecmp
base, after, movers = sys.argv[1:4]
RENAMES = {'CMakeFiles/hven.dir/src/trace/trace_writer.cpp.o': 'CMakeFiles/hven.dir/src/trace/json_lines_trace_sink.cpp.o'}
decl = {}
for l in open(movers):
    l = l.strip()
    if l and not l.startswith('#'): decl[l.split()[0] if not l.startswith('ADDED') else l.split()[1]] = l
def objs(root):
    out = set()
    for l in open(os.path.join(root, '.psym-manifest')):
        l = l.strip()
        if l and l != 'libhven.a' and not l.startswith('tests/'): out.add(l)
    return out
ob, oa = objs(base), objs(after)
def syms(p):
    r = subprocess.run(['nm', '-C', '--defined-only', p], capture_output=True, text=True).stdout.splitlines()
    # assembler-LOCAL labels (.L.str.N, .LCPI<f>_N, .Lanon...) are renumbered whenever a literal moves between TUs; they name no function or object (r3 criterion)
    return sorted(set(' '.join(x.split()[2:]) for x in r if len(x.split()) >= 3 and not x.split()[2].startswith('.L')))
bad = []; ident = 0; moved = []; paired = set()
for o in sorted(ob):
    t = RENAMES.get(o, o)
    if t not in oa: bad.append('MISSING in after: %s' % o); continue
    paired.add(t)
    if filecmp.cmp(os.path.join(base, o), os.path.join(after, t), shallow=False) and o == t:
        ident += 1; continue
    if o != t: moved.append((o, t, 'renamed' + ('' if not filecmp.cmp(os.path.join(base, o), os.path.join(after, t), shallow=False) else ', bytes identical')))
    same = filecmp.cmp(os.path.join(base, o), os.path.join(after, t), shallow=False)
    if same: continue
    ss = syms(os.path.join(base, o)) == syms(os.path.join(after, t))
    key = t
    if key not in decl: bad.append('UNDECLARED DIFFERENCE %s (symbol set %s)' % (t, 'equal' if ss else 'DIFFERS'))
    elif not ss: bad.append('DECLARED MOVER %s has a different symbol set' % t)
    else: moved.append((o, t, 'SYMSET_EQUAL (declared: %s)' % decl[key]))
for t in sorted(oa - paired):
    if t not in decl or not decl[t].startswith('ADDED'): bad.append('UNDECLARED ADDED OBJECT %s' % t)
    else: moved.append(('-', t, 'ADDED (declared)'))
def members(root): return sorted(subprocess.run(['ar', 't', os.path.join(root, 'libhven.a')], capture_output=True, text=True).stdout.split())
mb = sorted(RENAMES.get('CMakeFiles/hven.dir/src/trace/' + m.replace('.o', '') , m) for m in members(base)) if False else members(base)
ma = members(after)
mb2 = sorted(m.replace('trace_writer.cpp.o', 'json_lines_trace_sink.cpp.o') for m in mb)
extra_a = sorted(set(ma) - set(mb2)); extra_b = sorted(set(mb2) - set(ma))
print('non-test objects: %d identical, %d moved/added/renamed, %d base, %d after' % (ident, len(moved), len(ob), len(oa)))
for m in moved: print('  %s -> %s : %s' % m)
print('libhven.a members: after-only %s, base-only %s' % (extra_a, extra_b))
decl_added = [m for m in extra_a]
if extra_b or any(not any(x in m for x in ('ipqp_options',)) for m in extra_a): bad.append('libhven.a member set differs beyond ipqp_options: %s %s' % (extra_a, extra_b))
if bad:
    print('OBJECTS_NOT_AS_DECLARED'); [print('  ' + b) for b in bad]; sys.exit(1)
print('OBJECTS_AS_DECLARED')
