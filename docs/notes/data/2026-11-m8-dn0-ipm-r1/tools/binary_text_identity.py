#!/usr/bin/env python3
"""M8 DN-0 IPM r1: compare the .text of the MEASURED and RECORD (CFI) lane binaries instruction by instruction.

The CFI libhven.a has a larger .eh_frame, which the GNU linker places after .text/.rodata, so .text and
.rodata keep their addresses and sizes while the RW segment (.data.rel.ro, .got, .data, .bss) moves by a
constant SHIFT. Instructions that address the RW segment RIP-relatively then carry displacements that
differ by exactly SHIFT. This script disassembles both .text sections (objdump -d --no-show-raw-insn) and
requires: same instruction count, same address and mnemonic on every line, and on each differing line
every differing hex token differs by exactly SHIFT (or the symbolic suffix names the same symbol).

Usage: binary_text_identity.py MEASURED RECORD
"""
import re
import subprocess
import sys


def sec(b, name):
    for line in subprocess.run(["readelf", "-SW", b], capture_output=True, text=True, check=True).stdout.splitlines():
        f = line.replace("[ ", "[").split()
        if len(f) > 5 and f[1] == name:
            return int(f[3], 16), int(f[5], 16)
    raise SystemExit(f"no {name} in {b}")


def dis(b):
    p = subprocess.Popen(["objdump", "-d", "--no-show-raw-insn", "-j", ".text", b], stdout=subprocess.PIPE,
                         text=True, bufsize=1 << 20)
    for line in p.stdout:
        if re.match(r"^\s+[0-9a-f]+:\t", line):
            yield line.rstrip("\n")
    p.wait()


def main():
    a, b = sys.argv[1:3]
    ta, tb = sec(a, ".text"), sec(b, ".text")
    ra, rb = sec(a, ".rodata"), sec(b, ".rodata")
    da, db = sec(a, ".data"), sec(b, ".data")
    shift = db[0] - da[0]
    print(f".text   measured {ta[0]:#x}+{ta[1]:#x}  record {tb[0]:#x}+{tb[1]:#x}  same: {ta == tb}")
    print(f".rodata measured {ra[0]:#x}+{ra[1]:#x}  record {rb[0]:#x}+{rb[1]:#x}  same: {ra == rb}")
    print(f".data   shift {shift:#x} (page multiple: {shift % 4096 == 0})")
    n = same = shifted = bad = 0
    examples = []
    hexre = re.compile(r"0x[0-9a-f]+|\b[0-9a-f]{6,}\b")
    import itertools
    for la, lb in itertools.zip_longest(dis(a), dis(b)):
        n += 1
        if la == lb:
            same += 1
            continue
        if la is None or lb is None:
            bad += 1
            continue
        ok = la.split("\t")[0] == lb.split("\t")[0]
        xa, xb = hexre.findall(la), hexre.findall(lb)
        ok = ok and len(xa) == len(xb) and hexre.sub("H", la) == hexre.sub("H", lb)
        if ok:
            for u, v in zip(xa, xb):
                if u != v and int(v, 16) - int(u, 16) != shift:
                    ok = False
        if ok:
            shifted += 1
        else:
            bad += 1
            if len(examples) < 10:
                examples.append((la, lb))
    print(f"instructions: {n}; identical text: {same}; differing only by RW-segment displacement {shift:#x}: {shifted}; other: {bad}")
    for e in examples:
        print("  A:", e[0]); print("  B:", e[1])
    ok = ta == tb and ra == rb and bad == 0
    print("BINARY_TEXT_IDENTITY_OK" if ok else "BINARY_TEXT_IDENTITY_FAILED")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
