#!/usr/bin/env python3
"""AP-2 B8 riders: apply the text-safe part of the riders' manifest (ident_manifest.tsv) to C++ sources.
Rewrites CODE and COMMENTS; never the inside of a string or character literal (a string literal that must
follow a rename -- an option dump's enumerator name -- is edited by hand, and listed in the naming note).
Manifest lines (tab-separated):
  type        OLD  NEW   an identifier unique to the entity it names; word boundaries
  token       OLD  NEW   same mechanics, for an enumerator / macro / function name unique in the tree
  qualified   OLD  NEW   OLD is a `Scope::name` sequence (word boundaries at both ends)
  header      OLD  NEW   an include path
usage: apply_ident_manifest.py <manifest.tsv> <file>..."""
import re, sys

manifest, files = sys.argv[1], sys.argv[2:]
rules = []
for line in open(manifest):
    if not line.strip() or line.startswith('#'):
        continue
    f = line.rstrip('\n').split('\t')
    kind, old, new = f[0], f[1], f[2]
    if kind not in ('type', 'token', 'qualified', 'header', 'member-decl'):
        raise SystemExit('unknown kind ' + kind)
    if kind == 'member-decl':
        continue
    rules.append((kind, re.compile(r'(?<![\w])' + re.escape(old) + r'(?![\w])'), new))

TOK = re.compile(r'''R"([^(\s]{0,16})\((?:.|\n)*?\)\1"|"(?:\\.|[^"\\\n])*"|'(?:\\.|[^'\\\n])+'|//[^\n]*|/\*(?:.|\n)*?\*/''')


def rewrite_code(s, header_only=False):
    for kind, rx, new in rules:
        if header_only != (kind == 'header'):
            continue
        s = rx.sub(new, s)
    return s


def process(text):
    out, pos = [], 0
    for m in TOK.finditer(text):
        out.append(rewrite_code(text[pos:m.start()]))
        tok = m.group(0)
        if tok.startswith('//') or tok.startswith('/*'):
            tok = rewrite_code(tok)
        elif tok.startswith('"') and text[max(0, m.start() - 16):m.start()].rstrip().endswith('#include'):
            tok = rewrite_code(tok, header_only=True)
        out.append(tok)
        pos = m.end()
    out.append(rewrite_code(text[pos:]))
    return ''.join(out)


for path in files:
    try:
        s = open(path, encoding='utf-8', errors='surrogateescape').read()
    except (IsADirectoryError, FileNotFoundError):
        continue
    # <...> includes are code text; header rules apply to them through the code pass.
    t = process(s)
    # header rules also inside <...> includes (code segments already got type/token rules only)
    t = re.sub(r'(#include\s*<)([^>]+)(>)', lambda m: m.group(1) + rewrite_code(m.group(2), header_only=True) + m.group(3), t)
    if t != s:
        open(path, 'w', encoding='utf-8', errors='surrogateescape').write(t)
        print(path)
