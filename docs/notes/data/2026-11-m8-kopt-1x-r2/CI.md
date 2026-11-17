# kopt-1x r2 -- the Intel branch-14 acceptance reading (CI)

Branch `kopt-1x-r2` (NOT m8), code commit `b64fd4cfd9b5fd90ea29d199e1e62a1f37ebfaba`, workflow `ci.yml` dispatched once
(run 44099878541); the linux job was re-run until it drew an Intel host (a re-run of the job, up to 4 allowed).

| attempt | linux job | CPU | `mkl_auto_branch` | result |
|---|---|---|---|---|
| 1 | 134096721082 | AMD EPYC 7763 | 2 | success (reference route, no YORKNET record) |
| 2 | 134127891929 | AMD EPYC 9V74 | 2 | success (reference route, no YORKNET record) |
| 3 | 134153316219 | INTEL(R) XEON(R) PLATINUM 8573C | **14** | success, 3847 CTest entries passed |

(macOS and Windows jobs of the run: success.)

**YORKNET qp5 on Intel branch 14** (`intel-branch14-yorknet-records.txt`, the gtest XML properties of attempt 3; both algebras):

- status 0 (kOptimal), 1189 minors, 2 rank repairs, 0 dependent-blocker terminations, 0 certificate rejections, 0 stagnation drops/stops;
- `optimal_row_gray_exits` = **0**; worst equality ratio **7.401e-08** (row 81, residual 1.11e-16, row scale 1.5, lambda 0.0786505;
  eq 9.506e-15). `QpRdvRankRepair.YorknetQp5Border/Refactorize` assert ratio <= 1 again and pass.

Before (CI runs 43501262925 / 43655190606 and the first round's reading): 1176-1177 minors, 2 repairs, row 246 at 6.879x (residual 2.082e-7,
row scale 30.26, lambda 0.198474), one gray exit counted. The route on the Intel host is not the same one (1189 minors, a different final
row), so this reading says the contract held on Intel branch 14 at this commit, not that the old 6.88x point was repaired in place; the old
point is reproduced and repaired locally (`dev/red-base-reproducers.txt`, the certificate-seam test).
