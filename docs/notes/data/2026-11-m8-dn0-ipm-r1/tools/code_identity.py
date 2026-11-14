#!/usr/bin/env python3
"""M8 DN-0 IPM r1: prove the CFI measurement build changes no code byte.

For each member object of two libhven.a archives (pin build vs CFI build), compare, by name:
  * the raw bytes of every executable section (SHF_EXECINSTR: .text, .text.*),
  * the relocation entries applied to those sections (.rela.text*), as symbol+type+offset+addend,
  * the raw bytes of every allocated non-executable section other than .eh_frame (.rodata*, .data*, ...),
and list the sections present in only one object. The expected difference is .eh_frame /
.rela.eh_frame only. Uses readelf/objcopy from binutils.

Usage: code_identity.py PIN_LIBHVEN.a CFI_LIBHVEN.a WORKDIR
Exit 0 iff every executable section and its relocations are identical in every member.
"""
import os
import re
import subprocess
import sys


def sh(*a):
    return subprocess.run(a, check=True, capture_output=True, text=True).stdout


def sections(obj):
    out = {}
    for line in sh("readelf", "-SW", obj).splitlines():
        m = re.match(r"\s*\[\s*(\d+)\]\s+(\S+)\s+(\S+)\s+\S+\s+\S+\s+(\S+)\s+\S+\s+(\S*)", line)
        if m and m.group(2) != "NULL":
            out[m.group(2)] = (m.group(3), int(m.group(4), 16), m.group(5))
    return out


def sec_bytes(obj, name, tmp):
    p = os.path.join(tmp, "sec.bin")
    subprocess.run(["objcopy", "-O", "binary", "--only-section=" + name, obj, p], check=True,
                   capture_output=True)
    return open(p, "rb").read()


def relocs(obj):
    cur, out = None, {}
    for line in sh("readelf", "-rW", obj).splitlines():
        m = re.match(r"Relocation section '(\S+)'", line)
        if m:
            cur = m.group(1)
            out[cur] = []
            continue
        if cur and re.match(r"^[0-9a-f]{8,}\s", line):
            f = line.split()
            out[cur].append(" ".join([f[0], f[2]] + f[4:]))
    return out


def extract(ar, d):
    os.makedirs(d, exist_ok=True)
    names = sh("ar", "t", ar).split()
    if len(names) != len(set(names)):
        sys.exit(f"{ar}: duplicate member names")
    subprocess.run(["ar", "x", os.path.abspath(ar)], cwd=d, check=True)
    return names


def main():
    a, b, work = sys.argv[1:4]
    na = extract(a, os.path.join(work, "pin"))
    nb = extract(b, os.path.join(work, "cfi"))
    print(f"members: pin {len(na)}, cfi {len(nb)}; same names: {sorted(na) == sorted(nb)}")
    bad = 0
    exec_secs = exec_bytes = 0
    diff_kinds = {}
    for n in sorted(na):
        oa, ob = os.path.join(work, "pin", n), os.path.join(work, "cfi", n)
        sa, sb = sections(oa), sections(ob)
        ra, rb = relocs(oa), relocs(ob)
        for s in sorted(set(sa) | set(sb)):
            if s not in sa or s not in sb:
                key = ("only-in-" + ("cfi" if s in sb else "pin"), re.sub(r"\.text\..*", ".text.*", s))
                diff_kinds[key] = diff_kinds.get(key, 0) + 1
                if "X" in (sa.get(s) or sb.get(s))[2]:
                    print(f"EXEC SECTION PRESENT IN ONE ONLY: {n} {s}")
                    bad += 1
                continue
            flags = sa[s][2]
            if s.startswith(".rela"):
                same = ra.get(s) == rb.get(s)
                tgt = s[len(".rela"):]
                if not same:
                    if tgt in sa and "X" in sa[tgt][2]:
                        print(f"TEXT RELOCATIONS DIFFER: {n} {s}")
                        bad += 1
                    else:
                        diff_kinds[("differs", s)] = diff_kinds.get(("differs", s), 0) + 1
                continue
            if "A" not in flags or sa[s][0] == "NOBITS":
                continue
            same = sec_bytes(oa, s, work) == sec_bytes(ob, s, work)
            if "X" in flags:
                exec_secs += 1
                exec_bytes += sa[s][1]
                if not same:
                    print(f"EXEC BYTES DIFFER: {n} {s}")
                    bad += 1
            elif not same:
                diff_kinds[("differs", s)] = diff_kinds.get(("differs", s), 0) + 1
    print(f"executable sections compared: {exec_secs} ({exec_bytes} bytes); differing: {bad}")
    print("non-executable differences (kind, section): count")
    for k, v in sorted(diff_kinds.items()):
        print(f"  {k[0]} {k[1]}: {v}")
    print("CODE_IDENTITY_OK" if bad == 0 else "CODE_IDENTITY_FAILED")
    sys.exit(0 if bad == 0 else 1)


if __name__ == "__main__":
    main()
