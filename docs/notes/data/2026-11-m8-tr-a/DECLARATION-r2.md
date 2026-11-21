# M8 TR-A -- DECLARATION-r2 (successor to DECLARATION.md, which stays frozen; committed before any r2 leg)

## Why r2
Run 1 of DECLARATION.md (code head f733fd02d / mutation copy 6c189ea12): runtime P2 PASS (interior 1.000076, ipm 1.000000, ssn 1.000000, walk 1.000000), replay P3 and interior P4 PASS (0 differences x9 plus armed), ctest at the dev head green after the migration fixes. Two failures:
- P5 install smoke: scripts/check_export_contract.sh FAIL -- the umbrella hven.h did not include drivers/iteration_record.h (a real defect, fixed in c89c6d6db9e8185031812ccfdbdc779425f15df8).
- M: v4 (IPM main-site factorizations stamp dropped) SURVIVED -- V4's pin was too weak (monotone and bounded only). The pin now also requires strict growth of the count across every stepped IPM row.
## r2 legs (code head c89c6d6db9e8185031812ccfdbdc779425f15df8; src/ and bench/ unchanged since f733fd02d except include/hven/hven.h)
R1 ctest Release, all targets, 100% passed.   R2 install smoke default and clang + export contract: pass x2.
R3 mutations: control fails nothing; v4 killed by a failing test (the other thirteen were killed in run 1 against src identical to this head).
Runtime/replay/interior are NOT re-run: no src/ change affects them (the one library-side change is a header include).
