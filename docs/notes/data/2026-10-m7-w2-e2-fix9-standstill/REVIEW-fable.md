> Copied verbatim from `.superpowers/` (the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 E2 fix round 9 ("the standstill retreat") — scoped review (Fable, 2026-10-01)

Scope: `git diff bffd2ea2 1f8d3118` (one commit; base sources == 212886d5). Read from committed
objects only; nothing built or run except `clang-format --dry-run` on extracted objects, `git diff
--check`, and plain `diff`/`cmp`/python over the scratch evidence under
`.claude/worktrees/agent-a69f46e11796560b9/.scratch/{f81,e2fix9}/`. Inputs: brief S42–S47, the
F8-1 diagnosis, the fix9 report, the fix8 review (R9-1/R9-2). OBSERVED = I re-derived it from the
raw artifact; REPORTED = the report's claim I could not reach independently.

## (1) The rule as coded (`src/qp/qp_engine.cpp`)

- Qualifier `standstill_face` (:1192): `eq_rows_violated && !eliminated_only &&
  opts_.ws_algebra == kSchurBorder && verdict == kWrong`. `eq_rows_violated` is set only inside
  the `independent_working_set && me > 0` drift block (:1041–1060), so E2-off leaves it false and
  the option-off arm is untouched by construction — matches the ruling's five conjuncts exactly.
- "Structurally violated" IS the walk's own currency: the drift block's per-row
  `violation_is_structural(v, row_scale, lambda_e(j), eff_opts)` (:1053), the same predicate
  section 5's terminal classifier (`worst_structural_violation`, :2614) uses — tolerance band (a)
  plus the `kStructuralResidualFrac * dual_mu * |lambda|` footprint. Not a new threshold.
- Form (i), zero motion (:1195): fires before the KKT-point branch; sets `eliminated_only`,
  `++standstill_retreats`, `recheck_pending`, records `kEliminatedRetreat`, `continue`s.
  Form (ii), end of pass (:2329): `kind == kNone && !ride_blocked && alpha >= 1.0` extends the
  run; `>= 2` retreats the same way (no seam action — see rider).
- Reset: `standstill_steps_in = standstill_steps; standstill_steps = 0` at the loop top (:713–714)
  and the only write is :2330 at the very end of the body, so every `continue`, drop, add,
  retreat, and status `break` leaves the run at zero. Verified by reading every exit between
  :715 and :2336 — there is no path that reaches :2329 without passing :714 first. Correct.
- Bounded: `eliminated_only` is never cleared (writes at :1196, :2024, :2332 only; init :657)
  and is a conjunct of the qualifier, so the rule fires at most ONCE per solve and a
  retreat→standstill→retreat loop is impossible. After the retreat the eliminated path's own
  bound is the minor cap, as before (ACOPP118 qp2 shows it: `standstill_retreats` 1 and still
  kMaxIter). That is pre-existing and acceptable: the rule adds one factorization at most
  (T7Fix2's 4→5 is exactly that cost), it removes no termination.
- Healthy walks: at zero motion the EQP's own equations put the iterate on its rows up to the
  μ footprint, so a STRUCTURAL violation at zero motion is already evidence of a wrong solve or
  inconsistent rows (T7Fix2's e0=0/e0=1 is the latter); kWrong is a further restriction there.
  For form (ii) the restriction carries the weight: ACOPP57's 569-pass full-step jitter at
  9.87e-6 off its rows reads kOk with EQP ratio 1e-12 (F8-1 §4, OBSERVED in `f81/STATE.md`
  03:40), candidate S without kWrong regressed it, S2 with kWrong did not. Measured separator
  evidence: every capdump trigger point kWrong; the approximate S2 trigger fires on 0/51 corpus
  dumps and 0/5 ACOPP57 base solves offline; S2 corpus 0/102, fix9 corpus 0/102 and 0/51
  (OBSERVED, §5). So kWrong is a sound sufficient restriction IN CONJUNCTION; it is not a
  standalone signature — F8-1's corptrace found kWrong border passes inside healthy optimal
  corpus walks (YORKNET, STEENBRB_def qp0, R1_Cs qp3). The header text over-claims this (rider).
- False certificate: none possible from the rule. It emits no status; it hands the same working
  set to the eliminated factor with the dependency re-check armed (`untested` at :917 reads
  `recheck_pending`), and kOptimal still has to pass the unchanged terminal tests. The fixtures
  then re-derive the KKT battery from the dump's own matrices (`qp_dump.h:205`, engine-free).
- Observation, no item: form (i) pre-empts both the Dantzig drop and R-C's kWrong re-read at the
  terminal site (:1300). Both run on the redone eliminated pass instead, with a trustworthy
  factor — cost one factorization, semantics unchanged.

## (2) Principled recovery or tuned knob

Principled in direction: the trigger is the factor's own evidence (an EQP whose full step does
not land on its rows, or a KKT point of the face that is structurally off them) routed to the
engine's existing answer to "the border path cannot be trusted here". The count "2" is not
tuned — it is the smallest count that lets one legitimate full step land before judging. The
kWrong conjunct is the empirically chosen component: its attribution to the broken factor rests
on the oracle's Z'HZ ≥ 0 on the visited faces (F8-1 §3c, OBSERVED) while the mechanism behind
the reading (O(μ) eigenvalues vs perturbed pivots) is undetermined (§7). It IS a symptom
treatment of F81-1/F81-2 (exact dependencies admitted; impossible ρ on the border path), and
landing it ahead of the root cause is sound: it converts a cell with zero regressions on every
screen, it is once-per-solve, and it does not mask the defect — `standstill_retreats` is its own
counter, the seven fixtures PIN it at 1 on the border arm, and the lane column is scheduled for
RE-TAKE 10. A root-cause fix that stops the standstill will move those pins and must declare it.

## (3) The counter

`QpCounters::standstill_retreats` (`solver_counters.h:352–356`): terse, states the trigger and
the exclusion from `dep_border_retreats`; incremented at both forms only; separate from the
`dep_*` group by name and by accounting (border retreat :2025 does not touch it). The split is
visible in the cells: ACOPR118 `harness_qpsum_dep_border_retreats` 11→8 and ACOPP118 6→0
against the F8-1 S2 scratch, the only differing field in both (OBSERVED). Option-off arm: the
qualifier is false by construction and measured identical (§5).

## (4) Fixtures and test riders

- Seven dumps, headers honest: each `result status=1 minors=4710|5300 … retreats=0` is the
  fix8-head cap (OBSERVED on all seven); the test asserts that premise before solving.
- Each test asserts kOptimal, `standstill_retreats` 1/0 by arm, |f−ref| ≤ 1e-6·max(1,|ref|),
  and the independent KKT battery. References: six are the kRefactorize algebra's own certified
  values (the arm that never takes this rule; two of them asserted in-file as the refactorize
  tests); ACOPP118 qp3 is the SLSQP oracle 1592.5507 (refactorize caps there), and hven lands at
  1592.55071642 (Release and Debug, OBSERVED). Honest.
- Minor: on the ACOPR dumps (f ≈ −1e-3…−8e-5) the tolerance is effectively 1e-6 ABSOLUTE, i.e.
  1e-3…1e-2 relative; the border and refactorize points differ by ~5e-9 (distinct KKT points of a
  flat basin). The KKT battery (measured ≤ 1e-13) is the real certificate, so this is a basin
  check — acceptable, say so in the file (rider).
- Release-only trio: ACOPP118 qp0 certifies in Debug too (231 s, OBSERVED in
  `fixtures-B1-debug` history); qp1 caps in Debug on BOTH algebras incl. refactorize (OBSERVED:
  `[ e2f9 ] ACOPP118_f81_qp1.txt refactorize status=1 minors=4710` in the Debug log). Since
  refactorize never enters the rule, that is the dump's rounding sensitivity, which is itself the
  F81-1 geometry (exactly deficient faces), not a defect of fix9. Acceptable with F9-2 registered.
- F9-1 (observer not attached): acceptable — the re-check removal pass records `kUnset`
  (pre-existing, `qp_engine.cpp` ~:966) and the exhaustive seam switch would ADD_FAILURE; the
  counter and the KKT battery carry the assertions. It stays a seam item.
- `T7Fix2…` 4→5: a counter pin (`border ? 5 : 4`, `standstill_retreats border ? 1 : 0`); the
  kInfeasible status and x(0)=0.5, x(1)=0 assertions are untouched in the diff; `t7fix2-rel-1.log`
  passes. No correctness fixture re-pinned anywhere else in the diff.
- R9-1 applied as specified (`kStartRepairRefused` under the `base == ws_size_out` group with
  `Block::kNone`); R9-2 applied (1067→1069 SOC-on, 1062→1064 SOC-off) and confirmed by the key
  log's HS 3342.

## (5) Evidence, re-derived from the raw artifacts

- Corpus: `diff` of `F9-{border,refactorize}-{old36,new15}.txt` vs `H2-*` after stripping the
  new `sstr=` token: 0/0/0/0 differing lines = 0/102; `F9off-*` vs `H2off-*`: 0/0 = 0/51;
  `sstr` nonzero on 0 of 153 rows. OBSERVED.
- U0: `u0-B1/comparison.txt` byte-equal to `e2fix8/u0-H2/comparison.txt`; the three M6-pin
  comparisons read 17/13/13 = the declared 43; per-column diff of `head-{walk,ssn,ipm}.csv` vs
  fix8 H2: only `wall_s` differs (27 rows each). OBSERVED.
- Suites: `ctest-release-B1.log` 100% of 2891; Debug 1446 + 1444 = 2890 (3 declared skips of the
  new tests listed by name); long test alone 1/1 at 625.6 s. OBSERVED.
- Cells: ACOPR118-default.out `status=optimal`, 21 iterations, f 129660.68370269175,
  stationarity 6.97e-9, factorizations 15430; ACOPP118 numerical_error. Eleven TRAIN cells
  byte-identical to `f81/train/*-S2.out` (cmp); ACOPR118/ACOPP118 differ from `f81/cells/*-S2.out`
  in `dep_border_retreats` only. E2-off twins: ACOPR118 off on the fix8 binary == fix9 binary
  (cmp), ACOPP118 off == `e2fix8/repro` (cmp), STEENBRG == fix8 repro. OBSERVED.
- clang-format 0 differing lines on all 7 code/test files; `git diff --check` clean; YORKNET
  stop logged with its own pid in `wallcap.log`. OBSERVED.

## (6) Hygiene

No `.superpowers` pointers, settler names or ruling numbers in any added comment (grep of the
`+` lines over src/include/tests). Round labels ("E2 fix9") follow the file's existing convention.
`git log --format=%B bffd2ea2..1f8d3118 | grep -ci co-authored` = 0. The one text claim I would
tighten is the header's "kWrong … is that factor's failure signature" (rider below).

## Verdict: CLOSED, with riders for the next code commit (no re-review needed)

R10-1 `include/hven/detail/qp/qp_engine.h:584-586` — kWrong alone is not a signature (healthy
optimal corpus walks carry kWrong border passes); state the separator as the conjunction:
standstill AND structurally off the rows AND kWrong, screened at 0/51 corpus + 0/5 ACOPP57.
R10-2 `src/qp/qp_engine.cpp:2329-2336` — the second-full-step form records no walk action, so
the seam sees it only through the counter; fold into F9-1's seam item (an action for the
end-of-pass retreat, or record `kEliminatedRetreat` on the redone pass).
R10-3 `tests/qp/test_qp_standstill_retreat.cpp:81` — say the objective check is a basin check
(absolute 1e-6 on O(1e-3) ACOPR objectives) and the KKT battery is the certificate, or make the
tolerance relative with a floor.

Ledger: fix9 (`1f8d3118`, S2 as production code, once-per-solve border retreat on kWrong
standstills off structurally violated equality rows, `standstill_retreats` counter, 7 fixtures,
T7Fix2 counter pin, R9-1/R9-2) reviewed CLOSED — rule, reset, bound, option-off identity, corpus
0/102 + 0/51, U0 43 with wall_s the only moving column, suites 2891/2890/1, ACOPR118 optimal and
the E2-off twins all re-derived from raw artifacts; three text/seam riders, no code defect.

SIGNOFF: M7-W2-E2-FIX9-REVIEW-FABLE
