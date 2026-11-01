# Check the re-derived golden literals are exactly the declared additive trailing keys.
import re
import sys

old, new = open(sys.argv[1]).read(), open(sys.argv[2]).read()


def lits(src, name):
    t = src.index("TEST(JsonLinesTraceSink, " + name + ")")
    a = src.index("EXPECT_EQ(os.str(),", t)
    b = src.index(");", a)
    return "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', src[a:b]))


n = "GoldenLineSqpSolveEndWritesTheAbsenceSentinelsAsNull"
o, w = lits(old, n), lits(new, n)
anchor = '\\"restoration_curvature_switches\\":0,'
ins = '\\"ws_algebra_switches\\":0,\\"ws_algebra_switched_qps\\":0,'
print("absence: new == old + the two keys:", w == o.replace(anchor, anchor + ins))

n = "GoldenLineSqpSolveEndWithEveryCounterDistinct"
o, w = lits(old, n), lits(new, n)


def shift(m):
    v = int(m.group(2))
    return m.group(1) + str(v + 2 if v > 83 else v)


o2 = re.sub(r'(\\":)(\d+)(?=[,}])', shift, o)
anchor = '\\"restoration_curvature_switches\\":83,'
o2 = o2.replace(anchor, anchor + '\\"ws_algebra_switches\\":84,\\"ws_algebra_switched_qps\\":85,')
print("distinct: new == old shifted by 2 + the two keys:", w == o2)
import difflib
for d in difflib.ndiff(o2.split(","), w.split(",")):
    if d[0] in "+-": print(d)
