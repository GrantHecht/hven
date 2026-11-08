#!/usr/bin/env python3
"""M8 DN-0 IPM r1: are the KKT matrices F1 leaves behind the ones DN-0 dumped?

For every roster cell, compares the r1 dump directory with DN-0's (hven-bench-sd/.scratch/dn0/run/dump):
  * the mat_NNNN.bin file set and every file's bytes (header incl. inertia and input iparm, CSR, values),
  * index.tsv row by row on every column except the session handle `pt` (a process address),
and, per cell, status / iterations / objective of the two dump-build processes.
Usage: dump_identity.py R1_RUN DN0_RUN ROSTER  -> table on stdout, last line DUMP_IDENTITY n_same/n
"""
import csv
import glob
import os
import sys


def fields(path):
    d = {}
    if os.path.exists(path):
        for line in open(path, errors="replace"):
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                d.setdefault(k, v)
    return tuple(d.get(k) for k in ("status", "iterations", "objective"))


def index_rows(path):
    if not os.path.exists(path):
        return None
    return [r[:3] + r[4:] for r in (l.rstrip("\n").split("\t") for l in open(path))]


def main():
    r1, dn0, roster = sys.argv[1:4]
    n = same = 0
    mats_total = 0
    print("cell\tmats_r1\tmats_dn0\tbytes_identical\tindex_identical_excl_pt\toutcome_identical")
    for line in open(roster):
        pop, lane, prob = line.rstrip("\n").split("\t")
        tag = f"{pop}-{lane}-{prob}"
        a, b = os.path.join(r1, "dump", tag + ".dump"), os.path.join(dn0, "dump", tag + ".dump")
        ma = sorted(os.path.basename(p) for p in glob.glob(os.path.join(a, "mat_*.bin")))
        mb = sorted(os.path.basename(p) for p in glob.glob(os.path.join(b, "mat_*.bin")))
        byt = ma == mb and all(open(os.path.join(a, m), "rb").read() == open(os.path.join(b, m), "rb").read() for m in ma)
        idx = index_rows(os.path.join(a, "index.tsv")) == index_rows(os.path.join(b, "index.tsv"))
        out = fields(os.path.join(r1, "dump", tag + ".out")) == fields(os.path.join(dn0, "dump", tag + ".out"))
        ok = byt and idx and out
        n += 1
        same += ok
        mats_total += len(ma)
        print(f"{tag}\t{len(ma)}\t{len(mb)}\t{int(byt)}\t{int(idx)}\t{int(out)}")
    print(f"matrices compared: {mats_total}")
    print(f"DUMP_IDENTITY {same}/{n}")


if __name__ == "__main__":
    main()
