#!/usr/bin/env python3
"""RS-4: re-spell every `#include "hven/..."` as `#include <hven/...>` in the tracked C++ files of include/ src/ tests/ bench/ examples/.

Only lines whose first token is #include and whose operand is a quoted path beginning `hven/` change; the path text, the line's trailing comment and its line ending are kept
byte-for-byte.  Test-local ("model/support/x.h"), bare sibling ("x.h"), src-private and "../.." relative includes are not touched (DECLARATION-RS4.md item 1).
usage: rs4_respell.py ROOT FILELIST     (prints the per-file counts and the total)"""
import re, sys
root, lst = sys.argv[1].rstrip('/'), sys.argv[2]
pat = re.compile(r'^(\s*#\s*include\s*)"(hven/[^"\n]*)"')
total = files = 0
for rel in open(lst).read().split():
    p = f'{root}/{rel}'
    data = open(p, 'rb').read().decode('utf-8')
    out, n = [], 0
    for line in data.splitlines(keepends=True):
        m = pat.match(line)
        if m:
            line = f'{m.group(1)}<{m.group(2)}>' + line[m.end():]
            n += 1
        out.append(line)
    if n:
        open(p, 'wb').write(''.join(out).encode('utf-8'))
        files += 1; total += n
        print(f'{n:4d} {rel}')
print(f'TOTAL {total} includes re-spelled in {files} files')
