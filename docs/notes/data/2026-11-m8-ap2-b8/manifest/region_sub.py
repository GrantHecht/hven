#!/usr/bin/env python3
"""Apply word-boundary substitutions only inside C++ comments and/or string literals.
usage: region_sub.py <comments|strings|both> <map.tsv: old<TAB>new, regex-capable if line starts with 're:'> <file>..."""
import re, sys

mode, mapfile, files = sys.argv[1], sys.argv[2], sys.argv[3:]
subs = []
for line in open(mapfile):
    if not line.strip() or line.startswith('#'):
        continue
    o, n = line.rstrip('\n').split('\t')[:2]
    if o.startswith('re:'):
        subs.append((re.compile(o[3:]), n))
    else:
        subs.append((re.compile(r'(?<![\w])' + re.escape(o) + r'(?![\w])'), n))
tok = re.compile(r'//[^\n]*|/\*.*?\*/|R"([^(\s]*)\(.*?\)\1"|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)


def apply(t):
    for rx, n in subs:
        t = rx.sub(n, t)
    return t


for p in files:
    try:
        s = open(p, encoding='utf-8', errors='surrogateescape').read()
    except Exception:
        continue
    out = []
    last = 0
    for m in tok.finditer(s):
        out.append(s[last:m.start()])
        t = m.group(0)
        is_comment = t.startswith('//') or t.startswith('/*')
        is_string = t.startswith('"') or t.startswith('R"')
        if (is_comment and mode in ('comments', 'both')) or (is_string and mode in ('strings', 'both')):
            t = apply(t)
        out.append(t)
        last = m.end()
    out.append(s[last:])
    r = ''.join(out)
    if r != s:
        open(p, 'w', encoding='utf-8', errors='surrogateescape').write(r)
        print(p)
