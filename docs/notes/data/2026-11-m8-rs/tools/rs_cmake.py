#!/usr/bin/env python3
"""Re-point source-path strings in CMake files (code lines only, comments untouched; order preserved).
usage: rs_cmake.py <group.map> <CMakeLists.txt>...
src/CMakeLists.txt: a bare `dir/file.cpp` token (relative to src/) on a code line.  Any CMake file: `${HVEN_SRC_DIR}/dir/file.cpp`
and `${CMAKE_CURRENT_SOURCE_DIR}/../src/dir/file.cpp`-style spellings are handled by the same token rule applied to the text after `src/`
or `${HVEN_SRC_DIR}/`.  Prints every replacement."""
import re, sys
mp = {}
for l in open(sys.argv[1]):
    l = l.rstrip('\n')
    if ' -> ' not in l: continue
    o, n = l.split(' -> ', 1)
    if o.startswith('src/') and not n.startswith('DELETE'):
        mp[o[4:]] = n[4:]
bare = re.compile(r'^(\s+)((?:[A-Za-z_]\w*/)+[A-Za-z_][\w.]*\.cpp)(\s*)$')
dirtok = re.compile(r'(\$\{HVEN_SRC_DIR\}/)((?:[A-Za-z_]\w*/)+[A-Za-z_][\w.]*\.(?:cpp|h))')
srcpath = re.compile(r'(/src/)((?:[A-Za-z_]\w*/)+[A-Za-z_][\w.]*\.(?:cpp|h))')
def fix(line, is_src_cmake):
    code, sep, com = line.partition('#')
    if code.count('"') % 2 == 1:
        code, sep, com = line, '', ''
    if is_src_cmake:
        m = bare.match(code)
        if m and m.group(2) in mp:
            code = m.group(1) + mp[m.group(2)] + m.group(3)
    code = dirtok.sub(lambda m: m.group(1) + mp.get(m.group(2), m.group(2)), code)
    code = srcpath.sub(lambda m: m.group(1) + mp.get(m.group(2), m.group(2)), code)
    return code + sep + com
for f in sys.argv[2:]:
    is_src = f.endswith('src/CMakeLists.txt')
    out = []
    for i, l in enumerate(open(f).read().split('\n'), 1):
        n = fix(l, is_src)
        if n != l: print('%s:%d: %s -> %s' % (f, i, l.strip(), n.strip()))
        out.append(n)
    open(f, 'w').write('\n'.join(out))
