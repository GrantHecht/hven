#!/usr/bin/env python3
# Every example includes the umbrella header and nothing else from hven (AP-1 section 7).
import glob, os, re
os.chdir('/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4/examples')
for p in sorted(glob.glob('*.cpp')):
    lines = open(p).read().split('\n')
    idx = [i for i, l in enumerate(lines) if re.match(r'#include "hven/', l)]
    assert idx, p
    first = idx[0]
    lines[first] = '#include <hven/hven.h>'
    for i in reversed(idx[1:]):
        del lines[i]
    open(p, 'w').write('\n'.join(lines))
    print(p, len(idx))
