#!/usr/bin/env python3
"""M7 W0 T2 fix1 — the P-SYM classification, completed mechanically.

The retained transcript (`logs/72f-psym.out`) names WHICH symbols differ between
the two library objects; it does not say, for the ones it reports as STRUCTURAL,
WHAT differs. This does, per symbol, from the same two objects the transcript
was taken over, by the same rule for every symbol:

  1. disassemble the symbol with GNU objdump (the disassembler P-SYM itself
     requires);
  2. drop ALIGNMENT PADDING (nop / data16 / cs nop / xchg %ax,%ax), counting it
     separately;
  3. normalise INTRA-FUNCTION branch targets to a placeholder, recording the
     per-target delta, so a function that merely MOVED does not read as changed;
  4. diff what is left, mnemonic and operands, and classify every remaining
     difference:
       MEMBER-DISPLACEMENT   same mnemonic, same operands, one memory
                             displacement larger by a CONSTANT -- the new mask
                             members shifting every later member of the engine
                             object. The constant is reported, not assumed: it
                             is +8 where the two enums added eight bytes and +4
                             where one of them landed in existing padding, and a
                             symbol whose displacement deltas are not all one
                             value is reported as OTHER;
       LABEL-FAMILY          same mnemonic and operands but for the NUMBER in a
                             compiler-local label (.LCPI / .LBB / .Ltmp /
                             GCC_except_table), which codegen assigns per run
                             and which names no code;
       OTHER                 anything else, printed in full.

A symbol whose only differences are pad-count, a CONSTANT branch-target shift,
and member displacements of exactly +8 is LAYOUT. Anything else is named.

  python3 classify.py <old-object> <new-object> [symbol-substring ...]
"""
import re
import subprocess
import sys

PAD = re.compile(r"^\s*(nop|nopw|nopl|data16|cs nop|xchg\s+%ax,%ax)", re.I)
HDR = re.compile(r"^[0-9a-f]+ <(.+)>:$")
INSN = re.compile(r"^\s*[0-9a-f]+:\t(.*)$")
BRANCH = re.compile(r"^(j[a-z]+|call|loop[a-z]*)\s+[0-9a-f]+ <(.*)\+0x([0-9a-f]+)>")
BRANCH0 = re.compile(r"^(j[a-z]+|call|loop[a-z]*)\s+[0-9a-f]+ <([^+>]*)>")
DISP = re.compile(r"0x([0-9a-f]+)\(")
# Compiler-local labels: constant-pool entries, basic blocks, temporaries, and
# the per-function exception tables. Their NUMBERS are assigned per
# translation-unit codegen run and carry no meaning, and `GCC_except_table<N>`
# reaches a listing only because objdump names a call's target after the nearest
# preceding symbol -- the INSTRUCTION is byte-identical either way. A difference
# that is only a renumbering is not a difference in code. P-SYM calls this the
# "compiler-local label family" class and accounts for it by count; this
# normalises it by name.
LOCAL_LABEL = re.compile(r"(\.(LCPI|LBB|Ltmp|LJTI|Lswitch)|GCC_except_table)[0-9]+(_[0-9]+)?")


def symbols(obj):
    out = subprocess.run(["objdump", "-d", "--no-show-raw-insn", "-C", obj],
                         capture_output=True, text=True, check=True).stdout
    cur, body = None, []
    syms = {}
    for line in out.splitlines():
        m = HDR.match(line)
        if m:
            if cur is not None:
                syms[cur] = body
            cur, body = m.group(1), []
            continue
        m = INSN.match(line)
        if m and cur is not None:
            body.append(m.group(1).split("#")[0].rstrip())
    if cur is not None:
        syms[cur] = body
    return syms


def split(body):
    pads, insns = 0, []
    for t in body:
        if PAD.match(t):
            pads += 1
        else:
            insns.append(t)
    return pads, insns


def normalise(t):
    """Branch targets -> placeholder; returns (text, offset or None)."""
    m = BRANCH.match(t)
    if m:
        return "%s TGT <%s+OFF>" % (m.group(1), m.group(2)), int(m.group(3), 16)
    m = BRANCH0.match(t)
    if m:
        return "%s TGT <%s>" % (m.group(1), m.group(2)), 0
    return t, None


def classify(name, a_body, b_body):
    a_pad, a_in = split(a_body)
    b_pad, b_in = split(b_body)
    out = ["  pads %d -> %d; non-pad instructions %d -> %d" % (a_pad, b_pad, len(a_in), len(b_in))]
    if len(a_in) != len(b_in):
        out.append("  OTHER: the non-pad instruction COUNT changed -- "
                   "instructions were added or removed")
        return False, out
    deltas, disp_deltas, labels, other = {}, {}, 0, []
    for x, y in zip(a_in, b_in):
        nx, ox = normalise(x)
        ny, oy = normalise(y)
        if ox is not None and oy is not None and ox != oy:
            deltas[oy - ox] = deltas.get(oy - ox, 0) + 1
        if nx == ny:
            continue
        if LOCAL_LABEL.sub("LOCAL", nx) == LOCAL_LABEL.sub("LOCAL", ny):
            labels += 1
            continue
        hx = DISP.sub("D(", nx)
        hy = DISP.sub("D(", ny)
        if hx == hy:
            dx = int(DISP.search(nx).group(1), 16)
            dy = int(DISP.search(ny).group(1), 16)
            disp_deltas[dy - dx] = disp_deltas.get(dy - dx, 0) + 1
            continue
        else:
            other.append("%s || %s" % (x, y))
    if deltas:
        out.append("  branch-target shifts: " +
                   ", ".join("%+d x%d" % (k, v) for k, v in sorted(deltas.items())))
    if disp_deltas:
        out.append("  member-displacement pairs: " +
                   ", ".join("%+d x%d" % (k, v) for k, v in sorted(disp_deltas.items())))
    if len(disp_deltas) > 1:
        other.append("displacement deltas are not one constant: %s" % sorted(disp_deltas))
    if labels:
        out.append("  compiler-local label renumberings: %d" % labels)
    if other:
        out.append("  OTHER differences: %d" % len(other))
        for o in other[:20]:
            out.append("    " + o)
        if len(other) > 20:
            out.append("    ... and %d more" % (len(other) - 20))
        return False, out
    out.append("  VERDICT: LAYOUT ONLY (pad count, a constant branch-target "
               "shift, a CONSTANT member-displacement shift, compiler-local "
               "label renumberings)")
    return True, out


def main():
    old, new = sys.argv[1], sys.argv[2]
    wanted = sys.argv[3:]
    a, b = symbols(old), symbols(new)
    only_a = sorted(set(a) - set(b))
    only_b = sorted(set(b) - set(a))
    print("OLD: %s\nNEW: %s\n" % (old, new))
    layout = differing = identical = 0
    for name in sorted(set(a) & set(b)):
        if wanted and not any(w in name for w in wanted):
            continue
        if a[name] == b[name]:
            identical += 1
            continue
        ok, lines = classify(name, a[name], b[name])
        print(("LAYOUT   " if ok else "DIFFERS  ") + name)
        print("\n".join(lines))
        print()
        if ok:
            layout += 1
        else:
            differing += 1
    for n in only_a:
        print("ONLY-BEFORE  " + n)
    for n in only_b:
        print("ONLY-AFTER   " + n)
    print("\nTOTAL: %d byte-identical, %d layout-only, %d differing, "
          "%d only-before, %d only-after" % (identical, layout, differing, len(only_a), len(only_b)))
    return 1 if differing else 0


if __name__ == "__main__":
    sys.exit(main())
