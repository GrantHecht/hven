# (b) and (c) of engine_bytes_r2.sh. argv: <armB build dir> <riders build dir> <armB corpus> <riders corpus>
import hashlib, json, os, re, subprocess, sys
A, B, binA, binB = sys.argv[1:5]
rc = 0
def stamped(build):
    out = {}
    for e in json.load(open(os.path.join(build, 'compile_commands.json'))):
        m = re.search(r'-D(HVEN_\w+_GIT_DESCRIBE)=\\?"?([0-9a-z-]+)', e['command'])
        if m:
            out[os.path.relpath(e['file'])] = m.group(2)
    return out
sa, sb = stamped(A), stamped(B)
stampA, stampB = set(sa.values()), set(sb.values())
print('stamped TUs:', sorted(sa)); print('stamp A', stampA, 'stamp B', stampB)
if len(stampA) != 1 or len(stampB) != 1 or sorted(sa) != sorted(sb):
    print('STAMP_SET_UNEXPECTED'); sys.exit(1)
stampA, stampB = stampA.pop().encode(), stampB.pop().encode()
if len(stampA) != len(stampB):
    print('STAMP_LENGTHS_DIFFER'); sys.exit(1)
# (b) bench objects
stamped_objs = {os.path.basename(f) + '.o' for f in sa}
diff_objs = []
for root, _, files in os.walk(os.path.join(A, 'bench')):
    for f in files:
        if f.endswith('.o'):
            pa = os.path.join(root, f); pb = os.path.join(B, os.path.relpath(pa, A))
            if open(pa, 'rb').read() != open(pb, 'rb').read():
                diff_objs.append(os.path.relpath(pa, A))
print('bench objects differing:', diff_objs)
if any(os.path.basename(d) not in stamped_objs for d in diff_objs):
    print('UNSTAMPED_BENCH_OBJECT_DIFFERS'); rc = 1
# (c) corpus binary
def buildid_span(path):
    sec = subprocess.run(['readelf', '-S', '--wide', path], capture_output=True, text=True).stdout
    m = re.search(r'\.note\.gnu\.build-id\s+NOTE\s+[0-9a-f]+\s+([0-9a-f]+)\s+([0-9a-f]+)', sec)
    off = int(m.group(1), 16)
    return off + 16, off + 36          # 12-byte note header + "GNU\0", then the 20-byte descriptor
da, db = bytearray(open(binA, 'rb').read()), bytearray(open(binB, 'rb').read())
ia, ib = buildid_span(binA), buildid_span(binB)
if len(da) != len(db) or ia != ib:
    print('LAYOUT_DIFFERS', len(da), len(db), ia, ib); sys.exit(1)
pos, start = [], 0
while (k := db.find(stampB, start)) != -1:
    pos.append(k); start = k + 1
posA, start = [], 0
while (k := da.find(stampA, start)) != -1:
    posA.append(k); start = k + 1
print('stamp offsets A', posA, 'B', pos)
if pos != posA or not pos:
    print('STAMP_OFFSETS_DIFFER'); sys.exit(1)
allowed = set(range(*ia))
for p in pos:
    allowed |= set(range(p, p + len(stampB)))
n_diff = 0; outside = 0
for i, (x, y) in enumerate(zip(da, db)):
    if x != y:
        n_diff += 1
        if i not in allowed:
            outside += 1
print('corpus bytes differing:', n_diff, 'outside stamp+build-id:', outside)
for p in pos:
    db[p:p + len(stampB)] = stampA
for d in (da, db):
    d[ia[0]:ia[1]] = bytes(20)
ha, hb = hashlib.sha256(da).hexdigest(), hashlib.sha256(db).hexdigest()
print('normalized sha256 A', ha); print('normalized sha256 B', hb)
if outside == 0 and ha == hb:
    print('CORPUS_IDENTICAL_BUT_STAMP')
else:
    print('CORPUS_DIFFERS'); rc = 1
sys.exit(rc)
