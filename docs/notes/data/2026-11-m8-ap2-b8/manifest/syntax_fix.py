#!/usr/bin/env python3
"""Compiler-guided member rename. Runs clang -fsyntax-only over the compile database, rewrites every
site the compiler attributes to a renamed member (member_manifest.tsv), and repeats until nothing is
left to fix. Prints the errors it could not attribute. Run under the box lock.
usage: syntax_fix.py <compile_commands.json> <manifest> [max_rounds] [--check-only] [--extra-flags=...]"""
import json, re, shlex, subprocess, sys, os
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor

ccj, manifest = sys.argv[1], sys.argv[2]
max_rounds = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].isdigit() else 8
check_only = '--check-only' in sys.argv
extra = []
for a in sys.argv:
    if a.startswith('--extra-flags='):
        extra = a.split('=', 1)[1].split()
man = defaultdict(dict)
by_old = defaultdict(set)
for line in open(manifest):
    if line.startswith('#') or not line.strip():
        continue
    s, o, n = line.rstrip('\n').split('\t')
    man[s][o] = n
    by_old[o].add(n)

entries = [e for e in json.load(open(ccj)) if not e['file'].endswith('.hxx.cxx')]


def cmd_of(e):
    a = shlex.split(e['command'])
    out = []
    i = 0
    while i < len(a):
        t = a[i]
        if t == '-o':
            i += 2
            continue
        if t == '-c':
            i += 1
            continue
        if t in ('-MD',):
            i += 1
            continue
        if t in ('-MT', '-MF'):
            i += 2
            continue
        if t == '-Xclang' and i + 1 < len(a) and a[i + 1] == '-include-pch':
            i += 4
            continue
        if t == '-Xclang' and i + 1 < len(a) and a[i + 1] == '-include':
            out += ['-include', a[i + 3]]
            i += 4
            continue
        if t in ('-Winvalid-pch', '-fpch-instantiate-templates'):
            i += 1
            continue
        out.append(t)
        i += 1
    return out + ['-fsyntax-only', '-ferror-limit=0', '-fno-color-diagnostics', '-fno-caret-diagnostics'] + extra + [e['file']]


def run(e):
    p = subprocess.run(cmd_of(e), cwd=e['directory'], capture_output=True, text=True)
    return p.stderr


diag = re.compile(r"^(/[^:]+):(\d+):(\d+): error: (.*)$")
pats = [
    (re.compile(r"no member named '(\w+)' in '(?:const |volatile )*(?:struct |class )?([\w:<>, ]+?)'"), 'member'),
    (re.compile(r"field designator '(\w+)' does not refer to any field in type '(?:const )?([\w:<>, ]+?)'"), 'member'),
    (re.compile(r"initializer '(\w+)' does not name a non-static data member or base class"), 'nameonly'),
    (re.compile(r"use of undeclared identifier '(\w+)'"), 'nameonly'),
]


def basename(t):
    t = re.sub(r'<.*>', '', t).strip()
    return t.split('::')[-1]


for rnd in range(max_rounds):
    with ThreadPoolExecutor(16) as ex:
        outs = list(ex.map(run, entries))
    fixes = defaultdict(set)  # file -> {(line, col, old, new)}
    unresolved = set()
    nerr = 0
    for o in outs:
        for line in o.splitlines():
            m = diag.match(line)
            if not m:
                continue
            nerr += 1
            f, ln, col, msg = m.group(1), int(m.group(2)), int(m.group(3)), m.group(4)
            hit = None
            for rx, kind in pats:
                mm = rx.search(msg)
                if not mm:
                    continue
                old = mm.group(1)
                if kind == 'member':
                    st = basename(mm.group(2))
                    if st in man and old in man[st]:
                        hit = (old, man[st][old])
                else:
                    if len(by_old.get(old, ())) == 1:
                        hit = (old, next(iter(by_old[old])))
                break
            if hit and not check_only:
                fixes[f].add((ln, col, hit[0], hit[1]))
            else:
                unresolved.add(f"{f}:{ln}:{col}: {msg}")
    nfix = 0
    for f, fx in fixes.items():
        lines = open(f, encoding='utf-8', errors='surrogateescape').read().split('\n')
        for ln, col, old, new in sorted(fx, key=lambda t: (t[0], -t[1])):
            L = lines[ln - 1]
            c = col - 1
            if L[c:c + len(old)] == old and not re.match(r'\w', L[c + len(old):c + len(old) + 1] or ' '):
                lines[ln - 1] = L[:c] + new + L[c + len(old):]
                nfix += 1
            else:
                # the caret is not on the token (macro expansion etc.): report it
                unresolved.add(f"{f}:{ln}:{col}: NOFIX {old}->{new}: {L.strip()[:120]}")
        open(f, 'w', encoding='utf-8', errors='surrogateescape').write('\n'.join(lines))
    print(f"round {rnd}: errors {nerr}, fixes applied {nfix}, unresolved {len(unresolved)}", flush=True)
    if nfix == 0:
        for u in sorted(unresolved)[:200]:
            print(u)
        break
