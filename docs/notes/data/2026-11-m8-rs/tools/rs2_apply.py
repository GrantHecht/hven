#!/usr/bin/env python3
"""RS-2 mover: git mv per the RS-2 group map, then re-spell every test-local #include so it is written from the tests root.

Design v2 S7: "Every test-local include is spelled from the tests root (\"qp/support/ssn_fixtures.h\", \"support/console_capture.h\"),
with tests/ as an include root for tests and bench."  The RS-1 mover (rs_apply.py) kept relative spellings; this one is the S7 form.
Content edits are #include lines only.  Nothing is skipped silently: every rewrite is logged.

Which includes are touched (the includer is a file under tests/ or bench/; golden_rig/ and install_smoke/ keep their own layout and
their own include directories, and are touched only if they name a MOVED file):
  - an include (quoted, not "hven/...") that resolves, under the OLD tree, to a tracked file under tests/ (golden_rig/, install_smoke/ excluded)
    -> re-spelled as the target's NEW path relative to tests/.
  - an include that resolves to a file under bench/ (relative `../../bench/x.h` from tests/) -> re-spelled relative to the includer's NEW dir
    (the same text unless the includer's depth changed); a bench includer's `"x.h"` sibling is unchanged.
Exception: a bare same-directory sibling ("x.h" that is in the includer's own directory both before and after) stays bare.
Resolution order under the OLD tree: relative to the includer's dir; then the include roots the old build gave that includer
(its own top-level tests/<d> directory -- every old test directory was an include dir of its target -- then tests/, then bench/ for bench files
and tests/sqp for bench files, which was bench's only test include root).

usage: rs2_apply.py <group.map> [--dry-run] [--log LOG]      (run from the repository root)"""
import os, re, subprocess, sys, posixpath

def sh(*a):
    return subprocess.run(a, check=True, capture_output=True, text=True).stdout

args = [a for a in sys.argv[1:] if not a.startswith('--')]
dry = '--dry-run' in sys.argv
logf = None
if '--log' in sys.argv:
    logf = sys.argv[sys.argv.index('--log') + 1]
    args = [a for a in args if a != logf]
mapfile = args[0]

moves, deletes = {}, set()
for line in open(mapfile):
    line = line.rstrip('\n')
    if not line.strip():
        continue
    old, new = line.split(' -> ', 1)
    (deletes.add(old) if new.startswith('DELETE') else moves.__setitem__(old, new))
assert not deletes, 'RS-2 deletes nothing'

tracked = set(sh('git', 'ls-files').split('\n')) - {''}
for o in moves:
    if o not in tracked: raise SystemExit('source not tracked: ' + o)
for n in moves.values():
    if n in tracked: raise SystemExit('destination already exists: ' + n)
if len(set(moves.values())) != len(moves): raise SystemExit('two sources map to one destination')

CXX = ('.h', '.hpp', '.cpp', '.cc', '.cxx', '.inl')
INC = re.compile(r'^(\s*#\s*include\s*)(")([^"]+)(")(.*)$')
FIXED = ('tests/golden_rig/', 'tests/install_smoke/')
log = []

def new_loc(f): return moves.get(f, f)

def resolve(f, spelling):
    d = posixpath.dirname(f)
    t = posixpath.normpath(posixpath.join(d, spelling))
    if t in tracked: return t, 'rel'
    roots = []
    if f.startswith('tests/'):
        top = '/'.join(f.split('/')[:2])
        roots = [top, 'tests']
    elif f.startswith('bench/'):
        roots = ['bench', 'tests/sqp']
    for r in roots:
        t = posixpath.normpath(posixpath.join(r, spelling))
        if t in tracked: return t, 'root:' + r
    return None, None

edits, nrew = {}, 0
for f in sorted(tracked):
    if not f.endswith(CXX) or not f.startswith(('tests/', 'bench/')): continue
    if f.startswith('tests/golden_rig/audit/testdata/'): continue
    text = open(f, encoding='utf-8').read()
    out, changed = [], False
    for ln in text.split('\n'):
        m = INC.match(ln)
        if not m or m.group(3).startswith('hven/'):
            out.append(ln); continue
        pre, o, spelling, c, post = m.groups()
        t, kind = resolve(f, spelling)
        if t is None:
            out.append(ln); continue
        nt = moves.get(t, t)
        if kind == 'rel' and '/' not in spelling and posixpath.dirname(nt) == posixpath.dirname(new_loc(f)):
            out.append(ln); continue            # a bare same-directory sibling stays bare (e.g. the try_run dispatch probe has no tests include root)
        includer_fixed = f.startswith(FIXED)
        if t.startswith('tests/') and not t.startswith(FIXED) and not includer_fixed:
            ns = posixpath.relpath(nt, 'tests')
        elif includer_fixed and t.startswith(FIXED):
            ns = spelling                       # golden_rig / install_smoke internal include: untouched
            if kind == 'rel' and f in moves: raise SystemExit('fixed-layout file moved: ' + f)
        elif t.startswith('bench/'):
            ns = posixpath.relpath(nt, posixpath.dirname(new_loc(f))) if f.startswith('tests/') else spelling
        elif t.startswith('tests/') and includer_fixed:
            # a golden_rig/install_smoke file naming a tests/ fixture: spelled from the tests root as well
            ns = posixpath.relpath(nt, 'tests')
        else:
            ns = spelling
        if ns != spelling:
            log.append('%s: "%s" -> "%s"  [%s]' % (f, spelling, ns, kind))
            ln = pre + o + ns + c + post
            changed = True; nrew += 1
        out.append(ln)
    if changed: edits[f] = '\n'.join(out)

print('include lines rewritten: %d in %d files' % (nrew, len(edits)))
if dry:
    if logf: open(logf, 'w').write('\n'.join(log) + '\n')
    print('dry run; nothing changed'); sys.exit(0)

for o, n in sorted(moves.items()):
    os.makedirs(posixpath.dirname(n), exist_ok=True)
    sh('git', 'mv', o, n)
for f, text in edits.items():
    open(new_loc(f), 'w', encoding='utf-8').write(text)
for dp, dn, fn in os.walk('tests', topdown=False):
    if not os.listdir(dp): os.rmdir(dp)
if logf: open(logf, 'w').write('\n'.join(log) + '\n')
print('moves %d, files edited %d' % (len(moves), len(edits)))
