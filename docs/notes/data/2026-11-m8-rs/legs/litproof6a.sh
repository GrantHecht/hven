#!/usr/bin/env bash
# usage: WT=<worktree> flock /tmp/box-build.lock setsid -w bash litproof6a.sh      (needs .scratch/g6/arms from gate6a.sh's psym step)
# RS-6a (DECLARATION-RS6a-r5.md): the proof that the five test objects P-SYM reports as different differ ONLY by the length of a path literal a moved define carries. Each TU is recompiled with the AFTER arm's own command
# (cc-after.json) except that every declared path-valued -D token takes the value the BASE arm's command carried for the same source (cc-base.json); the result must be BYTE-IDENTICAL to the base arm's captured object.
set -o pipefail
source "$(dirname "$0")/env.sh"
box_check
E=$WT/docs/notes/data/2026-11-m8-rs; GD=$E/gates/RS-6a-r6; A=$WT/.scratch/g6/arms; OUT=$GD/litproof; mkdir -p "$OUT"
python3 - "$A/cc-base.json" "$A/cc-after.json" "$OUT" "$WT" "$@" <<'PY' > "$OUT/commands.txt"
import json, shlex, sys, re
cb, ca, out, wt = sys.argv[1:5]
decl = re.compile(r'^-D(HVEN_VERSION_STRING|HVEN_INCLUDE_ROOT|HVEN_SRC_ROOT|HVEN_DN1_FIXTURES|HVEN_GOLDEN_DIR|HVEN_TRACE_FIXTURE_DIR|HVEN_A4_GATE_CSV|HVEN_A4_GATE_MU1E2_CSV|HVEN_QP_E2D4_FIXTURES|HVEN_SQP_SOLVER_SOURCE|HVEN_SQP_[A-Z0-9_]*_CSV|HVEN_SQP_CORPUS_BINARY)=')
srcs = ['tests/trace/test_human_trace_goldens.cpp', 'tests/trace/test_trace_off_path.cpp', 'tests/trace/test_trace_v1_fixtures.cpp', 'tests/solver/test_message_sites.cpp', 'tests/bench/test_corpus_cells.cpp']
def load(p):
    d = {}
    for e in json.load(open(p)):
        rel = e['file'][len(wt) + 1:] if e['file'].startswith(wt + '/') else e['file']
        d[rel] = e
    return d
B, A = load(cb), load(ca)
for s in srcs:
    eb, ea = B[s], A[s]
    tb = shlex.split(eb.get('command') or ' '.join(eb['arguments'])); ta = shlex.split(ea.get('command') or ' '.join(ea['arguments']))
    bdef = {re.match(r'^-D(\w+)=', t).group(1): t for t in tb if decl.match(t)}
    new = []; i = 0
    while i < len(ta):
        t = ta[i]
        if t == '-o': new += ['-o', '%s/%s.o' % (out, s.replace('/', '_')) ]; i += 2; continue
        if decl.match(t):
            n = re.match(r'^-D(\w+)=', t).group(1)
            new.append(bdef.get(n, t)); i += 1; continue
        new.append(t); i += 1
    print('%s\t%s\t%s' % (s, ea['directory'], ' '.join(shlex.quote(x) for x in new)))
PY
rc=0
while IFS=$'\t' read -r src dir cmd; do
  ( cd "$dir" && eval "$cmd" ) > "$OUT/$(echo $src | tr / _).log" 2>&1 || { echo "COMPILE_FAIL $src"; rc=1; continue; }
  new="$OUT/$(echo $src | tr / _).o"
  # the base arm's captured object for the same source
  old=$(grep -E "/$(basename "$src").o$" "$A/psym-base/.psym-manifest" | grep -v "hven_sqp_corpus" | head -1)
  if [ -f "$A/psym-base/$old" ] && cmp -s "$new" "$A/psym-base/$old"; then echo "BYTE_IDENTICAL $src (base object $old)"; else echo "DIFFERENT $src (base object $old)"; rc=1; fi
done < "$OUT/commands.txt"
echo "litproof rc=$rc"
exit $rc
