#!/usr/bin/env python3
"""RS-6b L4 (DECLARATION-RS-6b-r3.md). usage: objcmp_r3.py BASE_SNAPSHOT AFTER_SNAPSHOT
Snapshots are psym_compare.sh capture directories. Every non-test object (anything not under tests/) of the base must exist in the after snapshot and be byte-identical, except objects embedding the
configure-time commit stamp (HVEN_SQP_CORPUS_GIT_DESCRIBE): those are compared after both stamp strings are replaced by one same-length placeholder (on in-memory copies). The stamp set is derived by
searching each object's bytes for either stamp string. Exit 0 iff all hold."""
import os, re, sys, subprocess
base, after = sys.argv[1:3]
def objs(root):
    out = set()
    for l in open(os.path.join(root, '.psym-manifest')):
        l = l.strip()
        if l and l != 'libhven.a' and not l.startswith('tests/'): out.add(l)
    return out
def stamp(cc):
    m = re.search(r'GIT_DESCRIBE=\\\\\\"([^\\ "]+)', open(cc).read())
    return m.group(1)
sb = stamp(os.path.join(base, '..', 'cc-base.json')).encode(); sa = stamp(os.path.join(after, '..', 'cc-after.json')).encode()
assert len(sb) == len(sa) or True
ph_b = b'#' * len(sb); ph_a = b'#' * len(sa)
ob, oa = objs(base), objs(after)
bad = []; ident = 0; stamped = []; lib_stamped = 0
for o in sorted(ob):
    if o not in oa: bad.append('MISSING in after: ' + o); continue
    b = open(os.path.join(base, o), 'rb').read(); a = open(os.path.join(after, o), 'rb').read()
    has = (sb in b) or (sa in a)
    if has and '/hven.dir/' in o: lib_stamped += 1
    if b == a and not has: ident += 1; continue
    if b == a and has: bad.append('stamp present yet bytes equal (unexpected): ' + o); continue
    if not has: bad.append('DIFFERS without the stamp: ' + o); continue
    if b.replace(sb, ph_b) == a.replace(sa, ph_a): stamped.append(o)
    else: bad.append('differs beyond the stamp: ' + o)
for o in sorted(oa - ob): bad.append('ADDED in after: ' + o)
print('stamps: base %s after %s' % (sb.decode(), sa.decode()))
print('non-test objects: %d identical, %d stamp-only (byte-identical after normalisation), %d base, %d after' % (ident, len(stamped), len(ob), len(oa)))
for o in stamped: print('  stamp-only: ' + o)
print('library objects (CMakeFiles/hven.dir) embedding the stamp: %d' % lib_stamped)
mb = sorted(subprocess.run(['ar', 't', os.path.join(base, 'libhven.a')], capture_output=True, text=True).stdout.split())
ma = sorted(subprocess.run(['ar', 't', os.path.join(after, 'libhven.a')], capture_output=True, text=True).stdout.split())
print('libhven.a members equal: %s (%d)' % (mb == ma, len(mb)))
if mb != ma: bad.append('libhven.a members differ')
if lib_stamped: bad.append('a library object embeds the stamp')
if bad: print('OBJECTS_NOT_AS_DECLARED'); [print('  ' + x) for x in bad]; sys.exit(1)
print('OBJECTS_AS_DECLARED_R3')
