# Shared constants for the M7 CLOSE GATE r3.  Sourced, never executed.  Everything of r1 (env.sh) is reused unchanged.
_h3="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$_h3/../wrappers-close/env.sh"
W3=$REC/wrappers-close-r3
F3=$F/r3                        # r3 scratch (the r1/r2 scratch under $F is never touched)
LOGS3=$F3/logs
R2REC=$REC/gates                # the r1+r2 record (frozen once committed): r2's measured census is the r3 prediction
R2_CENSUS=$R2REC/census/walk_census.csv
BIN=$F/build-release/bench/hven_sqp_corpus
# THE DECLARED BUILD IDENTITY (measured 2026-10-31 before the declaration commit; the pre-flight re-measures and refuses on any difference)
DECL_BIN_SHA256=d0e4164884c0e51d90ee556034ff744c0cb89adefd96541574f763aeb1aa4766
DECL_MANIFEST_RELEASE=be97fa8f8122136c246b54e657c3d928d154f57660d2cd3492ac5b14cdfb6705
DECL_MANIFEST_DEBUG=9aa45a1941280685024a37dc9ffd21e03ed5c671337d29a8f31df7ebd013a54c
DECL_MANIFEST_3SEAM=4559821e9dec826f1b6c39bc541c0f370e2faff62636c2f18dab2390ada05e15
# manifest = sha256 of `find <dir> -type f -printf '%p %s %T@\n' | LC_ALL=C sort`, run from $F
