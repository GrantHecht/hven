"""AP-2 B8 riders: the one rewrite rule of the P-SYM map (gen_maps.py's docstring states it)."""
import os, re
here = os.path.dirname(os.path.abspath(__file__))
M = os.path.join(here, '..', 'manifest')
TYPES = []
for line in open(os.path.join(M, 'ident_manifest.tsv')):
    if line.startswith('#') or not line.strip():
        continue
    f = line.rstrip('\n').split('\t')
    if f[0] == 'type':
        TYPES.append((re.compile(r'(?<![\w])' + re.escape(f[1]) + r'(?![\w])'), f[2]))
MOVED = [l.strip() for l in open(os.path.join(M, 'detail_moved_names.txt')) if l.strip()]
MOVED_RX = re.compile(r'(?<![\w:])hven::(' + '|'.join(sorted(map(re.escape, MOVED), key=len, reverse=True)
                                                    + ['continuation_detail', 'mesh_detail', 'predictor_detail'])
                      + r')(?![\w])')
FUNCS = []
for line in open(os.path.join(M, 'function_renames.tsv')):
    if line.startswith('#') or not line.strip():
        continue
    a, b = line.rstrip('\n').split('\t')
    FUNCS.append((re.compile(a), b))


def rewrite(s):
    for rx, n in TYPES:
        s = rx.sub(n, s)
    s = MOVED_RX.sub(r'hven::detail::\1', s)
    for rx, n in FUNCS:
        s = rx.sub(n, s)
    return s
