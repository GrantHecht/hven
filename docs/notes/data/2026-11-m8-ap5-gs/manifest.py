#!/usr/bin/env python3
"""M8 AP-5 G-S (DECLARATION.md; copied from ../2026-11-m8-ap5-gs legs, roots re-pointed): MANIFEST of the raw dumps kept
under the ignored .scratch/ (sha256, size, path relative to the worktree).
usage: manifest.py <worktree> > MANIFEST.txt"""
import hashlib
import os
import sys

wt = sys.argv[1]
roots = ['gs-suites', 'gs-replay', 'gs-interior', 'gs-install-smoke', 'gs-mutation']
print('# sha256  size  path (raw dumps under .scratch/, not committed)')
for r in roots:
    base = os.path.join(wt, '.scratch', r)
    for dirpath, _, files in sorted(os.walk(base)):
        for f in sorted(files):
            p = os.path.join(dirpath, f)
            h = hashlib.sha256(open(p, 'rb').read()).hexdigest()
            print('%s  %d  %s' % (h, os.path.getsize(p), os.path.relpath(p, wt)))
