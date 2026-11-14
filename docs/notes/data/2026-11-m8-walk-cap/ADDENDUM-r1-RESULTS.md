# M8 walk cap -- ADDENDUM r1 RESULTS (against ADDENDUM-r1.md, committed before the legs)

Code commit `2c50d71ae58140f03e580e232466c5e1e3e1100b`. Linux x86-64, Ryzen 7 5800X3D, clang 22.1.8, MKL 2026.2,
`MKL_NUM_THREADS=1`. Apple/Accelerate and Windows UNOBSERVED. One chain, `addendum-r1.out`, 2026-11-14 08:23 -> 08:46 (EST).

| leg | criterion | reading | verdict |
|---|---|---|---|
| F1 | 100% of 3653 Release and Debug; HsSweep pins unmoved | Release 100% of 3653, Debug 100% of 3653 (`addendum-r1/suites/`); HsSweepCorpus passes at 3355 / 3346 | MET |
| I1 | 0 differences vs `legs/rebase-92e7c5d83` | walk / ssn / ipm each 27 cells x 75 columns, 0 differences, byte-exact (`addendum-r1/replay/`); corpus binary sha256 `1649309a14af97ce95d714b6414470f6979bf1151d69ce8737f9117d146318cb` | MET |
| I4 | I4_PASS: every row equals its walk-cap leg row; mover set vs DN-3 r5 = the 21 declared | 476 rows, 21 movers (better 0, worse 0, neutral 21), 0 missing/capped, 0 in-gate non-byte-equal residuals, 0 FAIL (`addendum-r1/train/train_check.txt`); lane binary sha256 `59874093a3ebbfdab300cf8bf079291f210372870e93c0b575314aacd1d5925b`, pin install `.hven/install-2c50d71ae581` (local clone branch `m8-walk-cap-legs-r1`, never pushed) | MET |

I4 ran as the declared co-run (4 workers, physical cores 2-5, MKL_NUM_THREADS=1); no row deviated, so no solo re-run
was needed. Wall-clock is not quoted. Every mover of every leg: none beyond the 21 declared neutral TRAIN rows, which
equal their walk-cap leg rows. The riders are behaviour-neutral on the corpus, as expected.
