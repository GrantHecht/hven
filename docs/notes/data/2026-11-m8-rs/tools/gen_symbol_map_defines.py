#!/usr/bin/env python3
"""Derive the P-SYM --symbol-map entries that a changed path-valued compile definition causes.

A test target defines directory paths as string literals (-DHVEN_SRC_ROOT="<root>/tests/drivers/../../src").  A test that builds a std::filesystem::path
from such a literal instantiates `std::filesystem::path::path<char [N], path>(char const (&) [N], path::format)`, named by the literal's size N (length + NUL).
Moving the directory changes the literal, hence N, hence the NAME of that instantiation (and the relocation target of every caller).  This script reads the
compile definitions of every translation unit in the BEFORE and AFTER compile_commands.json (keys: mapped source, CMake target), and for every definition whose
value differs and that names a path (contains '/'), it writes `old-symbol => new-symbol` for the path<char [N]> instantiation.  The value difference is only
accepted if it is explained by the move map or the --dir-move: the AFTER value, with every `@/old/prefix` of the before value pushed through the maps and `..`
left as spelled, must equal the actual after value after both are normalised -- so a define that changed for any OTHER reason fails the script.
usage: gen_symbol_map_defines.py BEFORE.json AFTER.json --root WT --map MAP [--dir-move OLD=NEW ...] -o OUT"""
import argparse, json, re, shlex, posixpath
ap = argparse.ArgumentParser()
ap.add_argument('before'); ap.add_argument('after'); ap.add_argument('--root', required=True)
ap.add_argument('--map', action='append', default=[]); ap.add_argument('--dir-move', action='append', default=[])
ap.add_argument('-o', '--out', required=True)
a = ap.parse_args()
root = a.root.rstrip('/')
moves = {}
for mf in a.map:
    for l in open(mf):
        l = l.rstrip('\n')
        if ' -> ' in l and not l.split(' -> ', 1)[1].startswith('DELETE'):
            o, n = l.split(' -> ', 1); moves[o] = n
dirmoves = [tuple(d.rstrip('/') for d in dm.split('=', 1)) for dm in a.dir_move]

def defs(path, is_before):
    out = {}
    for e in json.load(open(path)):
        toks = shlex.split(e.get('command') or ' '.join(e['arguments']))
        f = e['file']; rel = f[len(root) + 1:] if f.startswith(root + '/') else f
        obj = ''
        for i, t in enumerate(toks):
            if t == '-o': obj = toks[i + 1]
        m = re.search(r'CMakeFiles/([^/]+)\.dir', obj)
        key = (moves.get(rel, rel) if is_before else rel, m.group(1) if m else obj)
        d = {}
        for t in toks:
            m2 = re.match(r'-D([A-Za-z_][A-Za-z0-9_]*)=(.*)$', t)
            if m2 and '/' in m2.group(2): d[m2.group(1)] = m2.group(2)
        out[key] = d
    return out

def norm(v):
    return re.sub(r'(/[\w./+-]+)', lambda m: posixpath.normpath(m.group(1)), v)

def push(v):
    # push every absolute path inside the value through file moves and directory moves
    def one(m):
        p = m.group(1)
        if not p.startswith(root + '/'): return p
        rel = posixpath.normpath(p[len(root) + 1:])
        if rel in moves: return root + '/' + moves[rel]
        for od, nd in dirmoves:
            if rel == od or rel.startswith(od + '/'): return root + '/' + nd + rel[len(od):]
        return p
    return re.sub(r'(/[\w./+-]+)', one, v)

b, af = defs(a.before, True), defs(a.after, False)
entries, errors, seen = {}, [], {}
for k in sorted(set(b) & set(af)):
    for name in sorted(set(b[k]) | set(af[k])):
        vb, va = b[k].get(name), af[k].get(name)
        if vb == va: continue
        if vb is None or va is None:
            errors.append('%s %s: define present in one arm only' % (k, name)); continue
        if norm(push(vb)) != norm(va):
            errors.append('%s %s: before %r pushed through the maps is %r, after is %r' % (k, name, vb, norm(push(vb)), norm(va))); continue
        nb, na = len(vb.strip('"')) + 1, len(va.strip('"')) + 1     # the literal as written: no quotes, plus the NUL
        if nb == na: continue
        fmt = 'std::filesystem::__cxx11::path::path<char [%d], std::filesystem::__cxx11::path>(char const (&) [%d], std::filesystem::__cxx11::path::format)'
        ob, nw = fmt % (nb, nb), fmt % (na, na)
        if entries.get(ob, nw) != nw: errors.append('conflicting rename for %s' % ob)
        entries[ob] = nw
        seen.setdefault((k[1], name, nb, na), 0); seen[(k[1], name, nb, na)] += 1
for (t, nme, nb, na), c in sorted(seen.items()): print('%-24s %-24s literal size %d -> %d  (%d TUs)' % (t, nme, nb, na, c))
if errors:
    print('ERRORS'); [print(' ', e) for e in errors]; raise SystemExit(1)
with open(a.out, 'w') as f:
    f.write('# derived by tools/gen_symbol_map_defines.py: path<char [N]> instantiations renamed because a path-valued -D define moved with its directory\n')
    for o, n in sorted(entries.items()): f.write('%s => %s\n' % (o, n))
print('%d symbol-map entries' % len(entries))
