# Re-derive the two sqp.solve.end golden literals from a gtest failure log (declared additive keys).
import re
import sys

log, src = sys.argv[1], sys.argv[2]
text = open(log).read()
actual = re.findall(r'os\.str\(\)\n    Which is: "(.*)"\n', text)
assert len(actual) == 2, len(actual)
s = open(src).read()


def chunks(esc, width=80):
    out, cur = [], ''
    i = 0
    while i < len(esc):
        tok = esc[i:i + 2] if esc[i] == '\\' else esc[i]
        if len(cur) + len(tok) > width:
            out.append(cur)
            cur = ''
        cur += tok
        i += len(tok)
    if cur:
        out.append(cur)
    return out


for name, lit in zip(["GoldenLineSqpSolveEndWithEveryCounterDistinct",
                      "GoldenLineSqpSolveEndWritesTheAbsenceSentinelsAsNull"], actual):
    t = s.index("TEST(JsonLinesTraceSink, " + name + ")")
    a = s.index("EXPECT_EQ(os.str(),", t)
    b = s.index(");", a)
    body = "EXPECT_EQ(os.str(),\n" + "\n".join('              "' + c + '"' for c in chunks(lit))
    s = s[:a] + body + s[b:]
open(src, 'w').write(s)
print("rewrote 2 literals")
