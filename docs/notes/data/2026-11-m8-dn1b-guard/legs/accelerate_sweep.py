#!/usr/bin/env python3
# M8 DN-1b (../DECLARATION.txt): copied from ../../2026-11-m8-dn1-impl/r2/ and re-pointed (worktree dn1b, evidence this directory, arm A and arm B trees build-dn1be/, arm A 7784e4f59 rebuilt by legs/prebuild.sh); older provenance lines below are historical.
# M8 DN-1 r2 (../../DECLARATION-r2.txt): copied from ../../legs/ and re-pointed (evidence r2/, arm B trees build-dn1r2/; arm A read-only from build-dn1e/).
# M8 DN-1 impl: copied from R11 (2026-11-m8-r11-block-decode) and re-pointed: worktree dn1, build-dn1e, this directory, arm A 02065dac.
# M8 R11: copied from F1 / PF-1 (see legs/*.sh line 2), byte for byte below this line.
"""AP-2 B4 (the B3 instrument), settler C222 addition: the Accelerate syntax sweep.

For every .cpp under src/ tests/ bench/ examples/ that contains USE_ACCELERATE_SPARSE, take
its Linux compile command from build-dn1be/rel/compile_commands.json (no PCH), add
-fsyntax-only -DUSE_ACCELERATE_SPARSE -ferror-limit=0, and record every error. Each error is
classified: KNOWN-HEADER (the Accelerate framework header cannot be found on Linux),
KNOWN-SIZEOF (a static_assert on an options struct's sizeof, whose Linux layout is gated on
__linux__ and does not hold with the two Accelerate-only fields), or OTHER (a finding).

A file compiled through a unity build has no compile command of its own; it borrows the
command of a unity source of the same test directory (same target, same flags).

PASS B (beyond the settler's spec, reported separately): the same sweep with
-isystem scripts/accelerate_syntax_stub, the repo's own stub <Accelerate/Accelerate.h>
(scripts/check_accelerate_syntax_linux.sh), so a file whose real Accelerate include is fatal
on Linux is still parsed past it. A pass-B error naming an Accelerate API the stub does not
declare is the stub's limit, reported as STUB-GAP; anything else is OTHER.
usage: accelerate_sweep.py <out-dir>
"""
import json, os, re, shlex, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

# r2 (B5): the worktree root is derived from this script's own location rather than carried from
# the tree the instrument was copied from. B8: b8/legs/ is FIVE levels below the root, as b7-riders/legs/ was;
# the classifier accepts the options structs in hven:: (the namespace rename). Copied from B7 riders, where
# B7's b7/legs/ was (checked, not carried); the one change from that copy is build-b7e -> build-b8.
WT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../../..'))
out_dir = sys.argv[1]
os.makedirs(out_dir, exist_ok=True)
cc = {e['file']: e for e in json.load(open(WT + '/build-dn1be/rel/compile_commands.json'))}
files = []
for d in ['src', 'tests', 'bench', 'examples']:
    for root, _, fs in os.walk(os.path.join(WT, d)):
        for f in fs:
            if f.endswith('.cpp'):
                p = os.path.join(root, f)
                if 'USE_ACCELERATE_SPARSE' in open(p, errors='replace').read():
                    files.append(p)
files.sort()


unity = {}
for fname, e in cc.items():
    if '/Unity/unity_' in fname:
        tdir = fname.split('/build-dn1be/rel/')[1].split('/CMakeFiles/')[0]
        unity.setdefault(tdir, e)


def cmd_for(f, stub=False):
    e = cc.get(f)
    if e is None:
        tdir = os.path.dirname(os.path.relpath(f, WT))
        u = unity.get(tdir)
        if u is None:
            return None, None
        e = dict(u)
        e['command'] = u['command'].replace(u['file'], f)
    args = shlex.split(e['command'])
    out, i = [], 0
    while i < len(args):
        a = args[i]
        if a == '-Xclang' and i + 1 < len(args) and args[i + 1] == '-include-pch':
            i += 4
            continue
        if a == '-o':
            i += 2
            continue
        if a == '-c':
            i += 1
            continue
        out.append(a)
        i += 1
    if out and out[-1] == f:
        out = out[:-1]
    extra = ['-isystem', WT + '/scripts/accelerate_syntax_stub'] if stub else []
    return out + extra + ['-fsyntax-only', '-ferror-limit=0', '-fno-color-diagnostics',
                          '-DUSE_ACCELERATE_SPARSE', f], e['directory']


STUB = False


def run(f):
    cmd, d = cmd_for(f, STUB)
    if cmd is None:
        return f, None, ''
    r = subprocess.run(cmd, cwd=d, capture_output=True, text=True)
    return f, r.returncode, r.stderr


def sweep(name):
    rows = []
    with ThreadPoolExecutor(12) as ex:
        for f, rc, err in ex.map(run, files):
            rel = os.path.relpath(f, WT)
            if rc is None:
                rows.append((rel, 'NOT-IN-BUILD', ''))
                continue
            errs = [l for l in err.splitlines() if ' error: ' in l]
            if not errs:
                rows.append((rel, 'CLEAN', ''))
                continue
            for l in errs:
                if re.search(r"'Accelerate/Accelerate\.h' file not found|Accelerate.*file not found", l):
                    kind = 'KNOWN-HEADER'
                elif STUB and re.search(r"use of undeclared identifier '(Sparse|kSparse)|no (type|member) named '(Sparse|kSparse)|unknown type name 'Sparse", l):
                    kind = 'STUB-GAP'
                elif re.search(r'static assertion failed.*sizeof\(hven::(?:solvers::)?(Ipm|Sqp)\w*Options\)', l):
                    kind = 'KNOWN-SIZEOF'
                else:
                    kind = 'OTHER'
                rows.append((rel, kind, l.replace(WT + '/', '')))
    with open(os.path.join(out_dir, name), 'w') as fo:
        fo.write('# files containing USE_ACCELERATE_SPARSE: %d\n' % len(files))
        for r in rows:
            fo.write('%s\t%s\t%s\n' % r)
    other = [r for r in rows if r[1] == 'OTHER']
    print(name, 'files', len(files), 'clean', sum(r[1] == 'CLEAN' for r in rows),
          'known-header', sum(r[1] == 'KNOWN-HEADER' for r in rows),
          'known-sizeof', sum(r[1] == 'KNOWN-SIZEOF' for r in rows),
          'stub-gap', sum(r[1] == 'STUB-GAP' for r in rows), 'not-in-build', sum(r[1] == 'NOT-IN-BUILD' for r in rows), 'OTHER', len(other))
    for r in other:
        print('OTHER', r[0], r[2][:300])
    return other


other_a = sweep('accelerate-sweep.txt')
STUB = True
other_b = sweep('accelerate-sweep-stub.txt')
sys.exit(1 if (other_a or other_b) else 0)
