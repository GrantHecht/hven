#!/usr/bin/env python3
# PF-2e diag log parser: pairs T(test), T(cert) and C lines by id within each per-process log file.
import sys, glob, re
def kv(line):
    d = {}
    for tok in line.split()[1:]:
        if '=' in tok:
            k, v = tok.split('=', 1); d[k] = v
    return d
def records(paths):
    for p in paths:
        cur = {}
        for line in open(p, errors='replace'):
            if line.startswith('T '):
                d = kv(line)
                cur.setdefault(d['id'], {}).setdefault(d['mode'], d)
            elif line.startswith('C '):
                d = kv(line)
                r = cur.pop(d['id'], {})
                r['C'] = d; r['file'] = p
                yield r
def flist(s): return [float(x) for x in s.split(',')]
if __name__ == '__main__':
    n = 0
    for r in records(sys.argv[1:]):
        n += 1
    print(n)
