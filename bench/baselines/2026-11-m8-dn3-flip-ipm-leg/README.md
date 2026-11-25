# 2026-11-m8-dn3-flip-ipm-leg -- the interior leg's baseline at the DN-3 flip

`interior_baseline.csv`: the top-level interior-point leg (`hven_sqp_corpus --engine interior`, the B7 baseline's own
`--cells` invocation; 43 rows, schema 31) at the shipped default after the M8 DN-3 flip,
`IpmLinearOptions::dense_factorization = kAuto`. A byte copy of
`docs/notes/data/2026-11-m8-dn3/r5/interior/interior-default-baseline.csv`.

Declared event (`docs/notes/data/2026-11-m8-dn3/DECLARATION-r5.md` section 1, CLAUDE.md section 7): the flip moves
the leg's three small fixtures, whose KKT orders are below the IPM's kAuto threshold (167), and nothing else. Against
`../2026-11-m8-b7-ipm-leg/interior_baseline.csv` this file differs in residual columns of `hs071_x1_fixed`,
`infeas2_spike` and `infeas2_stationary` only (r5's I3c-A2 on the 27-cell invocation: 26 differences, all on those
cells; statuses and iteration counts equal); every `f7_*` row is byte-identical. r5's I3c-A3 reproduced it byte for
byte against r4's independent run of the same invocation.

The B7 baseline stays on disk, unrewritten: it is the reference of the pre-flip pin `HVEN_LEG_IPM_PRE_DN3_DEFAULT`
(r5's I3c-N2, 0 differences) and is still the file `tests/sqp/CMakeLists.txt` points
`HVEN_SQP_INTERIOR_BASELINE_CSV` at (re-pointing that offline gate to this file is a follow-up, not done here).

Provenance: corpus binary stamp `297649961a3b` (sha256 `12cd2b9f...`), MKL 2026.2 on Linux (AMD Ryzen 7 5800X3D,
clang 22.1.8, Release, SAFER_FAST, `MKL_NUM_THREADS=1`), generated 2026-11-13. Apple/Accelerate and Windows values:
UNOBSERVED.
