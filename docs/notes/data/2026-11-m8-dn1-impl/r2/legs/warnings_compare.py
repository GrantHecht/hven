#!/usr/bin/env python3
# M8 DN-1 r2, P8 (../../DECLARATION-r2.txt, worded per settler ruling C269): arm B's Release warning set equals arm A's.
# usage: warnings_compare.py <arm A build log> <arm B build log>
# A warning is (file, message): every `<path>:<line>:<col>: warning: <message>` line, the path normalized -- the worktree
# prefix, arm A's export prefix (build-dn1e/armA-src/) and leading ../ dropped, and either arm's build directory
# (build-dn1e/armA/, build-dn1r2/rel/) replaced by <build>/ -- and line:col dropped. The SET of distinct pairs is the
# criterion; the per-pair counts are reported beside it, not judged.
import collections
import re
import sys

LINE = re.compile(r'^(?P<path>[^\s:][^:]*):\d+:\d+: warning: (?P<msg>.*)$')


def norm(path):
    path = re.sub(r'^.*?/build-dn1e/armA-src/', '', path)
    path = re.sub(r'^.*?/worktrees/dn1/', '', path)
    path = re.sub(r'^(\.\./)+', '', path)
    path = re.sub(r'^(build-dn1e/armA|build-dn1r2/rel)/', '<build>/', path)
    return path


def read(log):
    c = collections.Counter()
    with open(log, errors='replace') as f:
        for line in f:
            m = LINE.match(line.rstrip('\n'))
            if m:
                c[(norm(m.group('path')), m.group('msg'))] += 1
    return c


a = read(sys.argv[1])
b = read(sys.argv[2])
print(f'arm A: {sum(a.values())} warning lines, {len(a)} distinct (file, message)')
print(f'arm B: {sum(b.values())} warning lines, {len(b)} distinct (file, message)')
only_a = sorted(set(a) - set(b))
only_b = sorted(set(b) - set(a))
for tag, keys, src in (('ONLY IN A', only_a, a), ('ONLY IN B', only_b, b)):
    for k in keys:
        print(f'{tag}: {src[k]:3d} {k[0]}: {k[1]}')
moved = [k for k in set(a) & set(b) if a[k] != b[k]]
for k in sorted(moved):
    print(f'COUNT MOVED (reported): A {a[k]} B {b[k]} {k[0]}: {k[1]}')
print('## the set (arm B)')
for k in sorted(b):
    print(f'{b[k]:3d} {k[0]}: {k[1]}')
if only_a or only_b:
    print('WARNINGS_SET_DIFFERS')
    sys.exit(1)
print('WARNINGS_SET_EQUAL')
