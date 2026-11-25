# M8 TR-E (the trace consumers) -- DECLARATION (committed before any leg; never edited afterwards -- a failure is DECLARATION-r2.md)

Spec: docs/notes/2026-11-m8-tr1-trace-spec.md section 5.1 (hven-bench items 1-6), section 5.2 (the M7-4 residual), section 6 row TR-E.
Schema: docs/trace-schema-v1.md. hven base = origin/m8 `9d9dd2b2a`. hven code head under test = `24b2a7e42f3a56f1f3293fe9d70d13fc4d50e708`
(branch tr-e; commits ca1f53e5e dump_counters + fixtures, 968605c8f nested sqp fixture, 24b2a7e42 docs). hven-bench: branch tr-e off main d7a55b9;
code commits 3aa06da, d58608a; the pin bump 1ddbf2b is the LAST bench commit and pins exactly the hven head above (if hven code changes, that is a -r2).

## What changes
hven (no behaviour change, no break, no trace/console byte change):
- `hven::dump_counters(const IpmResult &, std::ostream &)` and `(const SqpResult &, ...)`: `name = value` lines from the X-macro tables `solve.end.counters` is
  written from (IPM adds `kkt_analyses_total` and the 13 `kkt_factor_counters.` fields; SQP flattens `ssn.` and `ipqp.`). New TU src/drivers/counters_dump.cpp.
  Tests `DumpCounters.*` (held against the trace counters object on real solves; determinism and stream-state; absent spelling).
- The v1 reader fixtures: tests/drivers/golden/trace_v1/{ipm_complete,sqp_complete,ipm_aborted}.jsonl, byte-exact, held by `TraceV1Fixtures.*`.
- docs/migration-0.2.md section TR-E, CATCH-UP.txt, docs/trace-schema-v1.md pointer.
hven-bench (declared harness schema events, docs/tr-e-trace-consumers.md): side-file columns through `dump_counters` (name map schema/side_columns.py);
`eval_f/g/jac/hess` from `SolveResult::work`; `--trace` per-cell v1 stream with a `traced` pin; schema/trace_v1.py reader; schema/compare_side.py
comparator; compare.py reads pre-TR-E headers as traced=0 and gains `--filled` / `--moved`; row-schema rename; the pin bump (item 1).

## EXPECTED MOVERS (everything else must not move)
1. Side-file column NAMES (map: IPM kkt_* -> kkt_factor_counters.*, kkt_analyses_this_call -> symbolic_analyses, soc_steps_taken -> soc_steps; SQP ssn_* -> ssn.ssn_*)
   and the SET of side columns (the dump carries counters the hand enumeration did not). VALUES of every mapped column: 0 differences.
2. `eval_f/eval_g/eval_jac/eval_hess`: null -> value on both hven lanes (declared `--filled`).
3. `options_digest`: moves on every hven row by construction (the options dump text changed between the pins: AP-5 renames, `wide_console` removed); declared `--moved`.
4. The `traced` pin column (0 on every untraced row; a pre-TR-E results.csv is read as 0).
5. NOT MOVING, asserted: status, outcome, iterations, factorizations, every counter column, every measure column (byte or the schema's near-ulp gate), budgets,
   hessian_provenance, set pins, split. `wall_s` is informational and never compared.

## Proofs and pass criteria (every run under `flock /tmp/box-build.lock`, `pgrep -a hven_bench_cell` empty first and as its own command, setsid -w, MKL_NUM_THREADS=1, ctest -j4)
P1   hven ctest, Release and Debug, every target, at the code head: 100% passed (includes the new `DumpCounters.*`, `TraceV1Fixtures.*`, and every pre-existing golden unchanged).
ST   Bench comparator self-tests (legs/selftests.sh, in the private clone of branch tr-e): tests.test_compare_side, tests.test_trace_v1, tests.test_compare all OK.
     The comparator's discrimination is part of the claim: its tests include a counter moved by one, a vanished column, a map pointing at the wrong column, a colliding
     map and a count with no gate, each of which must NOT read clean.
TR   ONE TRAIN leg, old pin vs new pin, serial and solo, TRAIN only (HOLDOUT is NOT run; the runner refuses it):
       arm A = hven-bench 7d974e8 (the last main commit at the old pin) built against hven bdd797e342d7; arm B = hven-bench branch tr-e (the pin bump 1ddbf2b) built against
       hven 24b2a7e42f3a. Cells: hs-27 TRAIN x {hven-ipm, hven-sqp} default; cutest-nlp-35 TRAIN {ACOPP57, SMBANK, READING6, ZAMB2-8, DALLASM} x {hven-ipm, hven-sqp} default.
       Verdicts (legs/train_compare.sh): (a) results.csv A vs B through `schema/compare.py --filled eval_f,eval_g,eval_jac,eval_hess --moved options_digest`: 0 mismatches, both sets;
       (b) side files A vs B through `schema/compare_side.py`: 0 differences, 4 files (2 lanes x 2 sets); the added-column list and renamed list are recorded;
       (c) the traced re-run of the hs leg on arm B vs the untraced one (`compare.py --moved traced`, `compare_side.py`): 0, i.e. tracing moves no counter; every written stream
       passes `schema/trace_v1.py` with 0 errors. Counters and statuses match EXACTLY; only names move. A cell that differs is re-run ALONE before it is called a difference.
M74  The M7-4 re-measure (spec 5.2) on the TR-E head's Release `hven_sqp_corpus`, interior (IPM corpus) leg, serial, solo, taskset core 2, perf instructions:u, G5's instrument
     (analyze_r11.py: counter identity first, THROTTLE == 0 or VOID, filtered samples median of 3 rounds summed over the 27 cells; arms r1 A->B, r2 B->A, r3 A->B):
       aa   : both arms unattached -- the instrument's noise floor on this box (reported);
       att  : arm A no callback, arm B the counting callback attached at kScalars -- the attach cost = B/A - 1.
     VERDICT: |attach cost| <= 0.5 % with counter identity and 0 throttled => M7-4 CLOSES; otherwise it is REGISTERED with its number. Both numbers quoted: this one and AP-5 G5's
     (docs/notes/data/2026-11-m8-ap5-g5/runtime/attach-delta-interior.txt: +0.0322 % at kScalars, arm A; +0.0314 % arm B). The 0.5 % band is the spec's, not re-banded on a reading.
Not run, disclosed: HOLDOUT (forbidden); the ipopt/snopt lanes (external lanes unchanged by TR-E; their binaries are not built in the private clones); an install-smoke and export-contract
leg (no public header was added; two declarations were added to existing public headers and one source file to the library, which the pin-prefix builds below install and the lanes then link, so the install
round-trip is exercised by the TRAIN legs' own binaries, not by check_install_smoke.sh); no mutation leg (TR-E adds an observer and a consumer, no engine path; the comparator's self-tests carry the discrimination proof); no Accelerate/Windows (UNOBSERVED).
Pre-declaration runs (not evidence): the Debug build and first Debug runs of the new tests, the Release build, the bench unit tests, the pin-prefix builds and any smoke cell.
Scratch under the worktree's ignored .scratch/; committed here: this file, legs/, summaries, CSV excerpts, VERDICTS.txt, MANIFEST.txt (sha256, size, path). Raw dumps stay ignored.
