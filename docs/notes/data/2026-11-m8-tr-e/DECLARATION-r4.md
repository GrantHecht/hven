# M8 TR-E -- DECLARATION-r4 (committed before the re-run; DECLARATION.md, -r2 and -r3 are unedited)

Written AFTER reading r3's data (disclosed): r3's TRAIN A-vs-B showed (i) both IPM side-file comparisons broken by a CSV defect of my own lane code (the dump's one array value,
`recovery_depth_histogram = [0, 0, 0, 0, 0]`, put commas inside a cell: the row had 44 fields against 40 header columns), (ii) the five cutest cells ERRORED in both arms because the
clones lacked the decoded CUTEst problems (`.scratch/h3/decoded/<P>`, ignored data), and (iii) on hs, results.csv A vs B: 0 mismatches, but the hven-sqp SIDE files differed in 29 values
of internal counters (eqp_polish_steps, border_refine_steps, harness_qpsum_dep_test_solves, ...) with identical iterations/factorizations/statuses.

What r4 changes, and only this:
1. Lane fix (hven-bench 2f24d63): the dump's array value is spelled `[a;b;c;d;e]` in the side file. Copied clone data: the decoded CUTEst problems and the SIF directory into all three clones.
2. A THIRD ARM, A' = hven-bench main d7a55b9 (the OLD lane code, hand-enumerated side file) built against the NEW hven pin prefix, with one local commit in a private clone that moves its pin
   file to 24b2a7e4 (legs/bench_build_mid.sh). Why: arm A vs arm B differs in TWO things at once, the consumer (TR-E) and hven (267 commits between bdd797e3 and 24b2a7e4, among them DN-3's
   dense auto-selection and AP-5's driver numerics), so a counter that moved cannot be attributed. A' vs B differs in the consumer ONLY.
3. The judgement of the TRAIN leg, re-stated against that: the spec's claim ("0 counter diffs through the column map; only names move") is the claim about THE CONSUMER CHANGE and is
   tested by (b) A' vs B: results.csv 0 mismatches and the four side files 0 differences, on hs-27 TRAIN and the five cutest cells. The declared pin-spanning comparison (a) A vs B is still run
   and reported: results.csv must be 0 mismatches (status, outcome, iterations, factorizations, every measure inside the gate; eval_* filled and options_digest moved declared); any SIDE
   value that moved between the pins is REPORTED BY NAME as hven's drift (it is not a TR-E difference) and attributed by (c) A vs A' (old lane code at both pins): if (c)'s side differences
   equal (a)'s, the drift is hven's. If (b) is not 0 the leg FAILS, whatever (a) says. (d) traced vs untraced B: 0 differences, and every stream validates.
Unchanged: code heads, cells, maps, P1/ST/M74 (their r3 results stand: M74 ran in r3 after its path fix; P1 Release/Debug 3850/3850; ST 60/60).
