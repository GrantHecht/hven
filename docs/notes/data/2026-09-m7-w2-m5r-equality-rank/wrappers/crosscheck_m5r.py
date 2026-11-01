#!/usr/bin/env python3
"""M7 W2 M5r -- the INDEPENDENT cross-check: numpy SVD ranks on the SAME matrices
the probe factored (dumped by the probe as (row, col, val) triplets at %.17g),
compared with the probe's QR ranks read from its committed row file.

Pairing, declared:
  QR, Eigen default threshold   <->  numpy matrix_rank default tol
                                      (S.max() * max(M, N) * eps)
  QR, 1e-10 relative            <->  SVD, tol = 1e-10 * S.max()
  QR, 1e-8  relative (arm)      <->  SVD, tol = 1e-8  * S.max()
each on the RAW rows and on the ROW-EQUILIBRATED rows (every row to unit norm).
A QR R-diagonal cut and an SVD cut are different statistics; where they give a
different count the row says DISAGREE and prints the singular values either
side of both counts so the reader can see why. Nothing is reconciled by hand.

Run with an interpreter that has numpy (the box's system python does not; the
chain names the one it used). No solver runs.
"""
import sys

import numpy as np


def load(path):
    rows, cols, vals, shape = [], [], [], None
    for line in open(path):
        if line.startswith("row,"):
            continue
        if line.startswith("# shape,"):
            _, m, n = line.strip().split(",")
            shape = (int(m), int(n))
            continue
        r, c, v = line.strip().split(",")
        rows.append(int(r))
        cols.append(int(c))
        vals.append(float(v))
    A = np.zeros(shape)
    for r, c, v in zip(rows, cols, vals):
        A[r, c] += v
    return A


def row_block(path, point):
    out, inside = {}, False
    for line in open(path):
        line = line.rstrip("\n")
        if line == f"POINT_BEGIN {point}":
            inside = True
            continue
        if line == f"POINT_END {point}":
            break
        if inside and "=" in line and " " not in line.split("=")[0]:
            k, v = line.split("=", 1)
            out.setdefault(k, v)
    return out


def ranks(A):
    s = np.linalg.svd(A, compute_uv=False)
    smax = s[0] if s.size else 0.0
    eps = np.finfo(float).eps
    return s, {
        "dflt": int(np.sum(s > smax * max(A.shape) * eps)),
        "alt": int(np.sum(s > 1e-10 * smax)),
        "arm": int(np.sum(s > 1e-8 * smax)),
    }


def main():
    rowfile, dumpdir, cell = sys.argv[1], sys.argv[2], sys.argv[3]
    print(f"numpy {np.__version__}  cell {cell}")
    bad = 0
    for point in ("x0", "xproj"):
        blk = row_block(rowfile, point)
        if not blk:
            continue
        for tag in ("eq", "skel", "entry"):
            try:
                A = load(f"{dumpdir}/{point}-{tag}.csv")
            except FileNotFoundError:
                continue
            if A.size == 0:
                continue
            Az = A.copy()
            nr = np.linalg.norm(Az, axis=1)
            Az[nr > 0] /= nr[nr > 0, None]
            for variant, M, pre in (("raw", A, tag), ("eqz", Az, tag + "_eqz")):
                s, r = ranks(M)
                for th in ("dflt", "alt", "arm"):
                    key = f"{pre}_{th}_rank"
                    if key not in blk:
                        continue
                    qr = int(blk[key])
                    ok = qr == r[th]
                    bad += 0 if ok else 1
                    lo, hi = min(qr, r[th]), max(qr, r[th])
                    sv = " ".join(f"s{i+1}/s1={s[i]/s[0]:.3e}"
                                  for i in range(max(lo - 1, 0), min(hi + 1, s.size)))
                    print(f"  {point:5s} {tag:5s} {variant} thr={th:4s} rows={M.shape[0]:5d} "
                          f"QR_rank={qr:5d} SVD_rank={r[th]:5d} "
                          f"{'AGREE' if ok else 'DISAGREE'}  [{sv}]")
    print(f"DISAGREEMENTS {bad}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
