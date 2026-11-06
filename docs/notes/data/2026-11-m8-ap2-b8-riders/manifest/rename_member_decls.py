#!/usr/bin/env python3
"""AP-2 B8 riders: rename each member DECLARATION of member_manifest.tsv inside its struct/class body
(top level of the body only; comments untouched). usage: rename_member_decls.py <worktree> <member_manifest.tsv>"""
import re, sys
wt, man = sys.argv[1], sys.argv[2]
rows = [l.rstrip('\n').split('\t') for l in open(man) if l.strip() and not l.startswith('#')]
STRIP = re.compile(r'"(?:\\.|[^"\\\n])*"|//[^\n]*|/\*(?:.|\n)*?\*/')
for hdr, struct, old, new in rows:
    p = f'{wt}/{hdr}'
    s = open(p).read()
    blank = STRIP.sub(lambda m: ' ' * len(m.group(0)), s)  # same offsets, comments/strings blanked
    m = re.search(r'\b(?:struct|class)\s+' + re.escape(struct) + r'\b[^;{]*\{', blank)
    if not m:
        raise SystemExit(f'NO STRUCT {struct} in {hdr}')
    i, depth, start = m.end(), 1, m.end()
    hits = []
    while depth:
        c = blank[i]
        if c == '{': depth += 1
        elif c == '}': depth -= 1
        elif depth == 1:
            mm = re.match(r'(?<![\w.>:])' + re.escape(old) + r'(?=\s*(=|;|\{|\(|\[))', blank[i:])
            if mm and not re.match(r'[\w]', blank[i - 1]):
                hits.append(i)
        i += 1
    # a declaration is the name followed by = ; { ( at body top level; a call inside an inline body is depth>1
    if not hits:
        if re.search(r'(?<![\w])' + re.escape(new) + r'(?![\w])', blank[start:i]):
            print(f'{hdr} {struct}::{old} already {new}')
            continue
        raise SystemExit(f'NO DECL {struct}::{old} in {hdr}')
    for h in reversed(hits):
        s = s[:h] + new + s[h + len(old):]
    open(p, 'w').write(s)
    print(f'{hdr} {struct}::{old} -> {new} ({len(hits)})')
