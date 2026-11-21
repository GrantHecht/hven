#!/usr/bin/env python3
"""Re-point test-source path tokens in one CMake file for the RS-2 test move (code lines only; comments untouched; order preserved).
usage: rs2_cmake.py <group.map> <OLD-cmake-path>:<NEW-cmake-path> [--dry-run]
A token `[dir/]file.(cpp|h|cmake)` (optionally after `${CMAKE_CURRENT_SOURCE_DIR}/`) is resolved against the OLD directory of the CMake
file; if the result is a moved file it is re-spelled relative to the NEW directory of the CMake file, and if the CMake file itself moved
every other tracked file token is re-spelled the same way.  Tokens after `${HVEN_SRC_DIR}/` (src/ TUs, untouched by RS-2) are skipped.
Prints every replacement; the NEW file must already be in place (run after rs2_apply.py)."""
import re, sys, posixpath, subprocess
mapf, spec = sys.argv[1], sys.argv[2]
dry = '--dry-run' in sys.argv
old_cm, new_cm = spec.split(':')
moves = {}
for l in open(mapf):
    l = l.rstrip('\n')
    if ' -> ' in l:
        o, n = l.split(' -> ', 1); moves[o] = n
old_dir, new_dir = posixpath.dirname(old_cm), posixpath.dirname(new_cm)
# tracked set as it was BEFORE the move: new paths of moved files are not tracked-old, so use moves + old tracked
tracked_old = set(subprocess.run(['git', 'ls-files'], capture_output=True, text=True, check=True).stdout.split('\n'))
tracked_old = (tracked_old - set(moves.values())) | set(moves)
tok = re.compile(r'(\$\{CMAKE_CURRENT_SOURCE_DIR\}/|\$\{HVEN_SRC_DIR\}/)?((?:\.\./|[A-Za-z0-9_.-]+/)*[A-Za-z0-9_-][A-Za-z0-9_.-]*\.(?:cpp|h|cmake))')
def fix(line):
    code, sep, com = line.partition('#')
    if code.count('"') % 2 == 1: code, sep, com = line, '', ''
    def sub(m):
        pre, p = m.group(1), m.group(2)
        if pre == '${HVEN_SRC_DIR}/': return m.group(0)
        t = posixpath.normpath(posixpath.join(old_dir, p))
        if t not in tracked_old: return m.group(0)
        nt = moves.get(t, t)
        if nt == t and old_dir == new_dir: return m.group(0)
        return (pre or '') + posixpath.relpath(nt, new_dir)
    return tok.sub(sub, code) + sep + com
f = new_cm
out = []
for i, l in enumerate(open(f).read().split('\n'), 1):
    n = fix(l)
    if n != l: print('%s:%d: %s -> %s' % (f, i, l.strip(), n.strip()))
    out.append(n)
if not dry: open(f, 'w').write('\n'.join(out))
