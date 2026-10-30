# M7 W2 M4b — arm E on the ten cells `hven-sqp` fails at DEFAULT options

**The result, in plain language.** We ran hven's EXPAND-style anti-cycling
experiment — call it *arm E* — on the ten CUTEst problems the SQP solver
currently fails at default settings, and it **fixed none of them**. All ten
ended exactly where they ended without it: eight with a numerical error, one
declaring the problem infeasible, one cut off by its time budget. The arm was
not idle — it changed the step the solver took tens of thousands of times per
problem — but the final answer never moved. On one problem, `ACOPP57`, it did
tens of thousands of things differently and produced a results row that is
**identical to the untouched run on every one of the 85 fields compared, to the
last bit**.

The trace says why, and the reason is specific and useful. At a degenerate
corner the solver drops a bound, immediately finds its search direction
pointing straight back through that same bound, and re-adds it — every time,
through the ordinary ratio test. Arm E's whole job is to replace that zero-length
step with a tiny positive one, and **it does: the step is taken, the iterate
moves.** But on the very next pass the solver recognises a KKT point of the
current working set and snaps the iterate onto that set's exact solution, which
puts the bound back exactly where it was. The tiny move is erased one pass after
it is made, before the next ratio test can ever see it. Two passes later the
state is bit-for-bit what it was. *(Observed. The inference drawn from it is in
§4, labelled there.)*

---

## 0. What this is about, and the words it uses

The subject of every claim in this directory is **arm E as implemented in the
pinned M4 binary**:

    /home/ghecht/Projects/hven/.scratch/m7w2m4/arm/build/m4_arm_probe
    sha256 c1be17d2d47e7930440450d72d233da56a8fc374bceb68062baea924fe50a720

**Never "EXPAND" and never "canonical EXPAND."** Two departures from Gill,
Murray, Saunders & Wright (1989) live in that binary; §5 names them, says where
each one bit on these ten cells, and points at the counters that show it. A
statement about this binary is not a statement about the paper's algorithm.

The governing rule is [`RULE.md`](RULE.md), committed before the first leg ran
and not edited since. It inherits M4's `RULE.md` §§6.2–6.6 (the per-solve
predicates) and §§7.2–7.7 (the per-cell verdicts) **by reference**, read out of
that file rather than retyped, so no drift between two copies is possible.

**This task is a follow-up experiment under a new rule. It is not a successful
continuation of M4's pre-registered primary path, and it does not reclassify
M4's result.** M4 stopped arms E, L and L+ at its own bit-equality neutrality
bar; that STOP stands, untouched. What M4 never did was run E on the ten. That
is the gap this closes.

---

## 1. The formal outcome

Applied mechanically in [`verdict.txt`](verdict.txt); tabulated in
[`tables/arm.md`](tables/arm.md).

    CONVERTED               0
    improved, unverified    0
    HARM                    0
    none                    9    (rank(E) == rank(OFF) on every ranked cell)
    TRUNCATED               1    (SAWPATH -- deadline_fired=1)

Terminal status is **identical to the OFF control on all ten**: eight
`numerical_error`, `YORKNET` `infeasible`, `SAWPATH` `interrupted`. `verdict.txt`
carries the rank arithmetic.

Three things are recorded there rather than glossed:

* **M4 §7.5's second `HARM` clause is `N/A` on these ten, because OFF solves
  none of them.** Stated, as §7.5 instructs, not printed as a vacuous
  "0 harmed". It matters: that clause would otherwise have caught `YORKNET`,
  whose minors go 50241 → 67259 (1.3387×). It does not, because `YORKNET` is
  not a cell OFF solved.
* **`opt_feas_tol` is ABSENT from this probe's row**, so §7.3's second condition
  has no input and was evaluated on no cell. It is moot — §7.3's *first*
  condition fails on every cell — but a reader must not believe a bar was
  applied that was not. The same is true of §7.1's `restoration_h_in/out`,
  `restoration_f_in/out` and `qp_failures_in_a_row_peak`.
* **`SAWPATH` is censored on BOTH sides.** Its OFF control also carries
  `deadline_fired=1`. Its 0.7737 minors ratio is two truncations of two
  different walks at the same wall-clock stop, not a saving.

**§6's own control holds.** `arm_lex_drop_switches` is 0 on all ten E rows, as
`RULE.md` §6 requires of an arm that touches no drop rule, and
`arm_lex_ratio_switches` is 0 as well. There is no instrument FINDING.

---

## 2. What the arm actually did

Full per-cell tables in [`tables/arm.md`](tables/arm.md); per-solve read-outs in
[`tables/readouts-m4b.md`](tables/readouts-m4b.md).

| | |
|---|---|
| minors ratio E/OFF | 0.7630 (`BRITGAS`) … 1.3387 (`YORKNET`) |
| shortened / lengthened / unchanged | 7 / 2 / 1 |
| `arm_min_steps` per cell | 3000 … 29897 |
| `arm_pivot_switches` per cell | 0 (`ACOPP57`) … 20409 (`YORKNET`) |
| RECURRENT solves, summed over the ten | OFF 52 → E 45 |
| cells with NO recurrent solve under E | **0 of 10** |

**`ACOPP57` is the sharpest single fact in the task.** Its E row and its OFF row
are byte-identical on every compared field — same `f` to the last digit, same
7248 minors, same 128 factorizations, same 7 majors — while `arm_min_steps` is
3555 and `arm_pivot_switches` is **0**. The row gate cannot tell the two runs
apart once the trace path is set aside
([`logs/compare-E-vs-off-no-traceout.txt`](logs/compare-E-vs-off-no-traceout.txt):
REPRODUCES on 85 fields, **0 near-ulp acceptances**), yet **7156 of the 7248
per-minor records differ**. *(Observed.)* The arm changed the step length at
nearly every minor and changed which constraint entered at none of them, and the
walk arrived at exactly the same place. That is the cleanest statement this task
has of the difference between "inert" and "invisible at the row".

---

## 3. §5's replacement of the neutrality bar, argued in full

M4 §5.5 required an arm to reproduce its own `off` leg **bit for bit** on three
cells `hven-sqp` already solves. E failed on all three, and M4's close-out
established from the traces *why*: E does not only decide ties. It admits
constraints against a slowly growing feasibility bound and takes a largest-pivot
second pass, so it changes which constraint enters even where nothing is tied.
On `SMBANK` — the one neutrality cell tie-free on every printed scalar — E's
first differing per-minor record is minor 6 of solve 0, in `alpha`, `dx_inf` and
`raw_ratio`: the ratio test's own output, on a step with no tie.

A bit-equality bar is therefore **unmeetable by an arm of this shape, for
reasons that have nothing to do with whether the arm is any good.** Keeping it
would have stopped this task before it started, for the second time, on the same
structural fact.

**`RULE.md` §5 replaced it; it did not lower it.** What replaced it —
[`tables/neutrality-descriptive.md`](tables/neutrality-descriptive.md) — asserts
nothing about equality, cannot be passed by accident, and had to be carried
whatever it said. Those three cells **receive no verdict**. They are context for
reading the ten, not a gate in front of them. The replacement was declared in
`RULE.md` before any leg ran.

**§5.3's declared caution, written before the ten ran, HELD.** It said, from
ratios of 0.8137 / 0.9820 / 1.4922, that *"E can lengthen a walk as readily as
shorten one."* On the ten: seven shorter, two longer, one exactly 1.0000. No leg
was a surprise in either direction, and no leg is read as the arm's general
property.

---

## 4. The decision-relevant read: by what route does the released bound come back?

**POST-HOC.** `RULE.md` §6 pre-registers M4 §§6.2–6.6 for these ten cells. It
does **not** pre-register this question, which was asked after the chain had
run. It is answered only from traces already taken. No leg was re-run, no solver
ran, and nothing in `verdict.txt` rests on it. Machinery:
[`wrappers/readd_route_m4b.py`](wrappers/readd_route_m4b.py) →
[`tables/readd-route-posthoc.md`](tables/readd-route-posthoc.md), and
[`wrappers/acopp57_period_m4b.py`](wrappers/acopp57_period_m4b.py) →
[`tables/acopp57-tail-period-posthoc.md`](tables/acopp57-tail-period-posthoc.md).

### 4.1 OBSERVED — the route, across all ten cells

Every number in this subsection is a count or a value read out of a trace.

* **The re-add is through the RATIO TEST, on the ordinary blocking-constraint
  add path.** Over the whole of all twenty flat traces, every single add that
  re-admits exactly the identity the preceding drop released carries
  `drop_route=none` — the add came through the ratio test's blocking
  constraint, not through any drop or re-pin route. Under E that is 3552 of
  3552 on `ACOPP57`, 29201 of 29201 on `BRITGAS`, and 100 % on every other cell.
* **Under OFF that step is exactly zero.** `alpha` is exactly `0` on 3552 of
  3552 `ACOPP57` re-adds, and on essentially all of them on every cell
  (`tables/readd-route-posthoc.md`, column "α exactly 0").
* **Under E the step is exactly the EXPAND floor.** On 3551 of 3552 `ACOPP57`
  re-adds, `alpha` is bit-equal to `tau / |p_released|` in IEEE double at this
  leg's own printed `arm_tau = 4.9e-14`. Across the ten, that identity holds on
  between 1630 (`ACOPR57`) and 23456 (`BRITGAS`) re-adds per cell; the
  remainder carry some other positive step, never zero.
* **The `alpha_min` step is actually taken. Δx ≠ 0.** On every one of those
  re-adds `dx_inf` is non-zero: 3552 of 3552 on `ACOPP57`, and the "Δx ≠ 0"
  column equals the "re-adds the same identity" column on all ten cells. Under
  OFF the same column is 0 or 1.
* **Every drop pass in every run satisfies `‖p‖∞ ≤ step_tol`** — 3574 of 3574 on
  `ACOPP57`, and 100 % on all ten cells under both arms.

### 4.2 OBSERVED — one full period of `ACOPP57`'s tail, record by record

`ACOPP57` solve 6, the run's last QP: 1795 minors, `exit_status=max_iter`, a
maximal exactly-periodic suffix at **period 2** covering 1759 passes (97.99 %)
under E and 1760 (98.05 %) under OFF. Minors 1780 and 1781 are well inside it.
Every column of both records, under both arms, is in
[`tables/acopp57-tail-period-posthoc.md`](tables/acopp57-tail-period-posthoc.md);
72 of the trace's 74 columns repeat byte-for-byte two minors later under **both**
arms.

**Record 1 — minor 1780, an `add` pass.**

| | `off` | `E` |
|---|---|---|
| `action`, `block` | `add`, `bound:14:lower` | same |
| `drop_route` | `none` | same |
| `ws_size_in → ws_size_out` | 21 → 22 | same |
| `ws_sig_in → ws_sig_out` | `13695891117004593176` → `7832398285949506436` | same |
| `p_inf` (‖search direction‖∞) | `3.3341635608911957e-06` | same |
| `p_released` (direction through the just-released bound) | `-6.993856282166977e-07` | same |
| `degenerate`, `ratio_tied` | 1, 1 | same |
| `x_inf`, `x_l1` at entry | `0.125`, `7.1252949701126038` | same |
| **`alpha`** | **`-0`** | **`7.0061491147510168e-08`** |
| **`raw_ratio`** | **`-0`** | **`7.0061491147510168e-08`** |
| **`dx_inf`** | **`-0`** | **`2.3359647080572948e-13`** |

The bound that is added is `bound:14:lower` — the same one the previous pass
dropped. `p_released` is negative and `released_directional` is
`+6.993856282166977e-07`: the search direction runs straight back through the
released bound, so that bound blocks at ratio zero and is the blocking
constraint. Four identities, each re-derived in IEEE double and **bit-equal**:

    alpha            == tau / |p_released|        7.006149114751017e-08
    dx_inf           == alpha * p_inf             2.335964708057295e-13
    alpha*|p_released| == tau                     4.9000000000000006e-14
    next x_inf       == this x_inf + tau          0.125000000000049

So E floors the zero step to `alpha_min`, and the released variable ends the
minor sitting exactly `tau` off its bound. The bound is added to the working set
and — because the step was the floor — the engine's ordinary exact re-pin is
deliberately skipped (`arm_unpinned_adds` 3555 == `arm_min_steps` 3555 on this
cell).

**Record 2 — minor 1781, a `drop_dantzig` pass.**

| | `off` | `E` |
|---|---|---|
| `action`, `drop_route` | `drop_dantzig`, `dantzig` | same |
| `ws_size_in → ws_size_out` | 22 → 21 | same |
| `ws_sig_in → ws_sig_out` | `7832398285949506436` → `13695891117004593176` | same |
| `drop_candidates`, `drop_tied` | 9, 1 | same |
| `candidates` (8 recorded, largest first) | `bound:14:lower:0.10734366316506212`, `bound:8:lower:0.042…`, … | same |
| `step_tol` | `1e-09` | same |
| **`x_inf` at entry** | **`0.125`** | **`0.12500000000004899`** |
| **`x_l1` at entry** | **`7.1252949701126038`** | **`7.1252949701170047`** |
| **`p_inf`** | **`0`** | **`2.3359647075287156e-13`** |

Under E the iterate **has** moved: `x_inf` is exactly `0.125 + tau`. `p_inf` is
`2.336e-13`, which is ≤ `step_tol = 1e-9`, so this pass takes the engine's
"KKT point of the current working set" branch. `bound:14:lower` is then dropped
by Dantzig — its multiplier `0.1073…` is the largest of the eight recorded
candidates, and `drop_tied=1` means nothing was tied with it.

**And minor 1782 is minor 1780 again, in 72 of 74 columns, under both arms** —
including `x_inf = 0.125` and `x_l1 = 7.1252949701126038`. The `tau` displacement
that existed at the start of minor 1781 is gone by the start of minor 1782.

### 4.3 INFERRED — what follows, and what does not

*The sentences in this subsection are inferences. The observations they rest on
are §§4.1–4.2; nothing below is read directly off a record.*

* **A ratio-test-only anti-cycling rule cannot break THIS floor/snap recurrence
  in this engine.** *(Read narrowly. `[fix1]` What the evidence establishes is
  the result for E's floor, and more generally for perturbations small enough
  that the next pass still takes the same KKT-snap branch with the same blocker.
  It is NOT a proof of impossibility for every conceivable step-length rule: a
  materially larger step could reach a different branch or a different blocker,
  and nothing here tested one.)* The re-add *is* via the ratio test, so a ratio-test rule is
  addressing the right mechanism — and E *is* such a rule, and it does floor the
  step, and the iterate does move. The floor's displacement nevertheless
  survives exactly one pass: the next pass is a drop pass, every drop pass in
  these traces satisfies `‖p‖∞ ≤ step_tol`, and on that branch the engine sets
  `x` to the current working set's own EQP solution — which re-pins the
  unpinned bound and erases `tau`. The next ratio test therefore starts from
  the same iterate the last one did. EXPAND's guarantee rests on the iterate
  *retaining* an accumulating relaxation; this engine removes it before it can
  accumulate.
* **This is M3b RULE-G's and M4 Task A's observation, realized.** Their note
  that *"the state is a function of the working SET"* applies directly and
  literally here: `x` is re-derived from the working set on every drop pass, so
  a perturbation applied to `x` alone — which is all a step-length rule can
  apply — cannot persist across one *when the next pass takes the same KKT-snap
  branch*, which is the case on every drop pass in these traces `[fix1]`. A rule
  that changes **which** constraint enters or leaves can; a rule that changes
  only **how far** the iterate moves cannot, within that scope.
* **`ACOPP57` is the controlled case for exactly that distinction.**
  `arm_pivot_switches = 0` there: E's largest-pivot second pass never once chose
  a different constraint from the one production would have chosen. So on that
  cell E changed only step length, 3555 times, and the outcome row is
  bit-identical. Where E *did* change the entering constraint — `YORKNET`,
  20409 switches; `STEENBRC`, 8335 — the walk changed a great deal and the
  terminal status still did not.
* **A narrowly targeted next arm follows from this, and is not proposed here.**
  What the observations point at is a rule that refuses the immediate re-add of
  a just-released constraint, or that survives the KKT snap — not a further
  adjustment to step length. Whether such a rule is sound, safe, or
  implementable is not established by anything in this directory, and
  `RULE.md` §9 forbids acting on it. **Nothing ships.**
* **Simply normalizing `-0.0` to `+0.0` is not indicated by these records.**
  OFF's `alpha` is `-0` and E's is a positive floor; the two compare equal, and
  a sole zero blocker remains a zero blocker either way. *(This inference agrees
  with the Codex review of M4 §7, reached here independently from the traces.)*

---

## 5. What this binary is not — the two disclosed departures, and one instrument note

Found by the Codex review of M4 (`.superpowers/m7-w2-m4arm-review-sol.md` §1a,
§4). They are properties of the binary every row in this directory came from, so
they are disclosed here and counted per cell in
[`tables/arm.md`](tables/arm.md) §3.

**(1) The EXPAND reset fires one iteration early.** `begin_minor()` increments
`expand_iter` and `delta`, returns true at `expand_iter >= K`, and the caller
resets *immediately before that minor's ratio test* — so the K-th iteration runs
at `delta_0` rather than at the expanded `delta`. The paper performs K expanded
iterations and resets afterwards.
(`include/hven/detail/qp/qp_anti_cycling_arm.h:258`, `src/qp/qp_engine.cpp:632`.)
**M4's neutrality legs never reached a reset, so M4's data could not see this.
These legs did:** `SAWPATH` reports `arm_resets=3` and `BRITGAS` `arm_resets=4`.
**The reset path ran under the off-by-one on those two cells**, and on those two
only — the other eight report `arm_resets=0`.
`arm_reset_nontrivial_moves` is 0 on all ten.

**(2) The negative-curvature ride calls the armed ratio test, but its add path
always re-pins the blocking bound exactly**, defeating the `alpha_min` floor on
that path. The adaptation is not stated in the arm's own header.
(`src/qp/qp_engine.cpp:2583`, `:2600`.) `arm_min_steps` counts every minor whose
step was the floor; `arm_unpinned_adds` counts those on which the exact re-pin
was actually skipped. **Equal means the ride floor path was not exercised;
unequal means it was, and the difference is the count of hidden re-pins.**

| cell | `arm_min_steps` | `arm_unpinned_adds` | hidden re-pins |
|---|---|---|---|
| `ACOPP57` | 3555 | 3555 | 0 |
| `DALLASM` | 5285 | 5285 | 0 |
| `STEENBRB` | 9508 | 9401 | **107** |
| `ACOPP118` | 9229 | 9229 | 0 |
| `ACOPR57` | 3000 | 3000 | 0 |
| `STEENBRC` | 12936 | 12783 | **153** |
| `STEENBRG` | 13998 | 13819 | **179** |
| `YORKNET` | 24993 | 24230 | **763** |
| `SAWPATH` | 24088 | 24088 | 0 |
| `BRITGAS` | 29897 | 29862 | **35** |
| **total** | | | **1237** |

**Five of the ten exercised it.** M4's three neutrality cells did not
(`arm_min_steps == arm_unpinned_adds` on all three), which is why M4's data
could not see this either.

**(3) `negligible_pivot` is printed as a realized parameter, and the
implementation does not read it.** `arm_negligible_pivot=9.9999999999999998e-13`
appears in every row of this task, and eligibility in the ratio test always uses
hven's existing `kEngineDenomTol` instead
(`include/hven/detail/qp/qp_anti_cycling_arm.h:145`). The default happens to
equal the engine constant, so **this round's data are unaffected** — but the
printed key falsely presents the threshold as tunable and realized, and any
reader of `RULE.md` §1.2's parameter list should read that one line as a
declaration of intent, not as a description of what ran.

**(4) `RULE.md` §1.2 calls the parameters "the paper's".** They are partly
paper-derived: `delta_0 = 0.5·delta_f`, `delta_K = 0.99·delta_f`, `K = 10000`
and `tau` are the paper's choices; `delta_master = 1e-9` deliberately replaces
the paper's ≈`1e-6`; and the negligible-pivot threshold is hven's own constant
per (3). `RULE.md` is not edited — it has run — so the correction is recorded
here.

---

## 6. The predicates, the read-outs, and how they were run

[`tables/readouts-m4b.md`](tables/readouts-m4b.md), written by
[`wrappers/readouts_driver_m4b.py`](wrappers/readouts_driver_m4b.py).

**§6 is PRE-REGISTERED on this task.** `RULE.md` §6 inherits M4 §§6.2–6.6 by
reference and pre-registers them, before any leg, for these ten cells. M4's own
driver, `readouts_driver_m4.py`, is committed beside the M4b driver **with its
POST-HOC docstring unedited** — that label is true of M4's three *neutrality*
cells, which is the only thing it speaks about, and the file is not edited to
say something else. The M4b driver's own docstring declares the four differences
between them (cell set, arm set, per-cell loading, and the dropped POST-HOC
label); **`readouts_m4.py`, `classify_cause_fix1.py` and `classify_walk_m3b.py`
— the tail finder and every predicate — are imported unmodified. Nothing that
computes a number is derived; only the loop around them is.**

**M4 §6.4's DECLARED DEFECT travels with the predicate and is repeated here.**
The coverage-only form of `RECURRENT_TAIL` admits a tail as soon as `|T| > p`.
At the search cap `p = 64` that is 65 passes, and on a 200-pass solve — the
length an arm that *worked* might well produce — 65 passes is 32.5 % and would
score as recurrence on a walk that merely coincided with itself twice at a
stride of 64. **T3 (`|T| ≥ 8p`) closes it**, and costs the OFF controls nothing:
their observed tails carry hundreds of repeats at period 2. A consequence,
stated so it is never a surprise: a solve with `N < 9` can never be `RECURRENT`,
and `RECURRENCE_GONE` reduces on it to "the solve moved".

**Which OFF traces were used, and why.** M4's, at
`.scratch/m7w2m4/arm/traces/off-<CELL>.csv`, not M3c's committed pass-2 slices.
The reason is identity: the OFF *row* this task compares against **is** M4's
row (copied byte for byte under `RULE.md` §4), so the OFF *walk* must be that
row's own walk. M3c's committed slices are slices of one solve per cell; these
are the whole run. Their sha256 were pinned by M4 in its own
`traces-m4/SHA256-parent-traces-m4.txt` and re-verified for this task at
read-out time — [`logs/pin-off-traces.txt`](logs/pin-off-traces.txt), 10/10 OK.

---

## 7. What this does NOT show — `RULE.md` §9, carried in its own terms

* **That E generalizes beyond TRAIN. HOLDOUT was never run.**
* **That E is cycle-free on hven's walk.** It is not proven so; known cycling
  examples exist even for EXPAND (Hall & McKinnon). On these ten, **every cell
  still carries at least one RECURRENT solve under E** — 45 across the ten,
  against OFF's 52 — and M4's read-outs already showed E carrying *four*
  recurrent solves on `ZAMB2-8` where OFF had three.
* **That §1.2's parameters are the right ones.** They are partly the paper's,
  taken once, with the corrections in §5(3) and §5(4).
* **That a converted cell converted for the reason the arm claims.** There are
  no converted cells. §6.6's read-outs are evidence toward mechanism and are
  labelled observed; anything beyond them is labelled inferred.
* **That E is neutral anywhere.** This task has no neutrality bar, by `RULE.md`
  §5, and §5.2's descriptive report asserts no such thing. `ACOPP57`'s
  byte-identical row is one cell, and its own trace shows 7156 of 7248 records
  differing underneath it.
* **That canonical EXPAND behaves as this binary does.** See §5.
* **That a re-add veto, or any other next arm, is sufficient or safe.** §4.3 is
  labelled inferred and licenses nothing.
* **Anything about production.** `RULE.md` §9: nothing ships. Not a default
  change, not a production switch, not a line under `src/` or `include/`, not in
  this task and not as a consequence of it. **Wall-clock in this directory is
  informational only** (CLAUDE.md §7); the deadlines are STOPS, not timings.

---

## 8. The files

| | |
|---|---|
| [`RULE.md`](RULE.md) | the rule, committed before the first leg, unedited |
| [`verdict.txt`](verdict.txt) | §§6–7 applied mechanically, per cell |
| [`tables/arm.md`](tables/arm.md) | ten cells × every §7.1 field, §6.6's counters, the two departures counted per cell |
| [`tables/neutrality-descriptive.md`](tables/neutrality-descriptive.md) | §5.2's descriptive report on the three cells, **no verdict** |
| [`tables/readouts-m4b.md`](tables/readouts-m4b.md) | §6's pre-registered predicates and per-solve read-outs, all twenty traces |
| [`tables/readd-route-posthoc.md`](tables/readd-route-posthoc.md) | **POST-HOC** — the re-add route, counted over all twenty flat traces |
| [`tables/acopp57-tail-period-posthoc.md`](tables/acopp57-tail-period-posthoc.md) | **POST-HOC** — one full period of `ACOPP57`'s tail, every column, both arms |
| [`rows/`](rows) | the ten E rows the chain wrote |
| [`rows-off/`](rows-off) | the ten OFF rows, copied byte for byte from M4, with their manifest |
| [`logs/`](logs) | the chain log, the two comparison transcripts, the pin transcripts, per-leg stderr |
| [`traces-m4b/`](traces-m4b) | the committed trace slices, `SLICES.txt`, and the sha256 of every slice and every parent flat trace |
| [`wrappers/`](wrappers) | the chain, the gate, the inherited read-out machinery, and this task's five derived scripts |
| `MANIFEST.sha256` | every committed file in this directory |
