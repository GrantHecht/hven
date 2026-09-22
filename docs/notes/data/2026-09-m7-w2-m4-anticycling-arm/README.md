# M7 W2 M4 — the anti-cycling ARM

*Branch `m7`. Chain HEAD `325765d3`, clean. RULE.md is `RULE.md` in this
directory and was **not edited** after its first leg ran.*

---

## The result, in plain language

We built two anti-cycling rules into hven's QP walk behind a test-only switch —
Gill & Murray's EXPAND (arm **E**) and a lexicographic tie-break (arms **L** and
**L+**) — to see whether either makes the ten CUTEst cells hven currently fails
actually solve. **Neither was ever tried on those ten.** Both failed the
safety check that stands in front of them, and the experiment stopped there,
exactly as the rule written beforehand said it would.

The safety check is simple and was fixed in advance: on three cells hven
**already solves**, an arm that only breaks ties should change nothing at all,
so it must reproduce the un-armed run bit for bit. All three arms failed it.
What they did there is the finding:

* **Arm L and arm L+ each BREAK a cell hven already solves.** `ZAMB2-8` goes
  from `optimal` to `numerical_error` under L and to `infeasible` under L+;
  `READING6` goes from `optimal` to `numerical_error` under L+. These are not
  "the counters moved" — the solver returns a worse answer on a problem it
  solved a moment earlier. *(Observed:
  `logs/gate-neutral-L.txt`, `logs/gate-neutral-L+.txt`, `tables/arm.md` §2.)*
* **Arm E never breaks a cell, and never leaves one untouched either.** Status
  stays `optimal` on all three and the objective is byte-identical on two of
  them, but the walk is different everywhere — including on the one cell with
  essentially no ties for a tie-break rule to break. That is EXPAND working as
  designed: it replaces the ratio test rather than only deciding ties. *(Observed:
  `logs/gate-neutral-E.txt`, `tables/readouts-m4.md`.)*
* **E is not obviously better or worse; it is a different walk.** Minors move
  `3833 → 3119` on READING6 (0.81×), `334 → 328` on SMBANK (0.98×) and
  `6666 → 9947` on ZAMB2-8 (1.49×). *(Observed: `tables/arm.md` §2.)*
* On ZAMB2-8 the whole of that +49 % sits **inside** solves that are still
  cycling: minors inside RECURRENT solves go `4620 → 7920` (+3300) while minors
  outside them go `2046 → 2027` (−19), against a total move of +3281. E did not
  break that cell's recurrence; it made the capped solves longer. On READING6 E
  *did* remove one of three recurrences, and the minor saving is exactly that
  one solve. *(Observed: `tables/readouts-m4.md`, "The two questions the
  close-out asks".)*

One thing did come out clean: **the un-armed build is M3c's build.** All ten
OFF controls reproduced M3c's committed rows on every printed field, with zero
values passing by tolerance. *(Observed: `logs/gate-off.txt`.)*

**Nothing ships.** RULE.md §10 fixed that before any leg ran and this result does
not change it. What we have is a ranking input for a future decision with its own
brief: on this evidence **L and L+ are disqualified as production candidates**,
and **E is a live but unproven candidate whose cost is not yet bounded.**
M4b takes E alone to the ten cells under its own pre-registered rule.

---

## What ran, and what the rule said about it

RULE.md §4.1 fixed the order: OFF controls first, then the neutrality legs, then
phase 3 over the ten. `logs/chain.log` is the whole run — started
`2026-09-22T07:28:06-04:00`, `CHAIN_END 2026-09-22T07:50:51-04:00`, 22 legs, all
`rc=0`, every one under the box lock with its own `PGREP_EMPTY` and
`FOREGROUND_START`/`END` lines. No leg was censored and nothing was re-run.
`deadline_fired=0` in 21 of the 22 rows; the exception is `off-SAWPATH`, whose
300 s SAWPATH deadline fired and whose status is `interrupted` — **as M3c's
committed row for that cell already was**, `deadline_fired=1` and `interrupted`
alike, so the OFF control reproduced the truncation too and the cell was carried
forward like the rest. *(Observed: `rows/off-SAWPATH.txt` against
`../2026-09-m7-w2-m3c-default-census/rows/SAWPATH.txt`.)* No ARM leg fired a
deadline, so RULE.md §7.2's `TRUNCATED` label applies to no cell × arm.

| phase | what | outcome |
|---|---|---|
| 1 | the ten §1.1 cells at `--arm off`, gated against M3c's committed pass-1 rows | **REPRODUCES 10 / 10**, 0 near-ulp acceptances |
| 2 | READING6 / SMBANK / ZAMB2-8 × {off, E, L, L+}, each arm gated STRICT against that cell's own `off` leg | **0 / 3 for every arm** |
| 3 | E, then L, then L+, over the ten | **never ran — all three arms stopped** |

The formal outcome under §8, per arm and with its differing fields, is
`verdict.txt`.

**§8's "partial results reported as UNINTERPRETABLE" has nothing to cover.** That
clause exists for phase-3 legs an arm had already run when its neutrality leg was
gated. This chain gated neutrality *before* phase 3, so at the moment each arm
stopped it had produced no phase-3 leg. Nothing is withheld because nothing
exists. No cell carries `CONVERTED`, `HARM` or `improved, unverified` — not a
zero, an absence — and §9's two conditional extensions consequently did not run,
which §9 itself instructed us to say.

---

## The four process findings

### 1. A copied classifier must DERIVE, never CARRY, its declaration

Task A's gate G5 failed against its own machinery because the copied P-SYM
classifier carried the *previous* task's header list instead of deriving one from
this task's `base..head`. This is the **third** instance of one error class — M7
W2 M3a fix1 was the first, M3b fix1 the second. The full account, including what
G5 measured successfully anyway (the production library is byte-for-byte
unmoved by the arm), is `gates/README.md`; it is carried here so the class is
visible from the task's own front page. The repaired classifier has no literal
left to go stale.

### 2. RULE.md §1.2's premise is FALSE for two of the three neutrality cells

§1.2 chose its three cells as "cells where NO tie and NO zero ratio occurs", and
required bit equality there on the strength of that. **The OFF controls' own
counters say otherwise.** *(Observed: `rows/off-*.txt`.)*

| cell | OFF `first_ratio_ties` | OFF `first_degenerate_fraction` | degenerate minors / minors (whole run) | OFF `qp_maxiter_exits` | OFF `qp_numerical_exits` | OFF `qp_failures_routed` |
|---|---|---|---|---|---|---|
| READING6 | 0 | 0 | **875 / 3833 = 22.8 %** | **3** | 0 | **3** |
| SMBANK | 0 | 0 | **1 / 334 = 0.3 %** | 0 | 0 | 0 |
| ZAMB2-8 | **55** | **0.4877** | **2300 / 6666 = 34.5 %** | **2** | **2** | **2** |

(The `first_*` columns are the FIRST QP record only; the third column is summed
over the run's own per-solve table in the same row file.)

So two of the three are heavily degenerate cells that hven nevertheless finishes,
and READING6 hits the minor cap three times on the way. Only SMBANK is tie-free
on every printed scalar, and even it carries one degenerate minor.

**The consequence, and it is the substantive finding of this task.** The
neutrality set did not test the arms on quiet cells. It tested them on
*degenerate-but-solved* cells — which is precisely the population an anti-cycling
rule exists to serve. On that population **L and L+ each break a cell the solver
already solves**, and that disqualifies both as production candidates on this
evidence. *(Inferred from the observed status changes; the observation is the
status change itself.)*

**And E is not inert on the one tie-free cell.** On SMBANK, E's first differing
per-minor record is minor 6 of solve 0, in `alpha`, `dx_inf` and `raw_ratio` —
the ratio test's own output, on a step that is not a tie. That is a behaviour
change **by construction**: EXPAND admits constraints against a slowly growing
feasibility bound and takes a largest-pivot second pass, so it changes which
constraint enters even where nothing is tied. §1.2's bit-equality bar could never
have been passed by an arm of that shape, and the bar being failed says nothing
against the arm. *(Observed: `tables/readouts-m4.md`. **Not** a bug claim — this
close-out does not assert that E violates the paper's (3.2)/(3.3), and no
evidence here bears on that question.)*

### 3. `trace_out` is a LEG LABEL that AMENDMENT 1 did not exclude — a defect of the GATE

AMENDMENT 1 (RULE.md §5.5a), written before any leg, excluded the probe's own
`arm` and `arm_*` keys from a neutrality comparison because they describe the
LEG, not the solve. It missed one field of exactly that kind: **`trace_out`, the
path the leg wrote its trace to, which carries the arm's name in its filename.**
It therefore differs on every neutrality leg for a reason that has nothing to do
with the walk. The defect was found after the chain ran.

**RULE.md is not edited and the transcripts stand.** `logs/gate-neutral-*.txt` is
the gate that ran, and `trace_out` appears in its `FIELDS DIFFER` lines.

**It changed no verdict, and that is proved rather than asserted.**
`gates/neutral-posthoc-no-traceout-{E,L,L+}.txt` re-run the same gate with an
**opt-in** `--exclude-label trace_out`, printing the exclusion with both values on
every leg:

| arm | READING6 | SMBANK | ZAMB2-8 |
|---|---|---|---|
| E | DIFFERS | DIFFERS | DIFFERS |
| L | **REPRODUCES** | **REPRODUCES** | DIFFERS |
| L+ | DIFFERS | **REPRODUCES** | DIFFERS |

§5.5 stops an arm unless **every** neutrality cell reproduces. Each arm still
fails at least one. **All three arms remain stopped.**

The flag is opt-in and the default is untouched, because a gate loosened after
seeing what it rejected is not a gate. `gates/default-unchanged.txt` shows the
amended `gate_m4.py`, re-run at its **defaults** with the chain's own arguments,
reproducing all three committed neutrality transcripts and the OFF-control
transcript **line for line**, differing only by the five FOLD lines the four new
folds add. The new folds prove both directions on constructed inputs: by default
a differing `trace_out` stops the leg; with the flag it is excluded and reported;
a real difference beside it still stops; and the rule is an exact name match that
does not eat `trace_output`.

### 4. What `arm_lex_drop_switches` counts — and what a row gate cannot see

*(The "vacuous on the drop rule" reading of the fixtures is scope-corrected in
"Corrections — FIX ROUND 1", item I3. The counts in this section are field
counts and are unaffected.)*

On READING6 arm L reports `arm_lex_drop_switches=13` while every printed field of
the solve is byte-identical to OFF's. The counter is not broken and the run is
not untouched; the two facts are about different things.

**A "switch" counts one drop decision, inside the armed run, at which the
lexicographic rule selected a different leaving constraint than the nominal
Dantzig drop would have selected at that same decision point.** It is a count of
*divergent choices*, not of cycles broken, not of iterations saved, and not of
anything the run's totals must reflect. Note what it is not: it is not a count
against the OFF run, and after the first switch the two runs no longer share a
working set, so the two counts need not agree.

The per-minor trace shows exactly that. The first place OFF and L part on
READING6 is **solve 19, minor 180**: a `drop_dantzig` pass with
`drop_candidates=51` of which `drop_tied=13` are tied on the multiplier. OFF
drops `bound:58`; L drops `bound:53`, the lowest index in the tied set. The
walks then differ for **60 consecutive records** — 28 of them drop passes
choosing a different index, which is more than the 13 the counter reports,
precisely because past the first switch the two runs are answering different
questions — and then re-converge: same total minors (3833), same factorizations
(6837), same objective, same status. *(Observed: `tables/readouts-m4.md`, "The
walk difference the ROW gate cannot see"; slices
`traces-m4/{off,L}-READING6-solve*.csv`.)*

This is a real limit of the neutrality gate and it is recorded as one: **§5.5
compares ROWS, and a row carries totals.** Two walks can take different routes
to identical totals, and the gate will call them identical. Measured at the
per-minor record instead:

| cell | arm | solves with ≥1 differing record | differing records | row gate (post-hoc, `trace_out` excluded) |
|---|---|---|---|---|
| READING6 | L | 1 | 60 | REPRODUCES |
| SMBANK | L | **0** | **0** | REPRODUCES |
| SMBANK | L+ | 3 | 20 | REPRODUCES |
| SMBANK | E | 29 | 332 | DIFFERS |
| READING6 | E | 15 | 3393 | DIFFERS |
| ZAMB2-8 | E | 23 | 11263 | DIFFERS |

So SMBANK under L is the only leg in the whole task that is inert in the strong
sense — no per-minor record differs at all. L on READING6 and L+ on SMBANK
"reproduce" only in the sense the row can express. Had `trace_out` been excluded
as it should have been, the gate would have passed two legs whose walks differ,
and this table is the reason that would not have been a mistake worth relying on.
*(Observed, POST-HOC; nothing in RULE.md pre-registered a per-record comparison,
and no verdict rests on it.)*

---

## §6.4's declared defect, repeated

RULE.md §6.4 declared, **before** any leg, that a coverage-only recurrence
predicate has a defect on short solves: `maximal_periodic_tail` admits a tail at
`|T| > p`, so at the search cap `p = 64` a 200-pass solve could score 32.5 %
coverage on a walk that merely coincided with itself twice at a stride of 64. T3
(`|T| ≥ 8p`) closes it. It is repeated here because §12 requires every rule defect
to be visible in this file, and because it was declared in advance rather than
discovered afterwards — the opposite case to finding 3 above.

The read-outs in `tables/readouts-m4.md` apply §6.2's predicate as written, T3
included.

---

## What this does NOT show — RULE.md §10, verbatim in force

* **That either arm generalizes beyond TRAIN.** HOLDOUT was never run and is
  never run.
* **That either arm is cycle-FREE on hven's walk.** Neither is proven so; known
  cycling examples exist even for EXPAND (Hall & McKinnon). The read-outs here
  show E *keeping* four RECURRENT solves on ZAMB2-8 where OFF had three.
* **That §2.1's parameters are the right ones.** They are the paper's, taken
  once, and never tuned against a result.
* **That a changed cell changed for the reason the arm claims.** §6.6's
  per-solve read-outs are evidence toward that and are labelled observed;
  anything beyond them is labelled inferred.

Four things specific to this close-out belong beside those:

* **Nothing here says E would help the ten.** E was never run on them. That is
  what M4b is for.
* **Nothing here says L's mechanism is wrong**, only that L and L+ as built, at
  these parameters, damage cells hven solves. RULE.md's Task A finding that L is
  *vacuous* on hven's own drop rule (`arm_lex_drop_switches = 0` on both cycling
  fixtures) stands separately and is not re-tested here — **but its SCOPE is
  the two fixture trajectories and not hven's drop rule**; see "Corrections —
  FIX ROUND 1", item I3, and the 13 and 31 drop switches L records on READING6
  and ZAMB2-8 in this very directory.
* **The post-hoc read-outs of `tables/readouts-m4.md` are POST-HOC.** §6 was
  pre-registered for §1.1's ten cells, not for these three. They are labelled as
  such in the file itself and no verdict rests on them.
* **A solve INDEX is not a subproblem identity across two arms.** Once the walks
  diverge, the same index is a different QP. The read-outs print the first
  diverging index per pair for exactly this reason, and only aggregate columns
  are read across arms.

---

## Corrections — FIX ROUND 1 (2026-09-22)

The Codex review of this task returned **FIX-ROUND 1: 0 Critical, 5 Important,
4 Minor.** Its verdict on the substance stands: *"the formal M4 STOP results
and production-neutrality evidence stand"*, with the qualification that arm E
be cited as **the pinned EXPAND-style implementation, not canonical EXPAND.**
The review is `.superpowers/m7-w2-m4arm-review-sol.md`; the fix round's report
is `.superpowers/m7-w2-m4arm-fix1-report.md`.

**RULE.md IS FROZEN AND IS NOT EDITED, and neither is `wrappers/m4_arm_probe.cpp`**
— that file is the source of the binary M4b pins by sha256 (`M4b RULE.md` §3.1),
so a comment change in it would move a pin for a cosmetic reason. Corrections to
text in either of those two files are recorded **here and nowhere else**.

| # | what was wrong | where it is right now | does it move a number in this directory? |
|---|---|---|---|
| I1 | EXPAND reset **one iteration early**: the K-th minor ran at `delta_0` instead of `delta_K` | `qp_anti_cycling_arm.h`'s `begin_minor()`/`note_reset()`, with a K-boundary fixture and a nontrivial-reset fixture | **No.** All three E legs here record `arm_resets=0` (`rows/E-*.txt`), so no leg in this directory entered the reset path at all |
| I2 | the negative-curvature **ride re-pinned** a blocking bound after an `alpha_min` floor step, undoing the move the floor exists to create | both add paths now ask one predicate, `QpAntiCycleArmState::expand_skips_exact_repin()`; new fixture on an indefinite QP | **No.** All three E legs here record `arm_min_steps == arm_unpinned_adds` (1/1, 4187/4187, 799/799), so no floor event was hidden by a ride re-pin on these rows |
| I3 | RULE.md §2's table (line 99) calls `L+` **"Bland's rule proper — the form the classical finiteness argument actually covers"**, and the header said the same | **It is a Bland-style ANALOGUE and the theorem does not cover it.** Corrected in `qp_anti_cycling_arm.h` and at `src/qp/qp_engine.cpp`'s drop hook; RULE.md keeps its wording because RULE.md is frozen | No |
| I4 | the replay build's **third** declared difference was never entered into RULE.md | the "Frozen-rule deviations" section below | No |
| I5 | `--negligible-pivot` was accepted and **printed as a realized parameter that the implementation never read** | `arm_candidates` reads `params.negligible_pivot`; its default is `detail::kEngineDenomTol` by a `static_assert`, and a fixture sets a non-default value and asserts the eligible set changes | **No.** Every leg here ran the default, which is the constant the old code used unconditionally |
| M1 | the thread fixture was sequential, not overlapping | a barrier-based fixture: two solves released together, one armed, one not | No |
| M2 | covered by I3 | — | — |
| M3 | RULE.md §3.1 (line 150) and the probe's header say **"the eight EXPAND/lexicographic parameters as arguments"**, and the arm's parameters were called "the paper's" | **There are SEVEN parameters and the probe exposes seven arguments** (`--delta-master`, `--delta-0-factor`, `--delta-max-factor`, `--expand-k`, `--negligible-pivot`, `--reset-nontrivial`, `--lex-full-pricing`). They are **paper-derived and ADAPTED**, not the paper's throughout: `delta_master` deliberately replaces the paper's ~1e-6 with hven's `feas_tol` default and `negligible_pivot` is hven's own engine threshold. Corrected in `qp_anti_cycling_arm.h`; RULE.md and the pinned probe keep their wording | No |
| M4 | `tables/readouts-m4.md` cited a manifest file that does not exist | `traces-m4/SHA256-parent-traces-m4.txt`, in the table and in the generator that writes it | No |

**Two scope corrections to this README's own text.**

* Finding 4 above and the "What this does NOT show" list say arm L is *vacuous
  on hven's drop rule*, citing `arm_lex_drop_switches = 0` on both cycling
  fixtures. **That is a statement about those two fixture trajectories, not
  about hven's drop rule.** On field cells L is not vacuous on the drop side at
  all: it records **13** drop switches on READING6 and **31** on ZAMB2-8
  (`rows/L-READING6.txt`, `rows/L-ZAMB2-8.txt`). The fixtures now assert
  `lex_drop_switches == 0` and `lex_ratio_switches > 0` **separately**, with
  that scope written beside them.
* Arm E is **the EXPAND-style implementation pinned in this round's binary**,
  not canonical EXPAND. The reset defect (I1) and the ride defect (I2) are the
  two places the pinned binary and the paper part company, and both are named
  wherever this round's E results are cited.

**Registered, not done here: a measurement of the corrected arm.** Nothing in
this directory and nothing in M4b is re-measured. The corrected arm is a
different binary and would need its own rule; running it on either population
is a NEW task and is registered as one. Two M4b rows would be the first to
move if it ran: `E-SAWPATH` (`arm_resets=3`) and `E-BRITGAS` (`arm_resets=4`)
are the only rows in either directory that entered the reset path, and they
did so under the defect.

---

## Frozen-rule deviations

RULE.md is frozen from before its first leg, and one difference that governed
the build was never entered into it. It is recorded here because a reader who
verifies the instrument against RULE.md would otherwise find the transcript
saying something the rule does not.

**RULE.md §3.2 declares `-DHVEN_TESTING=1` and nothing else, plus the PCH path
as "the only other declared difference". The replay adds a THIRD:
`-I<source>/include` prepended to every probe-side TU's includes.**

* **What it is.** The probe's own TU and the three bridge TUs read hven's
  headers from the INSTALL's flattened tree, and that install was taken at
  M3c's HEAD — before this task's arm header existed. Without the prepend the
  probe cannot see `qp_anti_cycling_arm.h` at all and cannot arm the switch it
  exists to measure.
* **When it was declared.** In `wrappers/replay_build_m4.py`'s
  `check_include_equivalence`, committed at `325765d3` (2026-09-22T07:27:57-04:00),
  and printed into the harvest — both **before** the first leg at
  `07:28:06-04:00`. It was declared in code and in the transcript; what it was
  not is an **amendment to RULE**, which is what AMENDMENT 1 (§5.5a) was and
  what this should equally have been.
* **What bounds it.** The source and installed header trees are compared file
  by file before the flag is used, and the difference set must be EXACTLY this
  round's own `git diff --name-only <base>..<head> -- include/`. The comparison
  found **2 differing and 129 identical** headers, which are exactly the two
  this round added or touched; any other differing or missing file REFUSES.
  So the binary has ONE header view — the source tree's — rather than a mixed
  one, which is the shape ODR violations come in.
* **Why it is recorded here and not in RULE.md.** A rule edited after its legs
  have run is not a pre-registration. The honest record of a rule that was
  contradicted is the contradiction, stated, beside the rule — not a rule
  rewritten to agree with what happened.

---

## What is in this directory

| path | what |
|---|---|
| `RULE.md` | the rule, committed before the first leg, **unedited** |
| `verdict.txt` | §8's outcome per arm, with every differing field, from the transcripts |
| `tables/arm.md` | the OFF-control line for the ten; the three neutrality cells × four arms on every §7.1 field; the gate's own verdict table. Machine-written from `rows/` |
| `tables/readouts-m4.md` | POST-HOC §6 read-outs over the twelve neutrality traces: the predicate per solve, the totals, the two close-out questions, the per-record walk comparison, and §6.6's read-outs on every RECURRENT solve |
| `rows/` | all 22 rows the chain wrote (10 OFF controls + 3 cells × 4 arms) |
| `logs/` | `chain.log`, the four gate transcripts, every `.err` (all empty), `armcheck.txt`, `passed-cells.txt`, `stopped-arms.txt`, `refusal.txt`, and Task B's `gate-smoke.txt`, `binary-m4.txt`, `harvest-diff-m4.txt` |
| `gates/` | Task A's gate record and its README, plus this close-out's `neutral-posthoc-no-traceout-{E,L,L+}.txt` and `default-unchanged.txt` |
| `traces-m4/` | 69 per-solve slices of the neutrality traces (every RECURRENT solve, and every first-divergence solve, under every arm that has it), `SHA256-slices-m4.txt`, and `SHA256-parent-traces-m4.txt` for the 22 flat traces, which are 247 MB and are **not** committed |
| `wrappers/` | the probe, the replay build, the gate, the chain, the classifiers, and this close-out's `readouts_driver_m4.py` and `table_arm_m4.py` |
| `rows-smoke/` | the §3.3 smoke row |

Every count in every table above is read from a named raw transcript in this
directory. No number was retyped from a summary, and an exit code is nowhere
treated as a result.

---

## Where this goes next

**M4b** — `docs/notes/data/2026-09-m7-w2-m4b-expand-on-the-ten/` — takes **arm E
alone** to §1.1's ten cells, under its own rule committed before its first leg.
L and L+ are excluded by name, with finding 2 above as the reason. The neutrality
bar is replaced there by a descriptive report, because §1.2's bit-equality bar
is unmeetable by an arm that replaces the ratio test, and the 0.81 / 0.98 / 1.49
minors ratios measured here are written into that rule in advance as the declared
caution that E can lengthen a walk as readily as shorten one.
