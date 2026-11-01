# 2026-10-m7-close-corpus -- the M7 close walk-census baseline

`walk_baseline.csv`: the 57-cell walk census of the M7 close gate (successor r3), pin `d4b8e84b`, in the same file shape as `../2026-08-16-u0-corpus/walk_baseline.csv` (prose header, then the census run's own co-run protocol header and the schema-76 rows; `scripts/census_compare.py` reads the first 13 columns). The M8 census compares against this file; the U0 baseline is left untouched.

Declared event: the new baseline of record for the walk census. Derivation, the 26 declared movers against U0 and their attributions, and the wall-dependent rows are stated in the file's header and in `docs/notes/data/2026-10-m7-close-gate/wrappers-close-r3/README.md` and `gates-r3/`. Provenance: corpus binary stamp `0de4ae7f19ed`, MKL 2026.2.0 on Linux (AMD Ryzen 7 5800X3D, clang 22.1.8), generated 2026-10-31 (r3) and reproduced byte-for-byte except `wall_s` by the 2026-10-30 r2 run. Apple/Accelerate and Windows values: UNOBSERVED.
