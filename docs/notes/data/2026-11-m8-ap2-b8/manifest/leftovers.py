#!/usr/bin/env python3
"""Count remaining occurrences of each manifest OLD token. usage: leftovers.py <manifest> <file>..."""
import re, sys
from collections import Counter, defaultdict
olds = defaultdict(set)
for line in open(sys.argv[1]):
    if line.startswith('#') or not line.strip():
        continue
    s, o, n = line.rstrip('\n').split('\t')
    olds[o].add(s)
cnt = Counter()
where = defaultdict(list)
texts = {}
for p in sys.argv[2:]:
    try:
        texts[p] = open(p, errors='ignore').read()
    except Exception:
        pass
for o in olds:
    rx = re.compile(r'(?<![\w])' + re.escape(o) + r'(?![\w])')
    for p, t in texts.items():
        k = len(rx.findall(t))
        if k:
            cnt[o] += k
            where[o].append(f"{p}:{k}")
for o, k in sorted(cnt.items(), key=lambda x: -x[1]):
    print(f"{o}\t{k}\t{','.join(sorted(olds[o]))}\t{' '.join(where[o][:6])}")
