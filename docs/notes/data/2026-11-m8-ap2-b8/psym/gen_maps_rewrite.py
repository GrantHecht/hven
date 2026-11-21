"""AP-2 B8: the one rewrite rule gen_maps.py and gen_exceptions.py share (gen_maps.py's docstring states it)."""
import os, re
here = os.path.dirname(os.path.abspath(__file__))
TYPES = []
for line in open(os.path.join(here, '..', 'manifest', 'text_manifest.tsv')):
    if line.startswith('#') or not line.strip():
        continue
    f = line.rstrip('\n').split('\t')
    if f[0] == 'type':
        TYPES.append((re.compile(r'(?<![\w])' + re.escape(f[1]) + r'(?![\w])'), f[2]))
NS = [(re.compile(r'\bhven::solvers::detail::'), 'hven::detail::'),
      (re.compile(r'\bhven::solvers::'), 'hven::'),
      (re.compile(r'\bhven::utils::(TypeStorage(?:Base)?)\b'), r'hven::\1'),
      (re.compile(r'\bhven::utils::detail::'), 'hven::detail::'),
      (re.compile(r'\bhven::utils::'), 'hven::detail::')]


def rewrite(s):
    for rx, n in NS:
        s = rx.sub(n, s)
    for rx, n in TYPES:
        s = rx.sub(n, s)
    return s
