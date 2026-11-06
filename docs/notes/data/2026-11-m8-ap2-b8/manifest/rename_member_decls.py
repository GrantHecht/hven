#!/usr/bin/env python3
"""Rename the DECLARATIONS of the members in member_manifest.tsv (depth-1 declaration lines of the
struct's own body only). usage: rename_member_decls.py <manifest> <header>..."""
import re, sys
from collections import defaultdict

man = defaultdict(dict)
for line in open(sys.argv[1]):
    if line.startswith('#') or not line.strip():
        continue
    s, o, n = line.rstrip('\n').split('\t')
    man[s][o] = n
done = defaultdict(set)
decl = re.compile(r'^\s*(?:template\s*<[^;{]*>\s*)?(struct|class)\s+([A-Za-z_]\w*)\b[^;]*$')
for path in sys.argv[2:]:
    lines = open(path).read().split('\n')
    changed = False
    i = 0
    while i < len(lines):
        m = decl.match(lines[i])
        if m and m.group(2) in man:
            name = m.group(2)
            # find the opening brace
            depth = 0
            j = i
            started = False
            while j < len(lines):
                code = re.sub(r'//.*', '', lines[j])
                if not started and '{' in code:
                    started = True
                if started:
                    body_depth = depth
                    if body_depth == 1:
                        for o, n in man[name].items():
                            pat = r'(?<![\w.>:])' + re.escape(o) + r'(?=\s*(=|;|\{|\[))'
                            upat = r'(using\s+[\w:<>]+::)' + re.escape(o) + r'(\s*;)'
                            new = re.sub(upat, r'\g<1>' + n + r'\2', lines[j])
                            if '(' not in code.split('=')[0] or new != lines[j]:
                                new = re.sub(pat, n, new)
                            if new != lines[j]:
                                lines[j] = new
                                changed = True
                                done[name].add(o)
                    depth += code.count('{') - code.count('}')
                    if depth == 0:
                        break
                j += 1
            i = j + 1 if started else i + 1
            continue
        i += 1
    if changed:
        open(path, 'w').write('\n'.join(lines))
        print('edited', path)
for s, d in man.items():
    missing = set(d) - done[s]
    if missing:
        print('NOT FOUND', s, sorted(missing))
