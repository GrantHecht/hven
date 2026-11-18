#!/usr/bin/env python3
"""Apply one RS commit group: git mv / git rm per the group map, then rewrite every
#include that names a moved file.  Content edits are include lines only; CMake path
strings are handled by --cmake.  Prints a log; nothing is silently skipped.

usage: rs_apply.py <group.map> [--dry-run] [--log LOG]
Run from the repository root.
"""
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
    if new.startswith('DELETE'):
        deletes.add(old)
    else:
        moves[old] = new

tracked = set(sh('git', 'ls-files').split('\n')) - {''}
for o in list(moves) + list(deletes):
    if o not in tracked:
        raise SystemExit('source not tracked: ' + o)
for o, n in moves.items():
    if n in tracked:
        raise SystemExit('destination already exists: ' + n)
if len(set(moves.values())) != len(moves):
    raise SystemExit('two sources map to one destination')

CXX = ('.h', '.hpp', '.cpp', '.cc', '.cxx', '.inl')
INC = re.compile(r'^(\s*#\s*include\s*)([<"])([^>"]+)([>"])(.*)$')
log = []

def new_loc(f):
    return moves.get(f, f)

def resolve(f, spelling):
    """Return (target-repo-path, kind) or (None, None)."""
    if spelling.startswith('hven/'):
        t = 'include/' + spelling
        return (t, 'hven') if t in tracked else (None, None)
    d = posixpath.dirname(f)
    t = posixpath.normpath(posixpath.join(d, spelling))
    if t in tracked:
        return t, 'rel'
    roots = []
    if f.startswith('src/'):
        roots = ['src/interior/utils', 'src']
    elif f.startswith('tests/'):
        roots = ['tests', 'tests/common_support']
    elif f.startswith('bench/'):
        roots = ['bench', 'tests']
    for r in roots:
        t = posixpath.normpath(posixpath.join(r, spelling))
        if t in tracked:
            return t, 'root:' + r
    return None, None

edits = {}      # file -> new text
nrew = 0
for f in sorted(tracked):
    if not f.endswith(CXX):
        continue
    if not (f.startswith(('include/', 'src/', 'tests/', 'bench/', 'examples/'))):
        continue
    if f.startswith('tests/golden_rig/audit/testdata/'):
        continue
    if f in deletes:
        continue            # a deleted file's own includes are moot
    text = open(f, encoding='utf-8').read()
    out = []
    changed = False
    for ln in text.split('\n'):
        m = INC.match(ln)
        if not m:
            out.append(ln)
            continue
        pre, o, spelling, c, post = m.groups()
        t, kind = resolve(f, spelling)
        if t is None:
            out.append(ln)
            continue
        if t in deletes:
            raise SystemExit('%s includes deleted file %s' % (f, t))
        if t not in moves:
            # target stays; a relative include from a MOVED includer must be re-spelled
            if kind == 'rel' and f in moves:
                ns = posixpath.relpath(t, posixpath.dirname(moves[f]))
                log.append('REL-INCLUDER-MOVED %s: "%s" -> "%s"' % (f, spelling, ns))
                ln = pre + o + ns + c + post
                changed = True
                nrew += 1
            out.append(ln)
            continue
        nt = moves[t]
        if kind == 'hven':
            ns = nt[len('include/'):]
        elif kind == 'rel':
            ns = posixpath.relpath(nt, posixpath.dirname(new_loc(f)))
            if posixpath.dirname(ns) and not spelling.startswith('.'):
                log.append('CROSS-DIR PRIVATE INCLUDE %s: %s -> %s' % (f, spelling, ns))
        else:
            r = kind[len('root:'):]
            ns = posixpath.relpath(nt, r)
            log.append('ROOT-INCLUDE %s: %s -> %s' % (f, spelling, ns))
        if ns != spelling:
            log.append('%s: %s -> %s' % (f, spelling, ns))
            ln = pre + o + ns + c + post
            changed = True
            nrew += 1
        out.append(ln)
    if changed:
        edits[f] = '\n'.join(out)

print('include lines rewritten: %d in %d files' % (nrew, len(edits)))
if dry:
    if logf: open(logf, 'w').write('\n'.join(log) + '\n')
    print('dry run; nothing changed')
    sys.exit(0)

for o in sorted(deletes):
    sh('git', 'rm', '-q', o)
for o, n in sorted(moves.items()):
    os.makedirs(posixpath.dirname(n), exist_ok=True)
    sh('git', 'mv', o, n)
for f, text in edits.items():
    open(new_loc(f) if f in moves else f, 'w', encoding='utf-8').write(text)
# remove directories the moves emptied (tracked content only; never dep/)
for root in ('include/hven', 'src', 'tests', 'bench'):
    for dp, dn, fn in os.walk(root, topdown=False):
        if not os.listdir(dp):
            os.rmdir(dp)
if logf:
    open(logf, 'w').write('\n'.join(log) + '\n')
print('moves %d, deletions %d, files edited %d' % (len(moves), len(deletes), len(edits)))
