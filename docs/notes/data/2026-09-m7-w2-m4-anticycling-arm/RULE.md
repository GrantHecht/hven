# M7 W2 M4 — RULE.md: does breaking the degenerate cycle make the shipped QPs solve?

**Written and committed BEFORE any leg ran — the OFF controls and the identity
legs included.** Its bytes are the pin. If a rule below turns out to be wrong,
the defect is DECLARED in the report and the rule stays as written; a rule
edited after seeing what it rejected is not a rule.

Tree `/home/ghecht/Projects/hven`, branch `m7`, base `f5205770` (M3c's close).
Task A landed at `62f75073`, its gate chain's declaration at `196f0ef0`, its
gate record at `9684a9b8`. This rule is written against `9684a9b8`; no commit
under it touches `src/` or `include/`.

---

## §0 What this asks, and what it is not

M3c established, at DEFAULT options and over the whole failing TRAIN set, that
**eight of the `hven-sqp` lane's failing cells die on capped QP subproblems**,
and that **19 of 21 traced capped solves are a zero-motion degenerate
drop/re-add cycle** — one bound, period 2 (sometimes 4 or 6), with 92.9 % of all
minors spent after the walk began repeating. Inertia control and the
negative-curvature ride were ruled out as the primary remedy. The mechanism was
named: classical degenerate active-set cycling under hven's conventions.

Task A built two ARMS against that mechanism, both `HVEN_TESTING`-gated, both
OFF by default, and proved the production library byte-identical with them
compiled in. **This rule measures whether either arm makes the shipped QPs
solve.**

It is an EXPERIMENT and it is not a change. Nothing in it licenses a line of
production code, a default change, or a remedy selection. §10 says so again at
the end, where it binds the outcome rather than the intent.

**What it cannot ask.** It cannot ask whether an arm generalizes beyond TRAIN:
HOLDOUT is never run, here or later, under any outcome. It cannot ask which arm
is CORRECT — neither is proven cycle-free on hven's walk, and known cycling
examples exist even for EXPAND (Hall & McKinnon). It ranks two arms on ten
cells; that is all it does.

---

## §1 The cells

### 1.1 The TEN — M3c's pass-2 cells, at DEFAULT options

`ACOPP57`, `DALLASM`, `STEENBRB`, `ACOPP118`, `ACOPR57`, `STEENBRC`,
`STEENBRG`, `YORKNET`, `SAWPATH`, `BRITGAS`.

These are exactly M3c's pass-2 set: nine `QP_CAP_BUDGET` cells plus `YORKNET`
(`RESTORATION_EXIT(infeasible)`, which M3c's census shows reaching its
restoration through the same capped walks). They are the cells whose pass-1
rows are COMMITTED in `docs/notes/data/2026-09-m7-w2-m3c-default-census/rows/`
and therefore the only cells whose OFF control has something to be gated
against.

`OFF` on all ten solves NONE of them at DEFAULT. That is the point: there is no
cell here that the arm could break, which is why §7's HARM clause reports
`N/A (OFF solves no cell in §1.1)` rather than a number.

### 1.2 The THREE NEUTRALITY cells — the cells `hven-sqp` ALREADY solves

`READING6`, `SMBANK`, `ZAMB2-8`.

These are the three of the 23 CUTEst TRAIN cells whose `hven-sqp @aeebdccc` row
is `optimal`, named in
`docs/notes/data/2026-09-m7-w2-m3c-default-census/RULE.md` §1 as the three
excluded from M3c's census for exactly that reason, and read by it from the
four-lane table
`/home/ghecht/Projects/hven-bench/runs/2026-09-m7-w2-h-repin/four-lane-table.md`.
`hven-bench` is READ-ONLY; nothing under it is written, and its CMake is never
entered.

**What they are for.** An arm that changes a walk where NO tie and NO zero ratio
occurs is not an anti-cycling rule, it is a behaviour change. Task A's
`NeitherArmChangesAWalkWithNoTieAndNoZeroRatio` fixture asserts that on one
constructed QP; these three cells assert it on whole DEFAULT solves of real
problems. **A difference on any of them is a FINDING and STOPS that arm** (§8).

### 1.3 What is NOT run here

* **HOLDOUT. Never, under any outcome.**
* The five CENSORED cells of M3c (`MSS2`, `MSS3`, `READING8`, `TAX213322`,
  `TAX53322`). M3c's `RULE.md` §3.1 forbids filling their rows later and this
  rule does not reopen them. They are not in any leg.
* The three group-W cells that carried counters (`ACOPR118`, `BIGBANK`, and
  `SAWPATH`, the last of which IS in §1.1) beyond `SAWPATH`. `ACOPR118` and
  `BIGBANK` are a CONDITIONAL extension only, under §9, and only if §7 finds a
  conversion.

---

## §2 The four arms

| arm | switch | what it is |
|---|---|---|
| `off` | `QpAntiCycleArm::kOff` | production behaviour exactly. The CONTROL. |
| `E` | `kExpand` | EXPAND-style working-feasibility tolerance (Gill, Murray, Saunders & Wright 1989), as transcribed in `include/hven/detail/qp/qp_anti_cycling_arm.h` with each rule's section cited at its site |
| `L` | `kLexicographic`, `lex_full_pricing = false` | the TIE-BREAK form: a fixed lexicographic order broken only where the ratio test ties and where `drop_worst`'s Dantzig window ties |
| `L+` | `kLexicographic`, `lex_full_pricing = true` | Bland's rule proper — the form the classical finiteness argument actually covers |

**`L` and `L+` are BOTH run, as separate sub-arms, and the reason is
pre-registered here.** Task A found `L` as briefed to be **vacuous on hven's own
drop rule**: on both cycling fixtures `lex_drop_switches` is 0 and the escape
comes entirely from `lex_ratio_switches`. `drop_worst` picks by largest
violation (Dantzig) and then by angle, so its tie WINDOW rarely has two members
whose lexicographic winner differs from the largest-angle one. An arm whose drop
half never fires is not the arm the finiteness argument covers, and reporting
`L` alone would report a half-arm under a whole arm's name. `lex_full_pricing`
exists for that reason and is OFF by default; it is the second sub-arm here so
that the difference between "the tie-break fired" and "Bland's rule fired" is
MEASURED and not assumed.

### 2.1 The parameters, and where each number comes from

Fixed before any leg, from the PAPER's own recommendations where it gives them,
and not tuned afterwards. Every one of them is PRINTED in each leg's row, so a
reader never has to trust this table.

| parameter | value | source |
|---|---|---|
| `delta_master` | `1e-9` | the paper's `delta_f = eps^(3/8)` reports `1e-6` at `eps = 1e-16`; hven's own structural-violation tolerance makes `1e-6` too loose, and the header argues the choice at its site |
| `delta_0_factor` | `0.5` | §4.2, `delta_0 = 0.5 * delta_f` |
| `delta_max_factor` | `0.99` | §4.2, `delta_K = 0.99 * delta_f` |
| `expand_k` | `10000` | §4.2, `K = eps^(-1/4)`; the paper reports `10000` at `eps = 1e-16` |
| `tau` | derived, `(delta_K - delta_0)/K` | §4.2, derived rather than typed |
| `negligible_pivot` | `1e-12` | the pivot below which a constraint cannot block |
| `reset_nontrivial` | `3.7e-11` | the §4.3 reset's "the iterate actually moved" bar |
| `lex_full_pricing` | `false` on `L`, `true` on `L+` | §2 above |

**A parameter is never changed between legs of the same arm**, and no leg is
re-run with a different one. If a parameter is wrong, that is a finding for the
settler and a separate experiment, not an edit here.

---

## §3 The instrument

### 3.1 The probe

`wrappers/m4_arm_probe.cpp` — M3c's `wrappers/m3c_p2_probe.cpp`, copied at this
task's HEAD and extended by **new ARGUMENTS and new PRINTED KEYS only**. Not one
line of its existing printing is removed or reordered, so a row of this probe is
a superset of a row of M3c's.

The additions:

* `--arm off|E|L|L+` — sets `QpAntiCycleArmState::arm` (and, for `L+`,
  `params.lex_full_pricing = true`) on the solving thread, before the solve, and
  resets the state after it.
* the eight EXPAND/lexicographic parameters as arguments, each defaulting to the
  header's own default, so a leg that passes none of them runs the §2.1 table.
* the arm's per-solve counters printed as new keys:
  `arm_min_steps`, `arm_pivot_switches`, `arm_unpinned_adds`,
  `arm_lex_drop_switches`, `arm_lex_ratio_switches`, `arm_resets`,
  `arm_reset_nontrivial_moves`, plus the arm label and every parameter's
  realized value.

**No file under `src/` or `include/` is changed by this task from `9684a9b8`
onward.** If the probe cannot reach a switch from outside those trees, the work
STOPS and reports it; it does not reach in.

### 3.2 The build — a REPLAY, not a construction

Built by `wrappers/replay_build_m4.py`, M3c's `replay_build_p2a.py` copied into
this task's `wrappers/` (M3c's is never edited), under M3c's RULE-P2A §3
unchanged:

* the install build's own Ninja edges are expanded against their rule's
  `command` template exactly as Ninja does and run through `/bin/sh -c`;
* the arm directory MIRRORS the install tree's relative object layout and the
  replay runs with that directory as its cwd, so the command STRING is
  identical, output path included;
* **`-DHVEN_TESTING=1` and nothing else** is added to each compile command, at
  the position CMake's own alphabetical `DEFINES` ordering would have put it;
* RULE-P2A §3.3a's PCH amendment applies unchanged and is the only other
  declared difference: the four PCH-consuming TUs get a PCH rebuilt from the
  same command plus the same single define, and their one `.pch` path token
  rewritten;
* the harvest transcript must be EMPTY of any other difference and the script
  exits non-zero if it is not;
* the compiler is PINNED to `/usr/bin/clang++`;
* the archive sha256 is recorded, and RULE-P2A §3.4's three structural checks
  are run: object identity against the install build, the `QpWalkObserver`
  symbol count (0 without the define, ≥ 1 with it), and the assertion that no
  link line names the INSTALLED `libhven.a`.

A replay that cannot reproduce a command exactly STOPS and reports the residual.
Nothing is approximated.

### 3.3 The smoke

ONE cell, `ACOPP57`, at `--arm off`, run before the chain is detached, under the
box lock, in the FOREGROUND if it fits inside 600 s and DETACHED under the lock
with the fact disclosed otherwise. It is not a leg and produces no row in
`tables/arm.md`; it exists so that a chain is never detached against a binary
nobody has run.

---

## §4 The legs, the terms, and the order

### 4.1 The order

1. **OFF controls**, the ten §1.1 cells, one leg each. Each gated by §5 against
   its COMMITTED M3c pass-1 row. **A cell that does not reproduce STOPS THAT
   CELL** — it is carried no further into any arm, and its line in every table
   reads `OFF_CONTROL_FAILED`.
2. **The neutrality legs**, the three §1.2 cells × four arms (`off`, `E`, `L`,
   `L+`) = 12 legs. `E`, `L` and `L+` must reproduce that cell's OWN `off` leg
   **bit-for-bit** on every printed field. **A difference STOPS THAT ARM** (§8).
3. **`E`**, then **`L`**, then **`L+`**, on the ten §1.1 cells — each arm only
   over the cells whose OFF control passed and only if §8 did not stop it.

This order is not cosmetic. The controls run first because a cell whose OFF leg
does not reproduce M3c cannot have its arm leg interpreted at all; the
neutrality legs run second because an arm that is not neutral where no
degeneracy occurs is a behaviour change and must stop before its results are
read as anti-cycling evidence.

### 4.2 The terms, identical to M3c's

* **DEFAULT options throughout.** `--tr-init` is never passed, so
  `SqpOptions::tr_init` is not assigned at all and the row prints
  `tr_init_touched=0`. No option is set except `common.threads = 1` and
  `common.print_level = 10`, which is the lane's own `default` label.
* `MKL_NUM_THREADS=1 OMP_NUM_THREADS=1`, `taskset -c 2`, **one solver process at
  a time**, serial.
* The box lock, `flock /tmp/box-build.lock`, **no timeout, no fallback**, held
  per leg and released between legs, with a `pgrep` for a foreign cell binary as
  its OWN command before the lock and again inside it, and `set -o pipefail`
  everywhere.
* **The SOLVER deadline is 600 s**, enforced by the probe's iteration callback —
  **300 s for `SAWPATH`**, the value M3c ran it at.
* **The OUTER stop is `timeout 900`** per leg. A leg it kills, or a leg exiting
  non-zero for any other reason, is **CENSORED**: no row, no trace, nothing
  filled in later, and its cell is reported CENSORED for that arm.
* The ledger is ON; the observer is ARMED with `--capture-solve -1` (every
  solve), `--max-records 2000000`.
* The whole of it is ONE DETACHED CHAIN (`setsid`) with its pid in
  `.superpowers/m7-w2-m4-STATE.md`, and nothing polls it.

### 4.3 Wall-clock

**Informational only** (CLAUDE.md §7). The two budgets in §4.2 are STOPS, not
measurements. No wall figure produced by this task is quoted as a measurement,
compared against a serial timing, or used in any verdict. The cost columns that
DO enter a verdict are counters: minors, majors, factorizations.

---

## §5 The OFF-control identity gate — every printed field, never a subset

This is M3c's RULE-P2A §5 applied to this task's OFF legs, with its hardened
`gate_p2a.py` v2 copied into this task's `wrappers/` as `gate_m4.py` (M3c's is
never edited). It exists because of the lesson RULE-P2A §5 records: a smoke that
chose eight integer counters on one cell reported a seam neutral while `f`,
`stationarity` and `feasibility_e` already differed in the same transcript. **A
gate that chooses its fields can only find what it chose to look at.**

### 5.1 The field set is COMPUTED, not chosen

The compared set is the **INTERSECTION** of the two rows' own `key=value` field
sets, computed mechanically, every member compared.

* Fields the committed pass-1 row carries and this probe does not print are
  reported BY NAME in an `ABSENT` list. **An absent field is never a pass.**
* Fields only this probe prints — the arm keys of §3.1, the trace bookkeeping —
  are listed too, so a reader sees the whole of what was and was not compared.
* The resulting field count and the full field list are printed per leg.

**The EXPECTED-FIELD MANIFEST** is not a list typed here: it is the committed
bytes of `docs/notes/data/2026-09-m7-w2-m3c-default-census/rows/<CELL>.txt`,
read by the gate at run time. Those rows carry, per cell, the `PROBE` block's
~50 scalar keys, the `OPTIONS` block's 16, the `COUNTERS` block's 49 (with its
own `c_field_count`), the `HISTORY` block's 29-column table with its
`h_field_count`, and the `LEDGER` blocks' first-record, aggregate and
per-record fields. The gate compares every one of them that this probe also
prints, and names every one it does not.

### 5.2 The class of a field is decided by its VALUE TEXT

A field is **residual class** iff either row's value text parses as a
floating-point number AND contains one of `.`, `e`, `E`, `nan`, `inf`.
Everything else — integers, statuses, names, signatures — is **byte class**.

* **byte class: exact byte equality. No tolerance of any kind.**
* **residual class: CLAUDE.md §7's near-ulp gate, declared here as**
  `|a − b| ≤ 1e-12 · max(1, |a|, |b|)`. This is the SAME constant M3c's
  `RULE-P2.md` §5.1 and `RULE-P2A.md` §5.2 declared, deliberately and without
  change: the three gates must be comparable, and a gate loosened after seeing
  what it rejected is not a gate.
* `nan` is matched as a BYTE fact and never numerically. `nan` against a number
  is a DIFFERENCE.
* Every residual-class verdict prints as `EQUAL_BYTES` / `WITHIN` / `EXCEEDS`
  **with both values and the absolute difference**, so "identical" and "within
  tolerance" are never conflated, and the count of near-ulp acceptances is
  reported per leg.

### 5.3 Wall-clock fields are excluded BY NAME and the exclusion is printed

The excluded names are exactly those matching `wall`, `seconds_elapsed` or
`time`. The deadline SETTING (`deadline_seconds`) and the deadline OUTCOME
(`deadline_fired`) are not wall-clock readings and ARE compared.

### 5.4 The verdict

A leg **REPRODUCES** iff every byte-class field is byte-equal and every
residual-class field is `EQUAL_BYTES` or `WITHIN`. Anything else is a
NON-REPRODUCTION, reported with every differing field, both values, and the
class each fell into.

**An OFF leg that does not reproduce STOPS THAT CELL.** It is not re-run to
agreement, it is not explained away, and the cell is carried into no arm.

### 5.5 The neutrality legs use the SAME gate, against a different reference

For §1.2's three cells the reference row is not M3c's — those cells have no M3c
row, having never failed. The reference is **that cell's OWN `off` leg from this
task's chain**, and the bar is **stricter**: every compared field must be
`EQUAL_BYTES`. `WITHIN` is NOT a pass on a neutrality leg. Two runs of the same
binary on the same cell at `MKL_NUM_THREADS=1` in the same process ordering
differ in nothing, and an arm claiming to be inert where no tie occurs owes bit
equality, not a tolerance.

### 5.5a AMENDMENT 1 — the arm's OWN keys on a neutrality leg

**Written before any leg of this task's chain ran, before the chain script
existed, and for a structural reason, not a numerical one.** The smoke of §3.3
is not a leg (§3.3 says so) and produced no row in any table; this rule is
therefore still unrun, and an amendment made now is a pre-registration, not an
edit to a rule that has seen its data. It is marked as an amendment so that a
reader can see exactly what changed and when, the way M3c's `RULE-P2A.md`
§3.3a marked its own.

**The defect.** §5.1 computes the compared field set as the INTERSECTION of the
two rows' key sets, and §5.5 requires every compared field to be `EQUAL_BYTES`
on a neutrality leg. On a neutrality leg BOTH rows come from THIS probe, so the
intersection includes this probe's own `arm` and `arm_*` keys — and `arm=off`
against `arm=E` differs by construction. As written, §5.5 would fail every
neutrality leg on the LABEL OF THE EXPERIMENT and stop all three arms for a
reason that is not about the walk at all.

**The amendment.** On a NEUTRALITY leg, and only there, fields whose name is
`arm` or begins with `arm_` are **EXCLUDED BY NAME, and the exclusion is
PRINTED** — exactly the treatment §5.3 already gives wall-clock fields, and for
the same reason: they describe the LEG, not the solve. The bar on every other
compared field is unchanged and stays `EQUAL_BYTES`.

**And they are REPORTED, not discarded.** Every neutrality leg prints its arm
counters beside its verdict. A neutrality cell on which `E`'s counters are
NON-ZERO and every behaviour field is nevertheless bit-identical is a strong
and reportable fact — the arm's machinery ran and changed nothing — and a rule
that dropped those counters silently could not carry it. A cell on which they
are all zero says something different and weaker: the arm never engaged there,
so the leg tested that the SWITCH is inert rather than that the RULE is.

**The OFF controls are untouched by this.** They compare against M3c's committed
rows, which carry no `arm` key at all, so the arm keys fall into §5.1's "printed
by the leg only" list and were never compared there.

### 5.6 The gate's own folds, run before it reads any leg

`gate_m4.py` runs its folds on CONSTRUCTED inputs and prints the transcript:
identical rows PASS; a single changed integer counter STOPS and names it; a
float inside the tolerance is `WITHIN`; a float outside it STOPS; `nan` vs `0`
STOPS; `nan` vs `nan` is `EQUAL_BYTES`; a field present in one row only is
reported ABSENT and never counted as agreement; a missing row file is REFUSED;
and — this task's addition — the STRICT mode of §5.5 rejects a `WITHIN` that
the ordinary mode accepts. **A checker that has never been seen to fail is not a
checker.**

---

## §6 Per traced solve — `RECURRENCE_GONE`, and the derivation of every number in it

### 6.1 The instrument these predicates read

Each traced solve is one walk-observer capture: a sequence of PASS records
carrying, per pass, the incoming working-set signature (`qp_walk_ws_signature`,
a 64-bit order-independent sum of mixed member tokens — order-independent,
side-sensitive and identity-sensitive, asserted by the seam's own fixtures), the
branch taken, the constraint identity added or dropped, `alpha`, and `dx_inf`.
The tail finder is M3c's `wrappers/readouts_p2b.py` machinery, copied into this
task's `wrappers/` and run unchanged: `maximal_periodic_tail` over the bit-exact
F0 state keys, candidate periods `p in [1, 64]`, ties to the SMALLEST `p`,
requiring `|T| > p`.

### 6.2 `RECURRENT_TAIL(solve)` — TRUE iff ALL THREE hold

* **T1** a maximal exactly-periodic suffix `T` at some `p ≤ 64` EXISTS;
* **T2** `|T| ≥ 0.10 · N`, where `N` is the solve's recorded pass count;
* **T3** `|T| ≥ 8p` — the tail is at least EIGHT full cycles.

`RECURRENCE_GONE(solve) := (NOT RECURRENT_TAIL(solve)) AND (Σ‖Δx‖∞ over the
solve > 0)`.

### 6.3 Where `0.10` comes from, and why it is not tuned

The bar is placed strictly inside the pure-mechanism value and the null value,
both of which are OBSERVED, not assumed, in M3c's committed
`tables/readouts-p2b.md`:

* **pure mechanism.** The 19 `CYCLING_PROPER` solves of M3c's traced set carry
  `tail_share_of_passes` from **0.8205** (the 6 980-record solve) to **0.9994**
  (the 5 080-record one), with a median near 0.96. The SMALLEST observed
  cycling coverage is 0.82.
* **null.** A solve with no exactly-periodic suffix at any `p ≤ 64` scores
  **0** by construction — M3c's traced set contains such a solve, and every
  neutrality cell of §1.2 is expected to be one.
* **the bar.** `0.10` is **8.2× below** the smallest observed cycling coverage
  and an order of magnitude above nothing. It is placed LOW on purpose:
  `RECURRENCE_GONE` is the ARM's claim, and a bar placed just under the observed
  cycling values would make the claim easy to meet by shortening a tail rather
  than removing it. At 0.10, `GONE` means the repeating tail is not merely
  shorter — it is gone by an order of magnitude.

### 6.4 Where `8p` comes from — A DECLARED DEFECT OF THE COVERAGE-ONLY FORM

The predicate as first drafted — a periodic suffix covering ≥ 10 % of passes —
**has a defect on SHORT solves, and it is declared here rather than discovered
later.** `maximal_periodic_tail` admits a tail as soon as `|T| > p`, i.e. one
full cycle plus one repeat. At the search cap `p = 64` that is 65 passes, and on
a solve of, say, 200 passes — the length an arm that WORKS might well produce —
65 passes is 32.5 % and would score as recurrence on a walk that merely
coincided with itself twice at a stride of 64.

T3 closes it. Eight full cycles at a fixed stride, on the bit-exact F0 state key
(every recorded field, floats packed back to their eight bytes), is not a
coincidence a progressing walk produces. And it costs the OFF controls nothing:
their observed tails carry **888 and more** repeats at period 2 — two orders of
magnitude above the bar — so T3 cannot fail on a cell that really cycles.

A consequence, stated so it is never a surprise: a solve with `N < 9` can never
be `RECURRENT`, and `RECURRENCE_GONE` reduces on it to "the solve moved". That
is the correct reading — a nine-pass solve has no repeating tail to lose.

### 6.5 The second conjunct

`Σ‖Δx‖∞ > 0` over the solve is the motion requirement, and it is on the WHOLE
solve, not on the tail. It is there because the M3c traced set contains a solve
whose maximal periodic suffix covers 100 % of its passes and whose
`tail_sum_dx_inf` is **74.53**: a walk that is going somewhere and whose
signature happens to return. `GONE` must mean "the walk moved and did not
repeat", and a predicate resting on the tail alone would call that solve cured
and a zero-motion cycle of the same shape cured too.

### 6.6 The other per-solve read-outs, pre-registered

Per traced solve, printed whether or not `RECURRENCE_GONE` holds:

* **`QP_EXIT` class** — `optimal` / `max_iter` / `numerical_error` /
  `infeasible`, from the ledger record's own status;
* **minors and factorizations vs the OFF control's SAME solve**, matched by
  ledger record index and label (`f_qp_k`), reported as both values and their
  ratio. A record index present under one arm and not the other is reported as
  such and never silently dropped;
* **minors at exit vs the cap** — the engine's effective minor cap for that
  solve beside the minors it used, so "it stopped early" and "it ran out" are
  never confused;
* **the arm's own counters**: `arm_min_steps`, `arm_pivot_switches`,
  `arm_unpinned_adds`, `arm_lex_drop_switches`, `arm_lex_ratio_switches`,
  `arm_resets`, `arm_reset_nontrivial_moves`. On `L` these are the direct test
  of Task A's vacuity finding: if `arm_lex_drop_switches` is 0 across the ten
  cells, the drop half of the tie-break form never fired on real problems
  either, and the report says so in those words;
* **the constraint the OFF control cycled on** — M3c's
  `tail_readmitted_modal_identity` for that solve — **and whether it is in the
  final working set under the arm.** Three outcomes are distinguished and none
  of them is a verdict: it is held at exit; it is not held at exit; the OFF
  control had no modal identity to name;
* **Dantzig ties under the arm**: `drop_ties` and `ratio_ties` from the ledger,
  per record, beside OFF's.

---

## §7 Per cell — `CONVERTED`, `HARM`, and `improved, unverified`

### 7.1 The fields compared, per cell, arm vs OFF

SQP terminal status, majors, minors, factorizations, `f`, `stationarity`,
`feasibility_e`, `feasibility_i`, `steps_accepted`,
`qp_failures_in_a_row_peak`, and the restoration record
(`restoration_entries`, `restoration_sub_status`, `restoration_h_in/out`,
`restoration_f_in/out`). All of them printed for every cell × arm in
`tables/arm.md`, whatever the verdict.

### 7.2 THE RANK ORDER on terminal status, fixed here

Better first. The principle: **an outcome that asserts a solution ranks above
one that admits defeat, which ranks above one that makes a CLAIM ABOUT THE
PROBLEM that the other lanes contradict, which ranks above one that reports the
arithmetic failed.**

| rank | status | |
|---|---|---|
| 0 | `optimal` | |
| 1 | `acceptable` | |
| 2 | `max_iter` | ran out of majors; asserts nothing false |
| 3 | `stalled` | |
| 4 | `diverging` | |
| 5 | `budget_exhausted` | |
| 6 | `infeasible` | ON THESE CELLS this is a claim other lanes contradict — `YORKNET`'s consensus objective exists and IPOPT/SNOPT/hven-ipm reach it — so it ranks BELOW admitting defeat. It is not ranked low as a general matter of the enum; it is ranked low ON THIS CELL SET, and the report says which cells it applied to |
| 7 | `numerical_error` | |
| — | `interrupted` | **NOT RANKED.** `interrupted` here means the 600 s (300 s) callback deadline fired. That is a CENSORING, not a status. A cell whose arm leg fires the deadline is reported `TRUNCATED` for that arm and receives no `CONVERTED`/`HARM` verdict at all |

### 7.3 `CONVERTED`

A cell is `CONVERTED` under an arm iff **all three** hold:

1. `rank(arm status) < rank(OFF status)` in §7.2's order;
2. `max(feasibility_e, feasibility_i) ≤ feas_tol`, with `feas_tol = 1e-6` read
   from that leg's own printed `opt_feas_tol` (not typed here — the leg prints
   what it used);
3. `|f − consensus| / max(1, |consensus|) ≤ 1e-6`, where `consensus` is M3c's
   §1.3 three-lane consensus for that cell, **machine-read from M3c's committed
   `tables/census.md`**, never recomputed and never chosen.

### 7.4 `improved, unverified`

Iff 1 and 2 of §7.3 hold and **no consensus exists** for that cell — M3c reports
`ABSENT (no consensus)` for `STEENBRC`, `STEENBRG` and `BIGBANK`, among others.
The objective bar is then ABSENT and the cell is reported `improved, unverified`
in those exact words. **It is never reported as `CONVERTED`, and never counted
toward a conversion total.** A solver that returns `optimal` at a point nobody
else agrees with has not been shown to have solved anything.

### 7.5 `HARM`

A cell is `HARM` under an arm iff **either**:

* `rank(arm status) > rank(OFF status)`; **or**
* the ranks are EQUAL, the cell is one **OFF already solved**, and
  `minors(arm) > 1.10 × minors(OFF)`.

**On §1.1's ten cells the second clause is `N/A`: OFF solves none of them.** The
report says exactly that rather than printing a vacuous "0 harmed". On §1.2's
three neutrality cells the bar is not 1.10× at all — it is bit equality (§5.5),
and any difference stops the arm.

### 7.6 The consensus rule is M3c's, unchanged

A consensus exists for a cell iff at least two COUNTED lanes agree to 6
significant digits, or at least two counted lanes both report `|f| ≤ 1e-6` (in
which case the consensus is `0`). The counted lanes are `hven-ipm @aeebdccc`
(counted only when `optimal` or `acceptable`), `ipopt` (only `Solve_Succeeded`
or `Solved_To_Acceptable_Level`) and `snopt` (only `Optimal`), read from the
committed four-lane rows. **Only committed RESULT ROWS of the SNOPT lane are
read. No SNOPT source, header or decompiled material is read by this task under
any circumstance.**

### 7.7 The cost read-out against SNOPT, for CONVERTED cells only

For a cell that CONVERTS, the report prints hven's majors / minors /
factorizations beside **SNOPT's committed majors and MINORS** — and states, on
every such line, that **hven's factorization count has NO SNOPT counterpart**
(SNOPT publishes no factorization count). M3c's own correction applies and is
repeated so it cannot be re-made: the four-lane table's fifth column is headed
`facts / minors` and carries FACTORIZATIONS for the two hven lanes and MINORS
for the SNOPT lane. The column is read per lane, not per row.

---

## §8 The stopping rules

| trigger | consequence |
|---|---|
| an OFF control leg does not reproduce its committed M3c row (§5.4) | **that CELL stops.** It enters no arm. Every table line for it reads `OFF_CONTROL_FAILED` |
| an arm's neutrality leg differs from that cell's own `off` leg on any compared field (§5.5) | **that ARM stops**, entirely, on every cell. It is a FINDING, reported with the differing fields, and the arm's partial results are reported as UNINTERPRETABLE rather than withheld |
| a leg is killed by `timeout 900`, or exits non-zero for any other reason | that cell × arm is **CENSORED**. No row, no trace, nothing filled in later |
| the probe's own deadline fires (`deadline_fired=1`) | that cell × arm is **TRUNCATED** (§7.2). It gets counters but no `CONVERTED`/`HARM` verdict |
| the build replay cannot reproduce a command exactly (§3.2) | **everything stops** and the residual is reported |
| the probe cannot reach the arm switch from outside `src/`/`include/` | **everything stops** and it is reported. Nothing under `src/` or `include/` is edited |

A stop is reported in the words above. None of them is re-run to a better
outcome.

---

## §9 The CONDITIONAL extensions — and they are conditional

Run **only if §7 finds at least one `CONVERTED` cell**, and only then:

* the converting arm on **`ACOPR118`** and **`BIGBANK`** — the two group-W cells
  that carried counters at M3c's wall and are not already in §1.1;
* the converting arm on **the five CENSORED cells at the 900 s outer stop**. A
  killed leg is **CENSORED and never filled**; that is the whole of what the
  extension can report about a cell it does not finish.

If §7 finds no conversion, neither extension runs and the report says so. An
extension is not a consolation leg.

---

## §10 What a SUPPORTED result licenses

**Nothing ships.** Not a default change, not a production switch, not a line
under `src/` or `include/`, not in this task and not as a consequence of it.

A SUPPORTED result — one arm converting cells, neutral where no degeneracy
occurs, at a factorization cost the read-outs make visible — **ranks the
production design for the settler and the owner**: which arm, at what
parameters, and what a production switch would have to look like. That ranking
is an input to a future decision that has its own brief, its own design, its own
review and its own gates. It is not a decision.

An UNSUPPORTED result is worth the same amount and is reported with the same
care: that the mechanism M3c named is real and that breaking it, in the two
forms tried here at these parameters, did not make the shipped QPs solve, is a
finding about hven's walk and not a failure of the experiment.

**What this does not show**, to be stated in the README in these terms:

* that either arm generalizes beyond TRAIN. **HOLDOUT is never run.**
* that either arm is cycle-FREE on hven's walk. Neither is proven so; known
  cycling examples exist even for EXPAND (Hall & McKinnon).
* that the parameters of §2.1 are the right ones. They are the paper's, taken
  once, and never tuned against a result.
* that a converted cell is converted for the reason the arm claims. The
  per-solve read-outs of §6.6 are EVIDENCE toward that and are labelled
  observed; anything beyond them is labelled inferred.

---

## §11 The process rules this rule inherits

Verbatim from M3b's and M3c's briefs and from CLAUDE.md:

* **SNOPT source firewall, ABSOLUTE.** Only the SNOPT lane's committed RESULT
  ROWS are read.
* **Algorithmic references are open-literature PAPERS only.** No GPL/LGPL solver
  source is read or incorporated — QPALM, PIQP and the rest, never;
  `~/Software/piqp-src` is never read.
* **`hven-bench` is READ-ONLY** and its CMake is never entered.
* **`docs/notes/data/**` is FROZEN except this task's own directory.** M3a's,
  M3b's and M3c's artifacts are never edited; what this task needs from them is
  COPIED here.
* **`notices/` is untouched.**
* Box protocol: `pgrep` as a SEPARATE prior command; `flock /tmp/box-build.lock`
  with NO timeout and NO fallback; `PGREP_EMPTY` and `FOREGROUND_START`/`END`
  inside the lock; `set -o pipefail`; ONE solver at a time.
* Never signal a process this task did not start; kill only an exact pid it
  started.
* Foreground under 600 s, else DETACHED with the pid in the state note and an
  IMMEDIATE hand-back. Nothing polls a detached chain.
* Explicit pathspecs only — never `git add -A` or `git add .`; `git status`
  immediately before every commit; no `--amend`.
* **NO model attribution trailer of any kind on any commit.**
* No push.

---

## §12 The deliverables

* `tables/arm.md` — one row per cell × arm, with §7.1's fields, §6.6's read-outs
  and the verdict.
* `verdict.txt` — the pre-registered verdicts, applied.
* `README.md` — leading with the result in PLAIN LANGUAGE, observed vs inferred
  labelled on every claim, every rule defect declared (§6.4 is already one, and
  it is declared here rather than found later), the rules unedited, and a "what
  this does not show" section carrying §10's four items.
* `.superpowers/m7-w2-m4-report.md`, ending exactly with the line
  `SIGNOFF: M7-W2-M4-IMPLEMENTER-REPORT`.

**This file is not edited after its first leg runs.**
