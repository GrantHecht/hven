#!/usr/bin/env python3
"""RS-3 comment re-pointer. Rewrites pre-RS path tokens (resolved through the move map) that sit in COMMENTS of
include/ src/ tests/ bench/ examples/ scripts/ cmake/ CMakeLists.txt .github.  A token inside a string literal, or in code, is never edited:
it is listed (--report) for manual disposition.  A token is rewritten only if it is NOT a suffix of a file that exists in the tree today
and IS a suffix of exactly one pre-RS path in the map.
usage: rs3_comments.py <worktree> <map> [--apply] [--report FILE] [--literal-ok f1,f2,...]
--literal-ok: files in which a token in a string/diagnostic text (never an artifact-reaching literal) may also be rewritten"""
import re, subprocess, sys, os
wt, mp = sys.argv[1], sys.argv[2]
apply_ = '--apply' in sys.argv
lit_ok = set(sys.argv[sys.argv.index('--literal-ok') + 1].split(',')) if '--literal-ok' in sys.argv else set()
rep = sys.argv[sys.argv.index('--report') + 1] if '--report' in sys.argv else None
old2new = {}
for l in open(mp):
    if ' -> ' in l:
        a, b = l.strip().split(' -> ')
        old2new[a] = b
cur = subprocess.check_output(['git', '-C', wt, 'ls-files'], text=True).split('\n')
cur = [c for c in cur if c]
def suffixes(p):
    parts = p.split('/')
    return {'/'.join(parts[i:]) for i in range(len(parts))}
cur_suf = set()
for c in cur: cur_suf |= suffixes(c)
old_by_suf = {}
for o in old2new:
    for s in suffixes(o):
        old_by_suf.setdefault(s, []).append(o)
TOK = re.compile(r'(?<![\w/.\-])((?:[\w.\-]+/)+[\w.\-]+\.(?:h|cpp|cmake|txt|py|sh|json|jsonl|csv|in|yml))(?![\w/])')
ROOTS = ('include/hven/', 'src/', 'tests/')
def resolve(tok):
    t = tok
    pre = ''
    for p in ('include/hven/', 'hven/', 'src/', 'tests/'):
        if t.startswith(p):
            pre = p; break
    if t in cur_suf or t.lstrip('./') in cur_suf: return None
    c = old_by_suf.get(t)
    if not c: return None
    if len(c) > 1: return ('AMBIG', c)
    old = c[0]; new = old2new[old]
    if pre in ('include/hven/', 'src/', 'tests/'): return ('OK', new)
    if pre == 'hven/': return ('OK', new[len('include/'):]) if new.startswith('include/hven/') else ('AMBIG', [new])
    for r in ROOTS:
        if new.startswith(r): return ('OK', new[len(r):])
    return ('AMBIG', [new])
def comment_start(path, line):
    ext = os.path.splitext(path)[1]
    if ext in ('.h', '.cpp', '.hpp', '.cc'):
        s = line.lstrip()
        if s.startswith(('//', '*', '/*')): return 0
        i = line.find('//')
        return i if i >= 0 else None
    if ext in ('.cmake', '.txt', '.sh', '.py', '.yml', '.in') or path.endswith('CMakeLists.txt'):
        i = line.find('#')
        return i if i >= 0 else None
    return None
def in_literal(line, pos, cstart):
    # a token before the comment start, or inside a "..." that opens after the comment marker is fine for // comments only if marker is outside quotes
    return cstart is None or pos < cstart
files = [f for f in cur if f.startswith(('include/', 'src/', 'tests/', 'bench/', 'examples/', 'scripts/', 'cmake/', '.github/', 'bindings/')) or f in ('CMakeLists.txt',)]
files = [f for f in files if not re.search(r'\.(csv|jsonl|json|txt|md|in)$', f) or f.endswith(('CMakeLists.txt', '.cmake', '.in'))]
files = [f for f in files if '/golden/' not in f and '/golden_rig/data' not in f]
out = []; nedit = 0; nfile = 0
for f in files:
    p = os.path.join(wt, f)
    try: lines = open(p, encoding='utf-8').read().split('\n')
    except Exception: continue
    changed = False
    for i, line in enumerate(lines):
        if '#include' in line: continue
        cs = comment_start(f, line)
        new_line = line; off = 0
        for m in TOK.finditer(line):
            r = resolve(m.group(1))
            if r is None: continue
            if r[0] == 'AMBIG':
                out.append(f'AMBIG {f}:{i+1}: {m.group(1)} -> {r[1]}'); continue
            if (cs is None or m.start() < cs) and f not in lit_ok:
                out.append(f'LITERAL/CODE {f}:{i+1}: {m.group(1)} -> {r[1]}'); continue
            a, b = m.start() + off, m.end() + off
            new_line = new_line[:a] + r[1] + new_line[b:]; off += len(r[1]) - (m.end() - m.start())
            nedit += 1
        if new_line != line:
            lines[i] = new_line; changed = True
    if changed:
        nfile += 1
        if apply_: open(p, 'w', encoding='utf-8').write('\n'.join(lines))
print(f'{"applied" if apply_ else "would apply"}: {nedit} token edits in {nfile} files')
if rep: open(rep, 'w').write('\n'.join(out) + '\n')
print(f'unresolved/literal entries: {len(out)}')
