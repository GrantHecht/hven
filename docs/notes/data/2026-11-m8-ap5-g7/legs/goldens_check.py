#!/usr/bin/env python3
"""M8 AP-5 G7 (../DECLARATION.md G): the two sqp.solve.end golden lines of tests/sqp/test_trace_writer.cpp, re-derived for
SqpCounters::payload_ignored. For each golden, the literal at <base> is transformed by the declared rule and must equal the literal
at <head> byte for byte:
  distinct  -- `"payload_ignored":95,` inserted after `"optimal_row_gray_exits":94,`; every integer of the nested ssn/ipqp
               objects up by one (they are filled after the direct fields);
  zero      -- `"payload_ignored":0,` inserted after `"optimal_row_gray_exits":0,`.
Every other line of the file outside these two literals and their RE-DERIVED comments is compared too: no other golden moved.
usage: goldens_check.py <base sha> <head sha> <out>"""
import re
import subprocess
import sys

base, head, out = sys.argv[1:4]
PATH = 'tests/sqp/test_trace_writer.cpp'


def show(sha):
    return subprocess.run(['git', 'show', '%s:%s' % (sha, PATH)], check=True, capture_output=True,
                          text=True).stdout


def literal(src, marker):
    i = src.index(marker)
    j = src.index('EXPECT_EQ(os.str(),', i)
    k = src.index(');', j)
    chunks = re.findall(r'"((?:[^"\\]|\\.)*)"', src[j + len('EXPECT_EQ(os.str(),'):k])
    return j, k, ''.join(chunks)


B, H = show(base), show(head)
lines = []
fails = 0
spans_b, spans_h = [], []
for marker, kind in [('RE-DERIVED AT M8 kopt-1x, the same route', 'distinct'),
                     ('RE-DERIVED AT M8 kopt-1x: `optimal_row_gray_exits`, 0.', 'zero')]:
    jb, kb, lb = literal(B, marker)
    jh, kh, lh = literal(H, marker)
    spans_b.append((jb, kb))
    spans_h.append((jh, kh))
    if kind == 'zero':
        key = '\\"optimal_row_gray_exits\\":0,'
        want = lb.replace(key, key + '\\"payload_ignored\\":0,', 1)
    else:
        key = '\\"optimal_row_gray_exits\\":94,'
        a = lb.index(key) + len(key)
        b = lb.index('}},\\"scaling_active')
        mid = re.sub(r'(?<=\\":)(\d+)', lambda m: str(int(m.group(1)) + 1), lb[a:b])
        want = lb[:a] + '\\"payload_ignored\\":95,' + mid + lb[b:]
    ok = want == lh
    fails += 0 if ok else 1
    lines.append('golden %-8s %s' % (kind, 'MATCH' if ok else 'MISMATCH'))


def outside(src, spans):
    parts, last = [], 0
    for j, k in spans:
        parts.append(src[last:j])
        last = k
    parts.append(src[last:])
    text = ''.join(parts)
    # the declared comment and count edits are allowed to differ
    drop = re.compile(r'RE-DERIVED AT M8 AP-5 G7|94 -> 95 at M8 AP-5 G7|kSqpCountersFieldCount, 9[45]u|^\s*//\s*$|'
                      r'^\s*// `optimal_row_gray_exits`; the nested tables')
    return [l for l in text.split('\n') if not drop.search(l)]


ob, oh = outside(B, spans_b), outside(H, spans_h)
rest_ok = ob == oh
fails += 0 if rest_ok else 1
lines.append('rest of the file (outside the two literals and the declared comment/count lines) %s' %
             ('UNCHANGED' if rest_ok else 'CHANGED'))
lines.append('pairs 2, failures %d' % fails)
open(out, 'w').write('\n'.join(lines) + '\n')
print('\n'.join(lines))
sys.exit(1 if fails else 0)
