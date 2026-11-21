#!/usr/bin/env python3
"""B8 phase N: the namespace rewrite. hven::solvers -> hven; hven::solvers::detail -> hven::detail;
hven::utils -> hven::detail (internal utilities) except TypeStorage/TypeStorageBase -> hven.
usage: ns_rewrite.py <file>...   (rewrites in place, prints the files changed)"""
import re, sys

RULES = [
    # TypeStorage is public: hven::utils::TypeStorage -> hven::TypeStorage (before the generic utils rule)
    (re.compile(r'\bhven::utils::(TypeStorage(?:Base)?)\b'), r'hven::\1'),
    (re.compile(r'\bhven::utils::detail::'), 'hven::detail::'),
    (re.compile(r'\bhven::utils::'), 'hven::detail::'),
    (re.compile(r'\bnamespace hven::utils\b'), 'namespace hven::detail'),
    (re.compile(r'\bhven::utils\b'), 'hven::detail'),
    (re.compile(r'\bhven::solvers::'), 'hven::'),
    (re.compile(r'\bnamespace hven::solvers\b'), 'namespace hven'),
    (re.compile(r'\bhven::solvers\b'), 'hven'),
    # bare qualifiers written inside namespace hven
    (re.compile(r'(?<![:\w])solvers::'), 'hven::'),
    (re.compile(r'(?<![:\w])utils::(TypeStorage(?:Base)?)\b'), r'hven::\1'),
    (re.compile(r'(?<![:\w])utils::detail::'), 'detail::'),
    (re.compile(r'(?<![:\w])utils::'), 'detail::'),
]

for path in sys.argv[1:]:
    with open(path, encoding='utf-8', errors='surrogateescape') as f:
        s = f.read()
    t = s
    for rx, rep in RULES:
        t = rx.sub(rep, t)
    if t != s:
        with open(path, 'w', encoding='utf-8', errors='surrogateescape') as f:
            f.write(t)
        print(path)
