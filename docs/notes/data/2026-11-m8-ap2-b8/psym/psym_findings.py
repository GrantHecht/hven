#!/usr/bin/env python3
"""List the findings of a psym compare.txt: the PER-SYMBOL blocks' and the SYMBOLS coverage blocks'
ONLY-BEFORE / ONLY-AFTER / DIFFERS lines (not EXCEPTION lines). usage: psym_findings.py <compare.txt> [object-substring]"""
import re, sys
flt = sys.argv[2] if len(sys.argv) > 2 else ''
cur = None
for line in open(sys.argv[1]):
    m = re.match(r'^(PER-SYMBOL|SYMBOLS)\s+(\S+?):', line)
    if m:
        cur = m.group(2)
        continue
    if not line.startswith(' '):
        cur = None
        continue
    m = re.match(r'^              (DIFFERS|ONLY-BEFORE|ONLY-AFTER|COLLISION)\s+(.*)$', line)
    if m and cur and flt in cur:
        print(f'{cur}\t{m.group(1)}\t{m.group(2).rstrip()}')
