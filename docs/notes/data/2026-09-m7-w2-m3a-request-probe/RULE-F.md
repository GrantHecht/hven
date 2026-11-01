# M7 W2 M3a — ADDENDUM F — THE DEGENERATE-STALL PROBE: THE PRE-REGISTERED DECISION RULE

Committed BEFORE any F leg ran, including the identity leg. Nothing below is
revisable by what the legs say; a rule that turned out to be the wrong question
is reported as the wrong question and re-registered in a named successor round,
never edited here.

Settler ruling, 2026-09-21, after addendum E closed.

---

## 0. What this addendum is, and what it is NOT

**It is NOT a re-test of the radius hypothesis.** That hypothesis is CLOSED by
`RULE.md` §5.5 and `verdict.txt` stands exactly as committed at `6bc75ee9`.

**It is NOT a re-test of H-QP.** Addendum E's hypothesis — "the first QP is
solvable given more minor iterations" — is closed by `RULE-E.md` §5's failure
path and `verdict-E.txt` stands exactly as committed at `d834a6a1`. Nothing here
re-runs an E leg, raises a cap, or relaxes an E predicate.

**It is a different question**, and it is the one E's numbers left standing: E
showed the first QP consuming 4680 / 2560 minors at the shipped cap and 46800 /
25600 at ten times it, returning `max_iter` every time. This addendum asks what
the walk was DOING for those minors.

### The hypothesis, registered as a HYPOTHESIS

> **H-DEG.** The first QP's tens of thousands of minor iterations on STEENBRB
> and YORKNET at `tr_init = inf` are a DEGENERATE STALL of the active-set walk,
> not a long genuine walk.

hven documents the mechanism and documents that it has no remedy:

> "Linearly dependent working sets do not break the loop: the delta/mu
> regularization keeps the KKT matrix factorizable, and the dependent rows split
> the multiplier between them. **No anti-cycling rule is implemented; a
> degenerate stall is bounded by max_iter.**"
> — `include/hven/detail/qp/qp_engine.h`, the `--- Degeneracy ---` note (~:333-338)

`QpCounters::degenerate_steps`' own doc states the same from the counter side:
"qp_engine.h implements no anti-cycling rule and this field changes no decision."

It is a hypothesis, not a diagnosis. A DEGENERATE_STALL verdict says what the
minors were spent on; it does not say which remedy is right, and this rule does
not pick one.

### No default is changed by this addendum under any outcome

Not `tr_init`, not `QpOptions::max_iter`, not `qp_cap_retry`, not
`restoration_proximity`, not `qp.tr_radius`, not `feas_tol`, and not any
`QpOptions` tolerance. **No verdict licenses a default change.** No file under
`src/` or `include/` is touched by this addendum at all: it adds a printing
argument to a test-only probe and reads a PUBLIC instrument.

**What a verdict IS for:** it RANKS engine work for the settler. The three
candidates, named here so the read-out is not mistaken for a recommendation
among them:

1. **An anti-cycling rule** — a documented textbook rule (Bland's) or a
   perturbation / EXPAND-style scheme from the OPEN literature. Never from SNOPT
   source: CLAUDE.md §6's firewall is absolute and applies to this line as to
   every other.
2. **Start / crash quality** — a better initial working set, so the walk does
   not begin at a vertex it must churn out of.
3. **Cap policy** — what `effective_qp_max_iter` should be, and what the driver
   should do with a capped subproblem.

Which of the three the numbers rank is the settler's call, not this rule's.

---

## 1. What the instrument is, and why it is new

### 1.1 The measurement gap addendum E reported, and why it is not a gap

`verdict-E.txt` and `README.md`'s closing sections recorded that
`QpCounters::schur_updates` — and with it the whole degeneracy set — "is NOT on
`SqpCounters` and is therefore not reachable from `SqpResult`". **That is true
and stays true**, and this addendum does not amend it: `SqpCounters` really does
not carry those fields, so nothing in `verdict-E.txt` is wrong.

What it missed is a SECOND public surface. `hven::solvers::Ledger`
(`include/hven/core/ledger.h`) is a public, read-only instrument;
`SqpSolver::attach_ledger(Ledger*, std::string)` is a public method
(`include/hven/drivers/sqp_solver.h:716`) that also forwards the same `Ledger`
to the driver's own `QpEngine`; and `Ledger::records()` returns
`const std::vector<SolveRecord>&`, one record PER QP-ENGINE SOLVE, each carrying
a whole `QpCounters` — `schur_updates`, `degenerate_steps`, `degenerate_run_max`,
`ratio_ties`, `drop_ties` and the rest included.

This is not a new invention for this addendum. The harness's own M7 W2 C1 cost
census already used exactly this surface (`hven-bench`'s
`runs/2026-09-m7-w2-c1-sqp-cost-census/probe/c1_probe.cpp`, read-only here),
producing a degeneracy table and a `ledger_schur_updates` column, and it proved
the attachment inert with a detached-vs-attached arm over 3 cells.

### 1.2 The ledger's SCOPE, stated before the legs

`SqpSolver::attach_ledger` attaches to `engine_` — the driver's main walk
`QpEngine` — and to nothing else (`src/drivers/sqp_solver.cpp:1700-1705`).
Consequently, and this is a fact about the record set, not a defect:

- **The restoration phase's sub-driver writes NO record.** `ledger.h` says so
  directly: it "receives attach_trace but no attach_ledger". At `tr_init = 1`,
  where a restoration phase runs, its subproblems are absent from `records()`.
- The lazily-built `ipqp_engine_`, `elastic_ipqp_engine_`, `ssn_engine_` and
  `cap_retry_engine_` are never handed the ledger either. At this lane's
  `default` options (`qp_mode == kWalk`) with no cap-retry, none of the four is
  constructed on the shipped path, but the statement stands regardless.

So `records().size()` is the number of solves the MAIN walk engine made, and is
a LOWER BOUND on the QP solves the whole solve performed. Every per-record
reading below is a reading about `engine_`. Record labels are the driver's own
sequence `<prefix>_qp_<n>`, printed verbatim, so a reader can see the ordering
rather than infer it.

### 1.3 The FIELDS, enumerated from the public header rather than chosen

`QpCounters` (`include/hven/core/solver_counters.h:31`) has exactly **twenty**
fields. **All twenty are printed on every record**, in declaration order, plus
the record's `label`, `warm` flag and terminal `QpStatus`:

```
factorizations      schur_updates       minor_iters         eqp_refine_steps
border_refine_steps verdict_refine_steps suspect_escalations k0_reused
symbolic_analyses   ws_adds             ws_drops            shift_adds
degenerate_steps    degenerate_run_max  ws_adds_bound       ws_drops_bound
distinct_ineq_added distinct_bound_added drop_ties          ratio_ties
```

Nothing is selected and nothing is summarised away. The probe prints the struct.

**TWO FIELDS THE RULING NAMES THAT `QpCounters` DOES NOT HAVE.** Stated here,
before the legs, rather than substituted for:

- **Negative-curvature counters: ABSENT.** `QpCounters` has no field naming
  negative curvature. The negative-curvature ride's own blocking constraint is
  folded into `ws_adds` by construction — that field's doc says so: "The
  negative-curvature ride's own blocker (section 4c) is counted here too, since
  it is the same ratio test reporting the same kind of event." There is
  therefore no reading of "how many rides happened", and **no neighbouring field
  is offered as one**.
- **Inertia counters: ABSENT.** `QpCounters` has no inertia field. `IpqpCounters`
  carries `ipqp_final_inertia_read`, but that is a different struct describing a
  different tier which does not run here, and it is **not substituted**.
- **Escalation counters: PRESENT, one.** `suspect_escalations` — rungs of the
  suspect-stall escalation ladder — is printed like the other nineteen.

### 1.4 Two DERIVED quantities, and they are derived, not read

Printed per record alongside the twenty fields, each marked as derived:

- `degenerate_fraction = degenerate_steps / minor_iters` — the ratio the §4
  verdict is stated in. Printed as `n/a (minor_iters = 0)` when the denominator
  is zero; see §4's undefined case.
- `ws_net_change = ws_adds - ws_drops + shift_adds` — which by the walk
  counters' own documented NET IDENTITY equals `W - S`, the working-set size at
  exit minus the size at entry. **The sizes themselves are not fields**:
  `SolveRecord` carries no entry or exit working-set size, so `W - S` is derived
  from that identity and never presented as a direct reading.
  `ws_adds_minus_drops = ws_adds - ws_drops` is printed beside it, since that is
  the quantity §4's descriptive read-out is stated in.

  **The identity's ONE documented exception, named before the legs**: the
  temporary-vertex start repair writes `ws.bound_state()` directly without
  touching `QpCounters`, so on a solve that ran it the identity is off by the
  pins the repair left standing. The repair runs only at `iter == 0` on an
  indefinite start vertex. The probe cannot observe whether it ran — there is no
  counter for it — so `ws_net_change` is reported as what the identity gives,
  with this caveat attached, and no leg's verdict depends on it (§4 makes it
  descriptive, never a verdict input).

---

## 2. The legs, fixed here

Five legs, in this order, each its own lock hold. Every leg runs this lane's
`default` label — hven's own defaults plus `common.threads = 1` and
`common.print_level = 10`, exactly as `lanes/hven/hven_bench_cell.cpp` sets them
— with ζ = 0 and `--deadline 300`, and with NO option off its default. The only
thing that varies across the four measured legs is `tr_init`, which is `RULE.md`
stage 1's own ladder argument, and `--ledger`, which changes no option at all.

| leg tag | cell | probe arguments beyond `--deadline 300` | its committed control |
|---|---|---|---|
| `STEENBRB@identity-F` | STEENBRB | `--problem STEENBRB --tr-init inf --ledger off` | `rows/stage1/STEENBRB@trinf.txt` |
| `STEENBRB@F-trinf` | STEENBRB | `--problem STEENBRB --tr-init inf --ledger on` | `rows/stage1/STEENBRB@trinf.txt` |
| `STEENBRB@F-tr1` | STEENBRB | `--problem STEENBRB --tr-init 1 --ledger on` | `rows/stage1/STEENBRB@tr1.txt` |
| `YORKNET@F-trinf` | YORKNET | `--problem YORKNET --tr-init inf --ledger on` | `rows/stage1/YORKNET@trinf.txt` |
| `YORKNET@F-tr1` | YORKNET | `--problem YORKNET --tr-init 1 --ledger on` | `rows/stage1/YORKNET@tr1.txt` |

`tr_init = 1` IS the shipped default and IS stage 1's control arm; `tr_init =
inf` is stage 1's `@trinf` arm. **No control is re-run for its own sake** — the
four `--ledger on` legs are themselves the re-runs, and §3's neutrality check is
what turns each of them into a comparison against its committed stage-1 twin.

Informational, from stage 1's log and quoted as informational only (CLAUDE.md
§7): the four arms ran 59.9 s, 31.6 s, 78.7 s and 97.3 s there (`logs/chain1.log`,
in the table's row order). Nothing in this
addendum is near the 300 s solver deadline, and a leg that nevertheless hits it
is TRUNCATED on §5's terms.

---

## 3. The two checks on the rebuilt probe

The probe gains ONE argument (`--ledger on|off`, **default off** = the E probe's
behaviour exactly) and one new printed key, `ledger`, plus a `LEDGER_BEGIN` /
`LEDGER_END` block emitted AFTER `LADDER_END`. The library is NOT rebuilt: the
E legs' install is re-used and its `libhven.a` sha256
`c79f404fd377f7ecdefc8ab016a71d65a8cba026a498b4f1f65ce303a72d5cd3` is re-checked
by the chain, which refuses a mismatch.

### 3.1 IDENTITY — the new argument is inert at its default

**`STEENBRB --tr-init inf --deadline 300 --ledger off`**, compared against the
committed `rows/stage1/STEENBRB@trinf.txt`.

Compared: every key present in BOTH `PROBE_BEGIN` blocks — the shared-key set —
and every ladder column present in both headers, row for row, as STRINGS (the
probe prints every float at `%.17g`, so string equality is value equality). The
`LEDGER_BEGIN` block is NOT part of the key block and is not compared: the
committed row has none.

Excluded, **by name and only by name** — this addendum's one new key:

```
ledger                   the --ledger argument, printed as a key
```

Time-valued fields are dropped BY NAME, the same total list `RULE.md` §4 fixed
and `RULE-E.md` §3 carried: `wall_seconds`, `elapsed`, `elapsed_seconds`. None
is printed by either binary today; the list is stated in full so a future key of
that kind is excluded by a committed rule rather than by a later judgement call.

Addendum E's five keys (`qp_cap_retry`, `qp_max_iter`, `qp_cap_retries`,
`first_qp_minor_iters`, `first_qp_factorizations`) are present in this binary and
absent from the stage-1 rows, so they fall outside the SHARED set by
construction. They are listed in the report as "only in the rebuilt probe" and
are not compared. That is the same treatment `RULE-E.md` §3's exclusion gave
them, reached by the shared-set rule rather than by a second exclusion list.

**The bar is byte equality**, with `RULE.md` §4's near-ulp fallback carried over
verbatim and unchanged: counters, statuses, integer- and enum-valued keys and
`x_k_source` are byte equality with no tolerance ever; residual-class floats and
columns accept `|a - b| <= 1e-12 * max(1, |a|, |b|)` as cross-process noise, and
**every such acceptance is printed with both values and the relative gap**.
Stage 1 and addendum E each needed none (`NEAR_ULP_TOLERATED=0`).

**An identity failure ABORTS the chain.** No F leg runs, and the failure is
reported as the finding.

### 3.2 NEUTRALITY — the attached ledger moves no solver counter

For each of the four `--ledger on` legs, the SAME comparison against THAT leg's
own committed stage-1 twin named in §2's table: the same shared-key set, the
same exclusion of `ledger` by name, the same ladder-block comparison, the same
near-ulp gate on the residual class alone. This covers the terminal `status`,
`f`, `feasibility_e`, `feasibility_i`, `infeasibility_certified`, and every
printed `SqpCounters` field — which is what "solver counters, status, f and h"
means here, stated by rule rather than by a hand-picked subset.

**IF ANY SOLVER COUNTER MOVES WITH THE LEDGER ATTACHED, THE ADDENDUM STOPS.**
That would be a finding ABOUT hven — a public, documented-inert instrument
perturbing a solve — and it is reported as one, with the differing keys and both
values. It is not worked around, not tolerated, and no H-DEG verdict is taken
from a leg whose neutrality check failed.

**Gating, fixed here.** `STEENBRB@F-trinf` runs FIRST of the four and its
neutrality check runs IMMEDIATELY after it, outside the lock. A failure there
ABORTS the chain before the remaining three legs run — the STOP is a stop, not a
note appended to a completed run. The other three are checked after the chain,
in the fold, on the identical machinery.

A near-ulp acceptance in the residual class is NOT a counter moving: the gate
touches residual-class floats only, counters and statuses are byte equality with
no tolerance, and every acceptance is printed.

---

## 4. The decision rule

Evaluated on the **FIRST QP-LEDGER RECORD** — `Ledger::records()[0]`, the main
walk engine's first solve — of the **`tr_init = inf` leg of each cell**
(`STEENBRB@F-trinf`, `YORKNET@F-trinf`). Those two legs, that record, and
nothing else.

Let `d = degenerate_steps` and `m = minor_iters` on that record, and
`r = degenerate_run_max`.

| verdict | condition |
|---|---|
| **DEGENERATE_STALL** | `d / m >= 0.5` **AND** `r >= 1000` |
| **GENUINE_WALK** | `d / m < 0.1` |
| **MIXED** | anything else — reported WITH the numbers |

The three are evaluated in that order and are exhaustive over `m > 0`: a record
with `d/m >= 0.5` but `r < 1000` is MIXED, and so is every record with
`0.1 <= d/m < 0.5`.

**The undefined case, pre-registered.** If `m == 0` on that record — or if the
leg produced no record at all — the ratio does not exist and the cell is
**UNDETERMINED**, reported as "the first ledger record has no minor iterations"
or "no ledger record", with the record's other nineteen fields printed anyway.
No verdict is taken by substituting a neighbouring quantity.

### 4.1 The descriptive read-out — NOT a verdict input

Pre-registered here so it cannot be read as one later. On the first QP of each
of the four measured legs, the fold reports:

- `ws_adds`, `ws_drops`, and `ws_adds_minus_drops = ws_adds - ws_drops`;
- `ws_net_change = ws_adds - ws_drops + shift_adds`, which by §1.4's net identity
  IS the working-set size at exit minus the size at entry, with §1.4's
  start-repair caveat attached;
- the plain statement of whether `ws_net_change` is zero, i.e. whether the
  working set at exit is the same size as at entry.

**The signature this describes**: adds ≈ drops with no net progress is what
cycling looks like. It is DESCRIPTIVE. It enters no verdict, it cannot make a
MIXED cell a DEGENERATE_STALL one, and a reading of "adds ≈ drops" is not
evidence of cycling on its own — a walk can churn without cycling.

### 4.2 What is reported regardless of verdict

For **every** leg: every field of §1.3 on the first record; the same twenty
fields AGGREGATED over all records of that leg (sums, except `k0_reused`, which
is a bool and is aggregated as a COUNT of records where it read true, and
`degenerate_run_max`, which is a PEAK and is aggregated with `max`, never
summed); the record count; the per-record table; each record's terminal
`QpStatus`; and the leg's `deadline_fired`.

### 4.3 The failure path

If both cells are UNDETERMINED, or if the neutrality check of §3.2 fails on any
leg, **the addendum reports that and stops.** H-DEG is then recorded as not
measured on these cells. It is not re-run at another radius, not widened to
other cells, not re-taken on a different record, and not rescued by relaxing the
thresholds. A rule relaxed after seeing the data is not a rule.

**No default is changed by this addendum under any outcome**, and no verdict
licenses one. See §0.

---

## 5. Budgets, censoring, and truncation

Carried from `RULE.md` §6 WITHOUT CHANGE, restated so this addendum is
self-contained.

**Per leg: a 300 s SOLVER deadline inside a 900 s OUTER stop.** The solver
deadline is enforced by the probe in its iteration callback and prints
`deadline_seconds` and `deadline_fired` on every leg; the outer stop is
`timeout 900` around the process, for a solve that spends its whole budget
inside one subproblem and emits no event the deadline could fire on.

- **TRUNCATED** — `deadline_fired = 1`. The leg's counters and its ledger
  records ARE reported, and the leg yields **NO VERDICT** under §4. A TRUNCATED
  leg's neutrality check is also withheld: an interrupted solve is not the
  committed control's solve, and a difference there is the interruption, not the
  ledger. Such a leg is reported as TRUNCATED, with its numbers, and that is the
  whole record.
- **CENSORED** — the 900 s outer stop fired (rc 124), or the leg exited non-zero
  for any other reason. **NO ROW.** Never filled, never interpolated, never
  replaced by a re-run at a different budget, never averaged with a neighbour.
  The leg is listed with its cell, its arm and its rc, and that is all.
- A TRUNCATED or CENSORED **identity** leg aborts the chain exactly as a §3.1
  identity failure does: it cannot prove the rebuilt binary reproduces a row
  that ran to completion.

**The per-record PRINT CAP, declared before the legs.** The probe prints at most
**5000** per-record lines and then one `ledger_records_print_truncated=1` marker
line. This is a PRINT cap on the artifact's size, not a measurement cap: the
aggregates of §4.2 are computed over **every** record whatever the cap does, and
the first record — the only one any verdict reads — is always printed. Stage 1's
majors and elastic activations put the expected record count in the tens, so the
cap is not expected to bind; it is registered so that a surprise is a marker in
the file rather than a judgement call at fold time.

**Measurement conditions, declared.** One leg per process, serial, ALONE under
`flock /tmp/box-build.lock` with no timeout and no fallback, released per leg;
`MKL_NUM_THREADS=1`, `OMP_NUM_THREADS=1`; pinned to one physical core with
`taskset -c 2`; a `pgrep` check for a foreign cell binary as a separate prior
command, with a `PGREP_EMPTY` marker inside the lock. These are the serial terms
of CLAUDE.md §7, not the co-run terms: **nothing here co-runs.** Wall-clock is
INFORMATIONAL everywhere and is quoted as a measurement nowhere; counters and
statuses are the currency.

**HOLDOUT is never run.** Both cells are TRAIN.

---

## 6. What gets committed, and in what order

1. This file — **before any leg, including the identity leg**. Done at this
   commit.
2. The probe's `--ledger on|off` argument and its `LEDGER_BEGIN` block, the
   identity/neutrality checker, and the chain wrapper.
3. The raw leg output, one file per leg, under `rows/f/`, unedited.
4. A machine-written fold — `wrappers/summarise_m3a.py` extended with the F
   fold — writing `verdict-F.txt` and `tables/f-*.txt`. **No number in the
   artifact transcribed by hand.**
5. An "Addendum F" section appended to `README.md` and `PROVENANCE.txt` (the new
   probe sha), and §10 appended to `.superpowers/m7-w2-m3a-report.md`, whose
   `SIGNOFF: M7-W2-M3A-IMPLEMENTER-REPORT` line stays LAST.

The stage-1 and addendum-E folds are NOT re-derived: re-running the summariser
must leave `verdict.txt`, `verdict-E.txt` and the existing `tables/` files
byte-identical to what is committed. A move there would be a silent pin mutation,
which CLAUDE.md §7 forbids.

Provenance: the F legs run on a REBUILT probe against the SAME hven install as
stage 1 and addendum E. The new probe's own sha256 differs by construction and is
recorded in `PROVENANCE.txt` alongside the other two, with §3.1's identity leg as
the proof the difference is inert at the default. Nothing built here carries
`HVEN_BENCH_PIN_SHA` — the harness CMake is never entered — so **no row from this
probe is ever cited as a harness row**, and `/home/ghecht/Projects/hven-bench` is
read-only to this addendum as it was to stage 1 and to addendum E.

No file under `src/` or `include/` is modified by this addendum, and `notices/`
is untouched.
