#!/usr/bin/env python3
"""Apply (or, with --inverse, revert) the text-safe part of the B8 manifest.
usage: apply_text_manifest.py [--inverse] <manifest.tsv> <file>..."""
import re, sys

args = sys.argv[1:]
inverse = False
if args and args[0] == '--inverse':
    inverse = True
    args = args[1:]
manifest, files = args[0], args[1:]
rules = []
for line in open(manifest):
    if not line.strip() or line.startswith('#'):
        continue
    f = line.rstrip('\n').split('\t')
    kind = f[0]
    if kind in ('type', 'token'):
        old, new = f[1], f[2]
        if inverse:
            old, new = new, old
        rules.append((re.compile(r'(?<![\w])' + re.escape(old) + r'(?![\w])'), new))
    elif kind == 'enumerator':
        enum = f[1].split('::')[-1]
        old, new = f[2], f[3]
        if inverse:
            # kThread\1 -> Thread\1
            old = new.replace('\\1', '([0-9]+)')
            new = f[2].replace('([0-9]+)', '\\1')
            rules.append((re.compile(r'(\b' + enum + r'::)' + old + r'(?![\w])'),
                          lambda m, n=new: m.group(1) + m.expand(n.replace('\\1', '\\2'))))
            continue
        rules.append((re.compile(r'(\b' + enum + r'::)' + old + r'(?![\w])'),
                      lambda m, n=new: m.group(1) + m.expand(n.replace('\\1', '\\2'))))
    elif kind == 'header':
        old, new = f[1], f[2]
        if inverse:
            old, new = new, old
        rules.append((re.compile(r'(?<![\w])' + re.escape(old) + r'(?![\w])'), new))
for path in files:
    try:
        s = open(path, encoding='utf-8', errors='surrogateescape').read()
    except (IsADirectoryError, FileNotFoundError):
        continue
    t = s
    for rx, rep in rules:
        t = rx.sub(rep, t)
    if t != s:
        open(path, 'w', encoding='utf-8', errors='surrogateescape').write(t)
        print(path)
