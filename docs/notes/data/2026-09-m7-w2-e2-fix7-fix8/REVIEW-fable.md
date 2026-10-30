> Copied verbatim from `.superpowers/` (source: the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 E2 fix rounds 7 + 8 — scoped review (Fable)

Reviewer: Fable 5.1, 2026-09-30. Scope: `git diff 2b4c9fa8 212886d5` (13 files; the two
fixture dumps skimmed by header only), read from committed objects. Evidence read under
`.claude/worktrees/agent-a69f46e11796560b9/.scratch/{e2fix7,e2fix8}/`. Nothing built, no
solver or test run; two read-only Python/awk passes over saved logs (the RPX lines, the corpus
rowcmp). Tree untouched except this file.

## (1) Rule A — an un-run test is never read as a verdict

`src/qp/ws_independence.cpp:206-219`: `!f.usable()` → `screen ? kDependent : kIndependent`,
`unrun = true`, rho left NaN, solves 0. Every library consumer of a `DepTest` traced:

- `QpEngine::test_candidate` (`src/qp/qp_engine.cpp:3976-4039`): R-A runs on `kIndependent`
  and REPLACES `t` with the exact row test (whose `unrun` is false) — so an un-run verdict R-A
  overrules is not counted `dep_unrun_tests`, as the counter comment states. R-E's grey re-read
  is gated on `t.rho < kDepUndecidable`, false for NaN by construction: a NaN rho never buys a
  re-read. `dep_screen_under_mandatory_dependence` reads `!(NaN <= kDepTol)` = true, i.e. the
  screen-positive un-run half is counted there when the mandatory rows are dependent — correct.
- The add site (`qp_engine.cpp:1841-1899`): `!t.dependent()` → the ordinary add; the
  `kUndecidable` branch (deferred add, `recheck_pending`, `deferred_member`) is unreachable
  for an un-run test. The known-dependent zero-motion guard (`qp_engine.cpp:1877`) still
  refuses an un-run `kIndependent` on a member the detector removed — the one place an
  un-run admission could re-enter a proven-dependent member, and it is closed.
- The ride (`qp_engine.cpp:5115-5127`): `t.dependent()` false → the ride proceeds, pre-E2.
- The repair (`qp_engine.cpp:3874-3877`): `test_candidate` only under `view.usable()`, so an
  un-run test cannot arise there.
- `detect_dependency` / `structural_dependency` (`ws_independence.cpp:~400-560`) build their
  own systems and never read `DepTest::verdict` from a `dependency_test`.

Nowhere is `kUndecidable` or `dependent()` reached by an un-run test. Unit contract
(`tests/sqp/test_qp_ws_independence.cpp:796-883`) covers all three cases, including the
"truly dependent candidate the test may not call dependent because it did not look" case.

"Screen negative → ordinary add" on an in-fact rank-deficient set: what catches it is the
pre-E2 machinery, not a new guard — the pass-top detector fires only on `over`
(`me + |W| > n`), `entry_check_pending`, `recheck_pending` or a shift add
(`qp_engine.cpp:900-916`); an un-run add sets none of these. So a dependent member admitted
un-run is caught (a) by the over-count on a later growth, (b) by the next factorization's
inertia/suspect reading, or (c) never, at the cost E2 was built to avoid. That is exactly
S17's pre-E2 admission and S39 ships it knowingly; the mitigation is that F7-1 removes the
main route to an un-run test at all (the measured corpus/HS/in-cell `unrun` is 0 everywhere
after fix8; the remaining route is a pass that never held a view — the border path before a
Schur complement exists). Acceptable; the honesty counter is in place to watch it.

## (2) Rule B and the budget `max(8, ceil(n/2))`

Derivation re-checked from the raw RPX lines, not the report: on-arm 838 repairs, 832 OK /
6 FAIL (off-arm 818/20); largest successful `tried` 161 (n = 824); largest `tried/n` at
n >= 24 is 75/344 = 0.218 (ACOPR118 qp5 border); 14 screen-positive repairs, which are the
only ones rule B touches. In-cell (`diag/repro/*-pd0.err`, unbounded): ACOPR118 16 repairs
all OK, max 118 pins (solve 0, cold, `screen_pins 11`), next 99, 96; ACOPP118 4 repairs all
OK, max 118, then 55, 53. The shipped-n/3 run shows that same solve-0 repair
`FAIL tried=115 budget=115 stopped=1` — the refutation of n/3 is on the row. n/2 = 172 at
n = 344 = 1.46 x 118. `repro2/ACOPR118-default.out` and `ACOPP118-default.out` are IDENTICAL
to the unbounded `pd0` runs in every non-wall field (my diff). The counters-header numbers
("838 replayed, 20 in-cell", p50/p90/p95, max 161, 0.218 / 0.195) all reproduce.

"Probes <= 2·pins" is a true property of the construction: the pin loop probes once per
`pinned.push_back` (`qp_engine.cpp:3884-3889`), the budget check is at the loop top BEFORE
the candidate is tested (`:3836-3839`) so `|pinned| <= pin_budget`, and the release pass
(`:3908-3918`) probes once per element of `pinned` and only on success. Candidates that are
`continue`d (certified dependent, or unpinnable) cost no probe. So removing the WIP's probe
budget removed dead code. The screen-NEGATIVE path of the repair keeps the old numerical test
verbatim (`:3874-3877`); only the screen-positive path changed (`structural_dependency` alone,
counted `dep_repair_screen_pins`).

Budget stop: `budget_stopped` → `++dep_repair_budget_stops` → the ordinary failure branch
(`ws`/`x` restored, `return false`), then F7-1 at the call site. On the first pass that
leaves `first_pass` false, so the next pass reads `kWrong` with no repair available and ends
at the terminal disjunct → `kNumericalError`. It cannot certify anything false: a `kWrong`
face never passes the inertia gate, and a stopped repair leaves `ws`/`x` exactly as before.
State left behind is only `face`/border, which F7-1 handles (below). The post-probe-restart
caller (`qp_engine.cpp:1296-1313`) sets `kNumericalError` directly on a `false` return, so a
budget stop there is terminal too.

`QpE2Fix8Budget` (`tests/qp/test_qp_unrun_dependency_test.cpp:303-372`, H = −I, n = 12):
this IS a regression the fixture enshrines, and the fixture says so honestly — the pre-fix8
E2-on walk would have pinned all twelve (the screen `0 + 0 + k + 1 > 12` fires only at k = 12,
i.e. never on a candidate), so fix8 turns a certified vertex into `kNumericalError` on this
class. The class is "inertia deficit above n/2 with no rows" — a fully concave box — which
none of the 858 measured repairs approaches (max 0.343 n) and which the deficit the gate
already reports could bound exactly (F8-2). I read it as an honest declared cost, not a
disguised one: it is pinned by name, the mechanism is stated correctly in the header, and
S42 accepted it. I would schedule F8-2 before the landing round rather than leave it
registered, since the deficit is free information at the probe.

OFF arm byte-identical: the budget is `bounded = opts.independent_working_set`
(`:3827`); rule B's block is inside `if (opts.independent_working_set)`; F7-1's
`view_before_repair` short-circuits on the option (`:977-978`) so the OFF arm makes no
extra `dep_factor_view` call; `pin_budget` is trivial integer arithmetic. Measured: corpus
OFF 0 rows vs V7/k1off; YORKNET OFF and STEENBRG OFF (`repro/*-off.out`) match RT9's
control rows in status, objective, majors, minors, factorizations (my check against
`hven-bench/runs/2026-09-m7-w2-retake9/run-A-independent-working-set-off`).

## (3) F7-1 — correct and complete

`qp_engine.cpp:973-1027`: spend the pass iff `dep_factor_view` was usable before the repair
and is not after. A failed repair that probed has re-captured `face` on a pinned set (or
pushed pins on the border ledger) so `face.holds`/`border_holds` fail on the restored `ws`:
view lost → spend. A repair that never probed changed nothing → fall through (T7Fix2 holds).
A pass that never had a view falls through into rule A's un-run verdict, which is the
designed fallback, not a stale one. Other routes checked: the successful repair `continue`s
(the release pass can leave `face` describing ws-minus-one-pin, but the next pass
re-establishes; pre-existing and harmless); the post-probe-restart failure is terminal
(`:1313`); `reread_wrong_inertia` does not touch `face`; `kSuspect` mid-repair takes the same
failure branch and is covered. Complete. `first_pass` is already false, so it cannot loop.

## (4) Movers

- HS 3340 → 3342: `.scratch/e2fix7/hs-table-off.txt` vs `hs-table-f7-once.txt` differ in
  exactly the four HS15-cold rows; majors/facts/acc-rej/SOC/elastic columns hold. Rule B at
  n = 2 (the RPX lines: 6 HS repairs `base FAIL tried=0 screen_pins=2 → OK tried=1`).
  Credible. Accelerate 3717 not in the diff — UNOBSERVED, untouched. NIT: the pin comment
  (`tests/sqp/test_hs_sweeps.cpp:1343-1346`) says "1067 -> 1069 in each of the four KD/SOC
  cells"; the table shows 1067→1069 in the two SOC-on cells and 1062→1064 in the two SOC-off
  cells. Same +2, wrong cell values.
- Corpus V7 → H2 (my rowcmp, new-column noise stripped): exactly Hj_STEENBRD/qp4,
  STEENBRB_def/qp0, STEENBRB_def/qp1 per algebra, +1 minor each, no status/f/residual
  column; `rrs` = 1 on those rows. F7-1, credible. (qp0 border also moves `refuse` 94→95 and
  `residiters` +1, which the report's one-line summary omits — telemetry, same class.)
- `AFullyPinnedRow…` border minors 1 → 2 with `dep_repair_restarts` 1: every correctness
  assertion (kInfeasible, one verified certificate, blocker 2, y_e, implied, limit) is
  asserted unchanged (`tests/qp/test_qp_anti_cycling_arm.cpp:1728-1756`). A counter-route
  re-pin, not a fixed-by-re-pin. No status/objective/certificate anywhere was re-pinned.

## (5) The cap (S43)

Evidence (`logs/capreplay.txt`) supports: all 8 in-cell capped dumps replay to the in-cell
exit exactly; at 4x the cap all 8 are still `kMaxIter`; factorizations flat on 7/8;
`stall_admits` climbs (qp3 839 → 4742); the E2-OFF arm caps on all 8 too. So "do not tune the
cap / no cap retry rescues them" is supported. "CYCLE" is inferred, not witnessed: no
working-set recurrence was measured, and the objective is NOT confined to "the 4th–7th
digit" as §8 says — ACOPP118 qp3 moves 3922.74 → 3387.06, ACOPR118 qp4 27.08 → 24.84, qp6
rises 12.029 → 12.066. On the border algebra, zero factorizations over many minors is also
ordinary (updates, not refactorizations). F8-1 should START by measuring recurrence
(ws-signature repeats / point-memory hits) before assuming a cycle; S43's wording should say
"stalls to any cap in both arms", not "cycles", until then.

## (6) Wall attribution (R3 vs rule B)

Supported. STEENBRG `default` on the RT9 pin binary vs fix8: same status (max_iter), same f
(36241.0123548), same stationarity (0.9592), 86 majors; wall 359 s vs 361 s from the log
timestamps. YORKNET > 600 s on both (three logged kills, own pids). OFF twins identical to
RT9 (above). ACOPP118 `default` RT9 247 s → fix8 48 s is true; note it ends at stationarity
4441 (5 majors) where RT9 sat at 3529 and the n/3 arm reached the OFF twin's f at 6.73 —
same status, so no scored row moves, but "cheaper" is not "better".

## (7) Hygiene

Headers terse enough and factual: no `.superpowers` pointers in any committed source; the
two removed false claims ("may not gain a line…614 factorizations", "a budget stop is the safe
direction") are absent; `qp_engine.h:936-941` is the whole budget surface. Counters sit in
`QpCounters`' independence group after `dep_stall_admits` (`solver_counters.h:374-406`),
default 0, comments state the exact semantics; like the other `dep_*` counters they are not
aggregated into `SqpCounters` (the standing contract; the lane sums them, F8-4/S41). The
`kStartRepairRefused` enum value is documented. clang-format: 0 replacements on all ten
touched source/test objects (`clang-format --style=file --output-replacements-xml` over
`git show`). `git diff --check` clean. `grep -ci co-authored` over the two messages = 0.
IPARM untouched; `notices/` untouched.

One gap: `tests/qp/test_qp_walk_seam.cpp:773-808` has an exhaustive `switch (r.action)` with
no `default` (every action carries a size identity; `kUnset` is `ADD_FAILURE`), and
`kStartRepairRefused` is not listed. It belongs with the `base == ws_size_out` group (the
record's own comment says the pass priced nothing and stepped nowhere), which is a stronger
statement than `kStartRepair`'s. Not fatal (`-Werror` is only set inside a configure probe,
`tests/CMakeLists.txt:53`), but the seam's contract is now incomplete.

## Verdict: CLOSED, with two riders for the next commit (no re-review needed)

R9-1 `tests/qp/test_qp_walk_seam.cpp:~795` — add `case Action::kStartRepairRefused:` under
the `EXPECT_EQ(base, r.ws_size_out)` group (and `Block::kNone`).
R9-2 `tests/sqp/test_hs_sweeps.cpp:1343-1346` — "1067 -> 1069 in each of the four" → the two
SOC-off cells are 1062 -> 1064.

Settler notes (not fix items): S43 "cycle" → "stalls to any cap in both arms" until F8-1
measures recurrence; the four `default` cells run on the shipped binary carry no
`dep_repair_budget_stops` witness (the lane does not export it, F8-4), so "0 budget stops"
is a statement about the 858 instrumented repairs, not about STEENBRG/YORKNET's
trajectories — export the four counters before RE-TAKE 10 as S41 already plans; consider
pulling F8-2 (deficit-aware budget) ahead of the landing round.

Ledger: fix7 (rule A, `b05f75e0`) + fix8 (bounded rule B `max(8, ceil(n/2))`, F7-1 narrowed,
`212886d5`) reviewed CLOSED — verdict logic, budget derivation (838 + 20 repairs, max 0.343 n,
1.46x headroom), OFF-arm identity and all movers verified on the evidence; two test-side
riders; S43's "cycle" is inferred, not measured.

SIGNOFF: M7-W2-E2-FIX8-REVIEW-FABLE
