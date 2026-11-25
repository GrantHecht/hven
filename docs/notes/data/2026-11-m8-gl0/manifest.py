#!/usr/bin/env python3
"""M8 AP-5 G7 (DECLARATION.md; copied from ../2026-11-m8-ap5-g6/manifest-r2.py, roots re-pointed): MANIFEST of the raw dumps kept
under the ignored .scratch/ (sha256, size, path relative to the worktree).
usage: manifest.py <worktree> > MANIFEST.txt"""
import hashlib
import os
import sys

wt = sys.argv[1]
roots = ['gl0-runtime', 'gl0-runtime-aa', 'gl0-suites', 'gl0-replay', 'gl0-interior', 'gl0-install-smoke', 'gl0-mutation']
print('# sha256  size  path (raw dumps under .scratch/, not committed)')
for r in roots:
    base = os.path.join(wt, '.scratch', r)
    for dirpath, _, files in sorted(os.walk(base)):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            h = hashlib.sha256(open(p, 'rb').read()).hexdigest()
            print('%s  %d  %s' % (h, os.path.getsize(p), os.path.relpath(p, wt)))
