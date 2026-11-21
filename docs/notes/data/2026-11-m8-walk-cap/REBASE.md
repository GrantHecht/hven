# M8 walk cap -- REBASE onto origin/m8 92e7c5d83 (AP-5 G1 callback / stop-token work)

The three walk-cap commits (code, DECLARATION, RESULTS) were rebased from 030bae675 onto 92e7c5d83 with no textual
conflict (code 5a318de55, DECLARATION 5976d8e2d, RESULTS 6ab22ccd7). Nothing on origin was rewritten. The
DECLARATION and the leg files are untouched; this file and `legs/rebase-92e7c5d83/` are new.

Linux x86-64, clang 22.1.8, MKL 2026.2, SAFER_FAST, `MKL_NUM_THREADS=1`; Apple/Accelerate and Windows UNOBSERVED.

## F1 -- full suite on the rebased head

| build | result |
|---|---|
| Release | 100% tests passed, 0 failed out of 3652 (`ctest-rel.log`) |
| Debug | 100% tests passed, 0 failed out of 3652 (`ctest-dbg.log`) |

3652 = G1's 3650 + the two walk-cap seam fixtures. `ctest -j4`, under setsid, GIT_CEILING_DIRECTORIES set.

HsSweepCorpus.Corpus: G1's pins on this base are Release 3347 / Debug 3338 (test_hs_sweeps.cpp at 92e7c5d83
carries the DN-3 values; G1 changed only the macOS CONFIG-SPLIT preprocessor guards there). The walk-cap pins
3355 / 3346 (+8 each, HS15 cold, as declared) pass unmodified: measured total = G1 pin + 8 exactly in both builds.
No re-pin was needed.

## I1 -- replay identity

Rebased Release corpus binary sha256 `0e8168c8b443456325ef8c0cafd910ae32eb6809be81c19f4cbe0463ada8c50f`, 27 cells x
{walk, ssn, ipm}, one run, serial, compared byte-exact (`compare_replay.py`, no residual gate needed) against the
walk-cap leg rows `replay/k1-<mode>.csv`:

| mode | cells | columns | differences |
|---|---|---|---|
| walk | 27 | 75 | 0 |
| ssn | 27 | 75 | 0 |
| ipm | 27 | 75 | 0 |

G1 is behaviour-neutral on the replay when no stop is requested, as expected.
Raw: `legs/rebase-92e7c5d83/{walk,ssn,ipm}.csv`, `comparison-*.txt`, `replay.out`.
