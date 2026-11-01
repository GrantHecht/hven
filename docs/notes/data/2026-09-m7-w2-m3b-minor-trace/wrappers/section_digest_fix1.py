#!/usr/bin/env python3
"""Print one line per ELF section, in SECTION-HEADER ORDER:

    <name>\t<sh_type>\t<sh_flags>\t<sh_size>\t<sh_link>\t<sh_info>\t<sh_addralign>\t<sh_entsize>\t<sha256 of the section's bytes>

WHY NOT `readelf -x`, WHICH WOULD ALSO WORK. A Debug object in this tree carries
about 8 500 sections (COMDAT groups, one per inline function), so a per-section
`readelf` would be ~17 000 process spawns for ONE object. This reads the section
header table once and hashes straight out of the file: same information, one
file read.

IT CARRIES THE HEADER FIELDS, NOT ONLY THE BYTES, and deliberately. A section
whose contents are unchanged but whose `sh_flags`, `sh_link`, `sh_info`,
`sh_entsize` or alignment moved IS a changed object, and a contents-only digest
would call it identical. `sh_offset` and `sh_addr` are NOT carried: an offset
shifts whenever anything earlier in the file changes size, which is exactly what
a debug-only change does, so carrying it would make every debug-only difference
look like a non-debug one.

A NOBITS section (`.bss`) has no file bytes; its digest is taken over the empty
string, which is the same on both sides, and its `sh_size` is carried, so a
changed `.bss` SIZE is still caught.

Exits non-zero, with nothing on stdout, on anything it cannot parse: a
truncated file, a non-ELF file, an unsupported class or a section header table
that points outside the file. The caller treats a non-zero exit as a REFUSAL,
never as agreement -- this script is one arm of a fail-closed check and it has
no "assume it is fine" path.
"""
import hashlib
import struct
import sys


def main(path):
    with open(path, "rb") as fh:
        data = fh.read()
    if len(data) < 64 or data[:4] != b"\x7fELF":
        sys.stderr.write("not an ELF file: {}\n".format(path))
        return 2
    ei_class, ei_data = data[4], data[5]
    if ei_class != 2:
        sys.stderr.write("only ELF64 is supported (EI_CLASS={})\n".format(ei_class))
        return 2
    endian = "<" if ei_data == 1 else ">"

    # Elf64_Ehdr: e_shoff at 0x28, e_shentsize at 0x3A, e_shnum at 0x3C,
    # e_shstrndx at 0x3E.
    e_shoff = struct.unpack_from(endian + "Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from(endian + "H", data, 0x3A)[0]
    e_shnum = struct.unpack_from(endian + "H", data, 0x3C)[0]
    e_shstrndx = struct.unpack_from(endian + "H", data, 0x3E)[0]
    if e_shentsize != 64:
        sys.stderr.write("unexpected e_shentsize {}\n".format(e_shentsize))
        return 2

    def shdr(i):
        off = e_shoff + i * e_shentsize
        if off + e_shentsize > len(data):
            raise ValueError("section header {} is past end of file".format(i))
        (sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info,
         sh_addralign, sh_entsize) = struct.unpack_from(endian + "IIQQQQIIQQ", data, off)
        return dict(name=sh_name, type=sh_type, flags=sh_flags, addr=sh_addr,
                    offset=sh_offset, size=sh_size, link=sh_link, info=sh_info,
                    addralign=sh_addralign, entsize=sh_entsize)

    # e_shnum == 0 means the real count is in section 0's sh_size (ELF's large
    # section-count escape, which every object in this tree actually uses).
    if e_shnum == 0:
        e_shnum = shdr(0)["size"]
    if e_shstrndx == 0xFFFF:
        e_shstrndx = shdr(0)["link"]

    strtab = shdr(e_shstrndx)
    if strtab["offset"] + strtab["size"] > len(data):
        sys.stderr.write("section name table is past end of file\n")
        return 2
    names = data[strtab["offset"]:strtab["offset"] + strtab["size"]]

    SHT_NOBITS = 8
    out = []
    for i in range(e_shnum):
        h = shdr(i)
        end = names.find(b"\x00", h["name"])
        name = names[h["name"]:end if end >= 0 else None].decode("utf-8", "replace")
        if h["type"] == SHT_NOBITS:
            body = b""
        else:
            if h["offset"] + h["size"] > len(data):
                sys.stderr.write("section {} ({}) is past end of file\n".format(i, name))
                return 2
            body = data[h["offset"]:h["offset"] + h["size"]]
        out.append("{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}\t{}".format(
            name, h["type"], h["flags"], h["size"], h["link"], h["info"],
            h["addralign"], h["entsize"], hashlib.sha256(body).hexdigest()))
    sys.stdout.write("\n".join(out) + "\n")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.stderr.write("usage: section_digest_fix1.py <object>\n")
        sys.exit(2)
    try:
        sys.exit(main(sys.argv[1]))
    except Exception as exc:  # fail closed: no output, non-zero exit
        sys.stderr.write("REFUSED: {}\n".format(exc))
        sys.exit(2)
