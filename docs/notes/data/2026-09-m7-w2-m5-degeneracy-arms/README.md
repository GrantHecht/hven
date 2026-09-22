# M7 W2 M5 — the degeneracy arms on the cells `hven-sqp` fails

**The result, in plain language.** We ran two experimental changes to the QP
walk — call them *arm a* (judge a drop's multiplier against the pass's own
stationarity scale instead of against an absolute tolerance) and *arm b1* (never
add a constraint that is linearly dependent on the ones already active; pass
over it and take the next candidate in ratio order) — on the CUTEst problems the
SQP solver currently fails at default settings, plus three problems it solves,
as a neutrality control. **Neither arm fixed anything. Arm b1 broke two of the
three problems that previously worked, and on most problems it could not
legally run at all.** Nothing here is a production change: both arms live behind
a test-only switch, and the gate record for the head that ran these legs
(`gates-run2/`) measures the production library at 42/42 objects byte-identical
in Release and 41/42 in Debug, the one difference being debug sections only.

Three things are worth a reader's attention, and all three are *observed*:

* **Arm b1 mostly refused to arm.** Its own safety rule is that the active
  constraints must be linearly independent before it may touch anything. On 9 of
  the 11 problems it ran on, that rule tripped — and on `SAWPATH` and `SMBANK`
  it tripped on *every single* QP subproblem, so the armed run is byte-for-byte
  the unarmed run (the trace files have identical sha256). The reason is the
  interesting part: the deficiency sits in rows the arm is *forbidden* to touch
  — the equality Jacobian and the fixed bounds. See Q1 below.
* **Where it did run, it hurt.** `READING6` and `ZAMB2-8` went from `optimal` to
  `numerical_error`, exiting at points 2.2 and 2.3 away from satisfying their own
  equality constraints. Under RULE.md §8 step 4 that is HARM, and it stops arm
  b1's and arm a+b1's verdicts being quoted at all.
* **The recurrence it was built to break did not break.** The pre-registered
  prediction was that the drop→re-add unit count would fall to ~0 on the eight
  class-(b) cells. It did not fall on any of them (ratios 0.76 to 2.02 of the
  control), so b1 is refuted as the cause on every cell where the question could
  be asked — with the caveat that every one of those cells is also UNSUPPORTED
  or TRUNCATED, so the refutation is as unquotable as the confirmation would
  have been.

Arm a is a smaller story: it changed the walk a great deal (tens of thousands of
refused drop candidates) and emptied a drop-candidate set that the absolute test
would have left non-empty **7 times on `YORKNET` under `a`, 4 times on
`ZAMB2-8`, at most once anywhere else and not at all on most cells**. On
`YORKNET` — the one cell where RULE.md §4's R1 allowed the "terminates optimal
under a" claim to be made at all — it turned `infeasible` into
`numerical_error`. **That claim is therefore NOT MADE.**

The governing rule is [`RULE.md`](RULE.md), committed with AMENDMENT 1 before
the first leg ran and **frozen from 2026-09-22T14:14:50-04:00**. It was not
edited by this read-out. The mechanical application of §§4, 5, 7 and 8 is
[`verdict.txt`](verdict.txt); every number behind it is in
[`tables/arm.md`](tables/arm.md).

---

## 0. What this is about, and the words it uses

The subject of every claim here is **arms a, b1 and a+b1 as implemented in the
pinned M5 probe**:

    .scratch/m7w2m5arm/build/m5_arm_probe
    sha256 eac445c0256e1b0af739f98e91a5d1cce586fedfa41562b7fb08684b145b43d4

Never "the independent-add rule" in the abstract, and never the **exchange**,
which RULE.md §1 records as REGISTERED AND UNBUILT: A4 dropped it for this
round and fixed its reference to Gill & Wong (2015) §4's multiplier-aware form.
**No verdict in this round is about the exchange.**

`OBSERVED` and `INFERRED` are used as labels below and mean what they say. Every
observed number is a counter, a status or a field of a committed row; every
inferred one is an argument from those, and it says so.

**Wall-clock is informational throughout** (CLAUDE.md §7). The legs ran one at a
time, one lock hold each, `taskset -c 2`, `MKL_NUM_THREADS=1`, but no claim in
this directory rests on a timing and none is quoted as a measurement.

**No "production-ready" wording appears anywhere** (A7), and this result is not
one where the question arises.

---

## 1. The formal outcome

Applied mechanically in [`verdict.txt`](verdict.txt); tabulated in
[`tables/arm.md`](tables/arm.md).

    CONVERTED               0
    improved, unverified    0
    HARM                    1   (YORKNET under arm a)  + 4 neutrality HARMs
    none                    3
    TRUNCATED               4   (b1/a+b1 x SAWPATH, a+b1 x STEENBRC, STEENBRG)
    UNSUPPORTED            20

    arm b1   :  9 of 11 cells disarmed  -> UNSUPPORTED AS A WHOLE (A9)
    arm a+b1 : 11 of 13 cells disarmed  -> UNSUPPORTED AS A WHOLE (A9)
    arm b1, a+b1 : status changed on READING6 and ZAMB2-8
                   -> HARM (§8 step 4); those arms' verdicts are not quoted

28 legs were attempted, 28 rows survive, **0 CENSORED**. The chain is
[`logs/chain-m5.txt`](logs/chain-m5.txt) (pid 3123140, 14:14:38 → 15:29:36).

`indep_enumeration_mismatches` is **0** on every armed leg and
`scale_unavailable_passes` is **0** on all 28, so RULE.md §7 deviation 4's
invalidation never fires and no cell's read-out is thrown out by either counter.

---

## 2. The three questions this read-out owed, answered

### Q1 — where does the rank deficit come from, and how often did the arm refuse at entry?

**It comes from the MANDATORY rows — the ones the arm may not touch.**
*(OBSERVED, then INFERRED; the inference is stated.)*

The entry repair builds a mandatory skeleton first: the equality rows (implicit
in `qp.me()`) plus every FIXED bound. If that skeleton is *itself* rank
deficient, no repair can reach the invariant, so the arm records
`indep_refused_mandatory_dependent`, disarms and runs the solve unarmed. That
refusal fired on six cells, at these fractions of that cell's solves
(OBSERVED, `tables/arm.md` §5):

| cell | refusals / solves | max(me+\|W\|−n) seen | max rank deficit |
|---|---|---|---|
| `SMBANK` | **29 / 29 = 1.00** | −37 | 1 |
| `SAWPATH` | **11 / 11 = 1.00** | −1 | 878 |
| `DALLASM` | 5 / 8 = 0.62 | 0 | 2 |
| `STEENBRC` | 6 / 22 = 0.27 | −9 | 37 |
| `STEENBRG` | 6 / 23 = 0.26 | −31 | 37 |
| `STEENBRB` | 5 / 23 = 0.22 | −11 | 45 |

The refusal has two possible causes in the code — more mandatory rows than
variables, or mandatory rows that are linearly dependent. **On all six cells
`max(me+|W|−n) ≤ 0` over the whole solve, and the mandatory skeleton is a
subset of the set that check ran on, so the row-count cause is excluded and the
cause must be genuine linear dependence among equality rows and fixed bounds.**
*(INFERRED, from the counter and the code path; the counter alone does not
distinguish the two, and this artifact does not evaluate the Jacobian itself —
no solver ran here.)*

The other half of the deficit story is the six cells that disarmed on a rank
violation *away* from entry (`ACOPP118` 5, `ACOPP57` 1, `ACOPR57` 14, `BRITGAS`
5, `STEENBRG` 1, `a+b1-YORKNET` 1). RULE.md §5 already names the two
UNCOVERED-BY-FIXTURE sites this is expected to catch, and the recorded
last-solve disarm site on `ACOPP118` and `ACOPR57` is **site 5, the repair
pin** — which is where the violation was *caught*, not necessarily where it was
*caused*. *(OBSERVED for the site; the caught-not-caused distinction is
RULE.md §5's own and is repeated here so the column is not over-read.)*

**Registered as an ENGINE question, per the dispatch's condition:** *is the
engine assembling rank-deficient mandatory rows — a rank-deficient equality
Jacobian, or fixed bounds that duplicate an equality — on `SMBANK` and
`SAWPATH`, on every subproblem?* It is not a property of the arm; the arm only
made it visible, exactly as it did with C7. Nothing here fixes it and nothing
here is a claim about what the engine should do instead.

### Q2 — on READING6 and ZAMB2-8, which b1 mechanism acted before the status broke?

*(All OBSERVED, from `tables/arm.md` §§3, 4, 6 and 9.)*

**`READING6`.** Four passes, 200 candidates examined, **200 passed over**, split
entry-removals 0 / screen-and-QR 0 / **QR-only 400** dependency verdicts (400,
not 200, because the split counts every site's verdict: 200 ordinary pass-overs
plus 200 repair-pin refusals — the identity is checked in `tables/arm.md` §4),
and `indep_no_blocker_passes = 4` — **every pass ended with no independent blocker
at any ratio, so the walk took the full step, four times out of four.** 188 of
those pass-overs exceeded A10's bound; the largest post-step violation of a
passed-over bound is **6.24**. The run reached 8 minors of a control's 3833 and
returned `numerical_error` at `feasibility_e = 2.218`. So the mechanism is the
no-independent-blocker branch of §5's rule, and it acted before anything else
could.

**`ZAMB2-8`.** 1769 passes, 7117 examined, **5456 passed over** (screen-and-QR
3128, QR-only 3387, entry removals 0), `indep_no_blocker_passes = 108`, **3617
A10 exceedances**, and a largest passed-over violation of **4.41e+09**. The run
returned `numerical_error` at `feasibility_e = 2.34`, `feasibility_i = 2.84`.

**Does the HARM come with an A10 exceedance or an INFEASIBLE-EXIT? BOTH, on
both cells.** Each is UNSUPPORTED under A10 and flagged INFEASIBLE-EXIT under
§5.1, and unlike the eight failing cells their OFF controls exit perfectly
feasible — so on these two the flag is an arm effect and says so in
`verdict.txt`'s `+OFF` column.

**And AMENDMENT 1's `cond(R)` does NOT explain `READING6`.** The pre-registered
reading beside every exceedance is the QR's own conditioning. On `READING6` the
per-solve high-water mark is **`indep_max_cond_r` = 2.94** — a *well-conditioned*
active set — beside 188 exceedances and a 6.24 violation. The ill-conditioning
story that C4 pre-registered as the likely explanation is available on
`ZAMB2-8` (6.9e+04), `BRITGAS` (5.0e+07) and the `STEENBR*` cells (0.8–4.0e+07),
but it is **not available on `READING6`**, and the rule's own words apply: it
EXPLAINS an exceedance where it is large and NEVER EXCUSES one — the solve stays
UNSUPPORTED either way. A reader should take the opposite reading too: a small
`cond(R)` does not make an exceedance benign. Separately, `STEENBRG` under b1
shows **2839 exceedances whose largest violation is 3.78e−12** — A10's bound is
relative, so it can be exceeded at an absolutely negligible violation, and an
exceedance count on its own is not a magnitude. *(OBSERVED.)*

### Q3 — where b1 stayed armed, did the core drop→re-add units fall?

**No. And the only two cells where it stayed armed for the whole run are the
two it broke.** *(OBSERVED, `tables/arm.md` §8; counts from the COPIED
`wrappers/m4c_tails.py`, run per trace, never re-derived by hand.)*

Core drop→same-bound-re-add units, arm ÷ OFF, on the eight class-(b) cells:

| cell | b1 | a+b1 |
|---|---|---|
| `ACOPP118` | 1.000 | 1.000 |
| `ACOPP57` | 1.003 | 1.003 |
| `ACOPR57` | 0.759 | 0.759 |
| `DALLASM` | 1.003 | 1.003 |
| `SAWPATH` | 1.000 (the arm never armed) | 1.000 |
| `STEENBRB` | **1.523** | **1.519** |
| `STEENBRC` | 1.014 | 0.263 (TRUNCATED at 21 % of OFF's minors) |
| `STEENBRG` | **2.021** | 0.544 (TRUNCATED at 22 % of OFF's minors) |

RULE.md §5's pre-registered prediction was "falls to ~0 … **if it does not, b1
is REFUTED as the cause on that cell**". It does not fall on any cell, and on
two it roughly doubles.

**Is any of it quotable? No.** Applying UNSUPPORTED first: all eight cells carry
at least one of A9, A3 or A10, four legs are TRUNCATED besides, and A9's
more-than-half rule makes **both arms UNSUPPORTED AS A WHOLE**. The two cells
where b1 held its invariant end to end — `READING6` and `ZAMB2-8` — are the two
whose status it broke, so their core-unit numbers (0 and 0.685) measure a walk
that died early, not a recurrence that was cured. **There is no cell on which
arm b1's effect on the recurrence is measured under conditions this rule calls
supported.**

---

## 3. A6's prediction, both halves

**OFFLINE — HOLDS.** *(OBSERVED; `logs/a6-offline.txt`, run before the dynamic
leg so the prediction was on the record before the solver moved.)* Over M4's
pinned `off-YORKNET.csv`, 2538 core drops examined — the exact number A6
predicted, cross-checked against `m4c_tails.py`'s own count rather than taken on
trust — the scaled test **refuses 2538 and admits 0**, no pass lacked a
published scale, and `|λ|/(opt_tol·scale)` is 1.84849e−10 at min, p50, p90 and
max.

**DYNAMIC — NOT REFUTED, AND NOT CONFIRMED: "diagnosis untested by this
trajectory".** *(OBSERVED.)* A6's refutation condition is the same candidate
identity still being dropped, or the same matched recurrence persisting. Under
arm a, `YORKNET` carries six recurrent tails releasing `bound:93:lower`,
`18:upper`, `212:upper`, `479:lower`, `236:upper`, `249:lower` — **the
intersection with the control's two cycling identities (`bound:393:lower`,
`bound:1:lower`) is EMPTY**, the tails sit at different solve indices and carry
different M4c classes. A6 rules that a different tail surviving is NOT a
refutation, and it is reported in A6's own words.

---

## 4. The cost of the QR, declared and then observed

RULE.md §3 declared the cost before the legs: **one dense rank-revealing QR of
an `n × (me+|W|)` matrix per pass, `O(n·(me+|W|)²)`, plus one projection per
candidate examined and one least-squares solve per candidate called dependent.**
It also named `SAWPATH` and `BRITGAS` as the cells most likely to be censored by
it.

What was observed (`tables/arm.md` §10): `STEENBRC` at `n = 540`, `me = 126`
took **8158 QR builds** under b1 (2.26 per pass) and **12130** under a+b1;
`STEENBRB` 12577 and 12610; `BRITGAS` 1936 over 16265 passes. Four legs fired
their deadline: both `SAWPATH` legs (300 s declared) and `a+b1-STEENBRC` and
`a+b1-STEENBRG` (600 s), whose OFF controls are **not** censored.

On the two STEENBR cells the a+b1 legs reached **one major and ~21 % of the
control's minors** inside the same budget while taking ~1.5× the QR builds their
b1 siblings took. *(The counts and the statuses are OBSERVED; that the QR is
what consumed the budget is INFERRED from those counts together with the
informational wall-clock — 612 s and 608 s against a 600 s deadline — and it is
not a timing measurement and not quoted as one.)*

---

## 5. What this does NOT show (RULE.md §9, unchanged)

* Not that either arm ships. Both are experiments on a test seam; the production
  forms — a cheaper dependency test than a per-pass dense QR, and the Gill &
  Wong exchange — are registered design items and neither is built.
* Not that the drop/re-add recurrence has ONE cause. M4c classified four
  populations; these arms address two of them.
* Not a timing claim of any kind.
* Not, for arm a, "terminates optimal" from a fixture — only from `YORKNET`, and
  only if leg 2 showed it (R1). **Leg 2 did not: `infeasible` → `numerical_error`.
  The claim is not made.** And not a zero dynamic core count (A6).
* Not, for b1, a claim about the exchange, which is not built.
* Not an "O(μ)" claim about the leak: RULE.md §2's sweep already refutes it below
  `dual_mu = 1e-8` on the fixture.
* Not "production-ready", in those words or any paraphrase (A7).

One more limit, belonging to this round's instrument rather than to the rule.
**The probe exports the arm's PER-SOLVE counters and not its PER-ADD trace
columns.** `add_b1_cond_r`, `add_b1_passed_a_dot_p` and the rest of A5's leak
columns are recorded by the seam on the walk-trace record, but the probe's flat
trace is M3b's column set and does not carry them, so RULE.md §6's per-add
read-outs — the projection-residual distribution per cell, the per-pass rank
pair, the per-exceedance `aᵀp` decomposition — **are not in this artifact.**
What IS here is the per-solve form of each: `indep_max_cond_r` beside every
`indep_leak_bound_exceeded`, `indep_rank_deficit_max` and
`indep_rows_minus_n_max` for the invariant, `indep_max_passed_violation` for the
hazard. Stated as an absence rather than left for a reader to notice.

Likewise §5.1's "the returned point's own primal infeasibility, recomputed from
the problem" is **not** recomputed here: the `feasibility_e`/`feasibility_i` this
artifact reads are the solver's own, on rows and general inequalities, and they
cannot see a violated BOX — which is precisely the blind spot C7 names below.
The only box reading available is the arm's own
`indep_max_passed_violation`, and it is in `tables/arm.md` §6.

---

## 6. The registered items

1. **C7 — the engine certificate (ENGINE item, not fixed here).** The optimality
   certificate does not check the returned iterate's primal BOUND feasibility.
   The site, as C7 requires it be named: `src/qp/qp_engine.cpp`, the
   would-be-optimal exit of the drop loop — the branch tests
   `detail::free_block_is_stationary(free_stat, stat_scale, eff_opts)` (~:956)
   and on success sets `status = QpStatus::kOptimal` (~:990) with no reading of
   the returned `x` against its own bounds. Everything the branch consults is
   DUAL; the primal side it does consult is the WORKING SET's residual, not the
   box. On the scale-carrier fixture it returned `kOptimal` at a point
   **5.59e−03 outside its box**. It is **not contingent on the arm**: an engine
   that certifies a point it has not checked is a property of the engine, which
   the arm merely made visible.
2. **The production exchange — REGISTERED AND UNBUILT (A4/C5).** Gill & Wong,
   *Math. Prog. Comp.* **7** (2015) §4's multiplier-aware exchange, **not**
   "release the largest dependency coefficient", which the Codex review showed
   is not theorem-backed. RULE.md §1 carries the three reasons R3's
   implementability condition fails in this walk.
3. **Rank-deficient MANDATORY rows — a new ENGINE question, raised by Q1.**
   On `SMBANK` (29/29 solves) and `SAWPATH` (11/11) the equality rows plus fixed
   bounds are linearly dependent on every subproblem. Registered as an engine-level
   question, with the caveat on its inference stated in Q1.
4. **Arm b1's own precondition is the finding, not a nuisance.** Any future
   independence-based add rule inherits the same entry condition, so the
   question in item 3 is on its critical path: an arm that may not touch the
   rows that carry the deficiency cannot run on these problems at all.

---

## 7. What is in this directory

| path | what it is |
|---|---|
| [`RULE.md`](RULE.md) | the pre-registered rule, with AMENDMENT 1. Committed before the first leg, **frozen**, not edited by this read-out |
| [`verdict.txt`](verdict.txt) | RULE.md §§4, 5, 7 and 8 applied mechanically, with the precedence declared before the table |
| [`tables/tails-m5.json`](tables/tails-m5.json) | `m4c_tails.py`'s own output for all 28 arm traces and the four OFF traces this round ran it on, so §8's core-unit table is auditable without re-running anything |
| [`tables/arm.md`](tables/arm.md) | every read-out §6 asks for that this probe exports: the compared fields, arm A's five counters, b1's counters, the three-mechanism split, the rank invariant, the exceedances-against-`cond(R)` column, which site acted, the core-unit counts, neutrality, and A11's cost |
| `rows/` | the 28 surviving leg rows, as the chain wrote them |
| `rows-off/` | M4's thirteen committed OFF rows, COPIED with their manifest and re-verified by the chain's STEP 0 — never re-run |
| `logs/` | the chain transcript, A6's offline transcript, and the OFF-trace pin check |
| `traces-m5/` | 13 committed slices with `SLICES.txt`; every parent pinned by sha256 in `SHA256-parent-traces-m5.txt` |
| `gates/`, `gates-run2/` | the two gate records; run 1 is the record for `0c6709ce` and run 2 for the AMENDMENT 1 head. Untouched by this read-out |
| `smoke/` | the OFF-row reproduction: ACOPP57 at `--arm off`, 58 of 58 fields by BYTES, 0 near-ulp acceptances |
| `wrappers/` | the probe source, the chain, the gate scripts, the COPIED `m4c_tails.py`, and this round's read-out driver, verdict driver and slicer |
| [`MANIFEST.sha256`](MANIFEST.sha256) | sha256 of every file committed here |

Nothing under `src/`, `include/` or `tests/` was touched by the chain or by this
read-out, and no solver ran to produce any file in §§1–7 above.
