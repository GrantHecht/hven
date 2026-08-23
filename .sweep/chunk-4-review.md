# Chunk 4 comment-sweep review — `include/hven/detail/{kkt,linear,warmstart,solvers}` + `include/hven/linear`

Repo `/home/ghecht/Projects/hven-sweep`, commit `2d4e445` over `aafc053`. Read
via `git show`/`git diff` only; working tree untouched. All line numbers are at
`2d4e445`.

## Verdict

**APPROVE WITH REQUIRED FIXES.** The sweep is accurate at a much higher rate
than chunks 1–3b: on the four warmstart headers (where 570 of the 1029 deleted
comment lines live) the surviving contract statements check out against the
code, the sIpopt/qpOASES/Apple/Intel attributions are kept explicit rather than
converted into hven claims, and two pre-existing wording defects were corrected
in passing (`kDualSignTol` "absolute" → relative-with-floor, `fd_step_scale`
"> 0" → "> 0 and finite"). The mechanical gates hold. Eight findings must be
fixed before merge; seven of the eight are one-line edits.

### Gates I re-ran independently

| Gate | Result |
|---|---|
| Comment-stripped code-token identity, all 17 files | **PASS** — 0 of 17 files differ (own tokenizer, string/char-literal aware) |
| MPL notice block `accelerate_session.h` lines 1–38 | **PASS** — byte-identical |
| MPL/BSD notice block `pardiso_session.h` lines 1–68 | **PASS** — byte-identical |
| Residual labels (`Task N`, `Phase-N`, `M3`, `§`, `fix round`, `WAVE`, `FX-n`, `S-n`, `T6`, `O-1`, `B-1`) | **PASS** — zero hits across the 17 files |
| Comment-line totals | 5046 → 4017 (report says 5042 → 4013; a 4-line counting-method delta, see F16) |

### Counts

- Findings: **16** — 8 blocking, 8 non-blocking.
- Files with a blocking finding: 6 of 17 (`fault_injection.h`, `kkt_calls.h`,
  `predictor.h`, `continuation.h`, `mesh_transfer.h`, `warm_start.h`).
- Files clean: `border_ops.h`, `kkt_assembly.h`, `accelerate_session.h`,
  `lapacke_shim.h`, `pardiso_session.h`, `session_id.h`,
  `solver_interface_specs.h`, `dense_symmetric_factor.h`.

---

## Blocking findings

### F1 — `fault_injection.h:24-26` — "Two such deviations" now enumerates one — **BLOCKING**

The sweep collapsed a two-item parenthetical to one item but kept the count and
kept "both".

Now reads:

> ```
> // goes where the fact is, with the reason written down. Two such deviations
> // exist today (PardisoIparmObserver's did-the-write-execute fields); both are
> // argued in full in docs/testing.md.
> ```

Original named both: the did-the-write-execute fields *and* the
`post_pardisoinit_*` fields, "recorded at a different line in the same function
for a different reason". Both deviations still exist in the file — the
`post_pardisoinit_*` block at lines 158-183 documents itself as exactly such a
deviation ("Recorded instead at the one line inside the session file where the
fact is still true"). This is the chunk-1–3b failure class *accounting that lost
a case*, and it matters here because the count is the MPL-deviation ledger.

Correct text:

> `... with the reason written down. Two such deviations exist today
> (PardisoIparmObserver's did-the-write-execute fields, and its
> post_pardisoinit_* fields, recorded at a different line in the same function
> for a different reason); both are argued in full in docs/testing.md.`

### F2 — `kkt_calls.h:78-90` — `@throws` sits on the delegating overload, not the throwing one — **BLOCKING**

`src/kkt/kkt_calls.cpp` puts the `throw std::runtime_error` inside the **3-arg**
`factorize_checked(k, K, decision)`; the 2-arg overload is
`return factorize_checked(k, K, analysis_decision(k, K));`. The sweep tagged the
2-arg declaration:

> ```
> /// @brief Analyze iff the pattern changed, then factorize.
> /// @throws std::runtime_error If the outcome is
> /// FactorizeOutcome::Status::kBackendError; every other status is returned.
> hven::linear::FactorizeOutcome factorize_checked(KktFactor &k, const SpMatRM &K);
> ```

and left the 3-arg declaration (lines 83-90) with **no `@throws` at all**. The
comment rules state it directly: "`@throws` goes on the declaration that throws,
never on a neighbour." The report's uncertainty item 1 says both were verified
against the .cpp, so the verification simply did not reach the tags.

Fix: add the identical `@throws std::runtime_error` clause to the 3-arg
overload's docstring (keeping it on the 2-arg is fine — it does throw
transitively).

### F3 — `predictor.h:445` — `THROWS` domain dropped the non-finite `fd_step_scale` case — **BLOCKING**

The rewritten `predict()` throws list still reads:

> `// the wrong size or a non-finite dp, or a non-positive fd_step_scale. A`

but the guard at `predictor.h:549` is
`if (opts.fd_step_scale <= 0.0 || !std::isfinite(opts.fd_step_scale))`, and its
message is "must be finite and > 0". The sweep *corrected* the sibling field
comment at line 230 in the same commit ("Must be > 0 and finite."), so the two
statements in one header now disagree. This is precisely the `@throws`
lost-a-qualifier class flagged from chunks 1–3b.

Correct text: `... or an fd_step_scale that is not finite and positive.`

### F4 — `predictor.h:93-95` — RELAX note dropped "NUMERICALLY", making a claim that is false on Accelerate — **BLOCKING**

Now reads:

> ```
> //         the frozen z subtracted from its right-hand side, which forces the
> //         released multiplier to land at zero instead of staying at its
> //         frozen value
> ```

The original said "land at **NUMERICALLY ZERO**" and then spent a paragraph on
exactly why the unqualified word is wrong: on MKL Pardiso the cancellation lands
at exactly `0.0` bit-for-bit, on Accelerate it leaves
`-7.7037197775489434e-34`. That is a cross-backend numerical caveat with a live
test arm (`test_predictor.cpp`'s D18 arm), i.e. rule-3 keep-material, not
rationale. The sweep correctly weakened the *other* occurrence
(`predictor.h:712`, "to exactly zero" → "to zero") but strengthened this one by
deleting the qualifier that made it true.

Correct text: `... which forces the released multiplier to land at numerically
zero (exactly 0.0 on MKL; an O(1e-34) residue on Accelerate) instead of staying
at its frozen value`.

### F5 — `kkt_calls.h:58` — hash-count floor changed unit from "per SSN major" to "per solve" — **BLOCKING**

Now reads:

> ```
> // the call sites keeps steady state at the floor of two O(nnz) hashes per
> // solve (this decision plus SymmetricFactor::factorize()'s own pattern
> // guard, ...)
> ```

The original quantity was **three vs two O(nnz) pattern hashes per SSN major**
(the B1 pricing that motivated threading the decision through at all). A solve
runs many majors, so "two hashes per solve" understates the steady-state cost by
the major count and makes the floor claim unfalsifiable. Units/branch loss is
one of the named failure classes.

Correct text: `... keeps steady state at the floor of two O(nnz) hashes per SSN
major ...`

### F6 — `continuation.h:412-415` — the `~1.8x` clearance is attached to the wrong quantity — **BLOCKING**

Now reads:

> ```
> // fails a broad set of existing tests). The value 200 clears by ~1.8x the
> // largest legitimately-cheap step observed on the binding control sweep at
> // N = 2000 nodes, where a floor of 100 measurably moved a healthy-corpus
> // cell.
> ```

In the original derivation the binding control's step **cost 126 minors** in
total, and the number 200 clears is **110** — the minors already spent at the
*top of its third major*, which is where the budget test fires. 200/110 = 1.82;
200/126 = 1.59. As rewritten, the sentence pins 1.8x to the 126-minor step, so
the stated ratio does not follow from the stated quantity, and a future retuner
recomputing it will get a different number and conclude the comment is stale.

Correct text: `The value 200 clears by ~1.8x the largest pre-convergence spend
observed on the binding control sweep at N = 2000 nodes (110 minors at the top
of the step's final major), where a floor of 100 measurably moved a
healthy-corpus cell.`

### F7 — `mesh_transfer.h` — deleting "THE PHASE-6 INGEST GAP" orphaned two live cross-references — **BLOCKING**

The section is gone from `mesh_transfer.h` (it is in the report's rationale list
as `mesh_transfer.h:309-375`), but two files outside this chunk still send the
reader to it by title:

- `include/hven/core/start_level.h:63` — `// mesh_transfer.h's THE PHASE-6 INGEST GAP; kSeeded is the driver-side change`
- `tests/sqp/test_mesh_transfer.cpp:446` — `// WORK.** This is the measurement mesh_transfer.h's PHASE-6 INGEST GAP note`

Criterion (c) fails outside the chunk. Two options, either acceptable: (a) keep
a two-line named anchor in `mesh_transfer.h` section 4 carrying the title and
the one surviving fact ("the hash-less object reaches the solve at kSeeded;
factorization reuse is what the sentinel protects"), or (b) amend those two
call-sites in a follow-up to point at section 4's ingest paragraph. Do **not**
leave both dangling.

Note the *intra*-file reference to it was correctly repointed
(`mesh_transfer.h:129` now says "see section 4's ingest note", which resolves),
so this is a scan-outside-the-chunk gap, not a careless edit.

### F8 — `warm_start.h:462-476` — malformed Doxygen block on `from_interior_point` — **BLOCKING**

The declaration now carries a plain `//` block immediately followed by a `///`
block, and the `@brief` is a preconditions statement:

> ```
> // Builds a WarmStart from an interior-point-style primal-dual point. See
> // this header's own note immediately above for the sign convention ...
> //
> /// @brief Dimensions: n = x.size(), me = lambda_e.size(), mi =
> /// lambda_i.size(). slack_i must match mi; z_lower, z_upper, lower and upper
> /// must each match n.
> /// @throws std::invalid_argument On any size mismatch ...
> ```

Doxygen sees only the `///` half, so the generated brief for this public
function becomes "Dimensions: n = x.size()…" and the actual summary sentence is
invisible. This is the one `// … ` → `/// …` adjacency in the whole chunk (I
scanned all 17 files; it is unique), so it reads as an accident.

Correct shape:

> ```
> /// @brief Builds a WarmStart from an interior-point-style primal-dual point.
> ///
> /// See this header's own note immediately above for the sign convention
> /// `slack_i` must already be in (cI(x) VALUES, NOT an IP solver's own
> /// non-negative slack), the activity-inference rule, and what this object's
> /// fields do and do not carry. Dimensions: n = x.size(), me =
> /// lambda_e.size(), mi = lambda_i.size(); slack_i must match mi; z_lower,
> /// z_upper, lower and upper must each match n.
> /// @throws std::invalid_argument On any size mismatch (offending sizes in
> /// the message).
> ```

---

## Non-blocking findings

### F9 — `symmetric_factor.h:282` and `:420` — two lines of an aligned table lost their indentation — MINOR

> `//         std::nullopt  MKL: iparm[9] is never touched -- pardisoinit's`
> `//         kTwoByTwo              iparm[20] = 1  "Apply 1x1 and 2x2`

Both should start at `//   ` like their siblings (`//   kOneByOne`,
`//   kOneByOneNoAutoRefine`, and the present-value row above `std::nullopt`).
As it stands the two rows sit six columns right of the column they belong to,
breaking the only two-column tables in this option block. Cosmetic, but these
are the only two mangled lines in a file that otherwise lost six comment lines
total, so they read as reflow slippage rather than intent.

### F10 — `warm_start.h:338` — citation to a note that does not exist in this repo — MINOR

> `// measured in docs/notes/2026-08-06-activity-tol-repair.md. The dual-side`

`docs/notes/2026-08-06-activity-tol-repair.md` is not in the tree at `2d4e445`
(it is an origin-project note that did not migrate; the repo's `docs/notes/`
holds only 2026-08-14 and later). Every other `docs/notes/` citation in this
chunk was removed to the report's rationale list — this one survived. Either
drop the path (the sentence stands without it) or move the measurement to the
rationale list like its siblings.

### F11 — three stale test paths survive rewritten sentences — MINOR (pre-existing)

`kkt_assembly.h:74` → `tests/test_qp_engine_border.cpp`,
`continuation.h:234` → `tests/test_continuation.cpp`,
`predictor.h:65` → `tests/test_predictor.cpp`. All three live under
`tests/sqp/` in this repo. Pre-existing, not introduced, but each sits in a line
the sweep rewrote, so it was in scope to fix. (`fault_injection.h:268`'s
`tests/linear/test_symmetric_factor.cpp` is correct.)

### F12 — `mesh_transfer.h:222` — "warm_start.h's StartLevel note" resolves to `core/start_level.h` — MINOR (pre-existing)

`StartLevel` and its take/refuse list are defined and documented in
`include/hven/core/start_level.h`, not `warm_start.h`. Pre-existing; the sweep
kept the sentence and shortened it. Retarget to `start_level.h` while the file
is open.

### F13 — `bordered_eqp.h:59` — the softened pinned-scatter sentence loses the magnitude that made the argument — MINOR

(Report uncertainty item 8 asked for this judgement.) Original: "a pin's dual is
a regularization artifact of size ~1/mu, which makes that gap O(1) rather than
O(1e-8)". Now: "a pin's dual can be large, making that gap material." The
softening is not *wrong*, but "material" is not checkable and the whole reason
the unconditional overwrite is defensible is that the gap is O(1), not O(mu). I
would restore the magnitude: `... a pin's dual is a regularization artifact of
order 1/dual_mu, so that gap is O(1) rather than O(dual_mu)`. Also note the
sweep dropped the parenthetical defining *when* the working sets are
over-determined ("every variable pinned AND a working row still demanding
something of them"), which was the reader's only way to know the case is
reachable.

### F14 — `bordered_eqp.h:159` — a measured property of one reproduction became a general claim — MINOR

> `// well-conditioned unregularized system -- converges only geometrically.`

The original stated it as an observation ("the system it targets is well
conditioned: cond ~1.5e2 in the reproduction"). "The well-conditioned
unregularized system" now asserts the property in general, which nothing in the
file establishes. Suggest `... refinement -- whose target system was well
conditioned in the reproduction -- converges only geometrically`.

### F15 — `schur_complement.h:38` and `:113` — two consequence clauses deleted with their premises — MINOR

- `:38` keeps "the cap is detected by the add that crosses it" but drops the
  clause it explains ("so one border beyond the cap is always paid for"), and
  drops the caller-facing consequence entirely ("a caller that ignores
  `needs_refactorization()` will simply grow C without bound and pay the
  O(dim()^3) rebuild for it"). The `dim()` peak claim survives but is now
  asserted rather than derived.
- `:113` drops the parenthetical explaining why a torn length is *dangerous*
  ("drop_border's per-array erase and rebuild_schur's C assembly both index all
  three at `dim()`, so a length skew is an unguarded out-of-bounds read in
  Release, not a degraded answer"). That is a Release-only memory-safety
  consequence, i.e. rule-3 material.

Both are judgement calls rather than inaccuracies; I would restore the `:113`
parenthetical at minimum.

### F16 — report table off by 4 comment lines — INFORMATIONAL

My count is 5046 → 4017 (`bordered_eqp.h` 194→159, `predictor.h` 871→635); the
report says 5042 → 4013 with 192→157 and 869→633. A counting-convention delta on
continuation-`*` lines, not a discrepancy in what was deleted. Worth
reconciling only so the chunk tables stay comparable across chunks.

---

## Report's "docstrings written under uncertainty" — adjudicated

| # | Claim | Verdict |
|---|---|---|
| 1 | `kkt_calls.h` `@throws` on both `factorize_checked` overloads | **Contradicted** — only the 2-arg carries it; the throwing 3-arg has none. See **F2**. |
| 2 | `kkt_assembly.h` "The QP engine … SUBTRACTS rhs_shift" | **Confirmed** — `eqp_solve.h:174/177/181` build the rhs as `-g − rhs_shift`, `be − rhs_shift`, `bi − rhs_shift`. Actor is fair; naming `eqp_solve.h` would be tighter. |
| 3 | `warm_start.h` producer inventory vs `sqp_driver.h` | **Confirmed** — matches `sqp_driver.h:441-443` clause for clause ("a ZERO-MAJOR exit re-emits the ingested duals as cleared by this very block; and a restoration exit carries the sub-solve's own subgradient selectors"). The added clause "predictor.h … clamps its emitted prices to be non-negative outright" is also confirmed at `predictor.h:1183-1185` (`if (out.lambda_i(j) < 0.0) out.lambda_i(j) = 0.0;`), and is *more* accurate than the text it replaced. |
| 4 | `predictor.h` `max_activity_rounds` qualitative degradation, both directions | **Confirmed** — the deleted table's two directions (uncapped drops rows that should stay active; uncapped raises the consuming solve's minors, 37 vs 9) both survive. |
| 5 | `predictor.h` VALIDATE-THEN-CATCH generalization | **Fair** — `DenseSymmetricFactor` does throw both `std::invalid_argument` (dense_symmetric_factor.cpp:44/52/57/184/189) and `std::runtime_error` (:87/99/150/180/231), so "has reported both illegal-argument and runtime failures" holds. The phrase "under different names" is vague; "under different exception types" would be clearer. |
| 6 | `continuation.h` RETRY ECONOMICS / floor qualitative | **Partly contradicted** — the qualitative retry-economics text is fair; the floor's `~1.8x` is misattached. See **F6**. |
| 7 | `solver_interface_specs.h` two new `@brief` docstrings | **Confirmed** — both structs contain nothing but the pure-virtual `Concept`, which is exactly what the docstrings say. |
| 8 | `bordered_eqp.h` pinned-scatter softening | **Loses something** — see **F13**. |

## What the sweep got right, worth recording

- Attribution discipline held throughout: sIpopt keeps the fix-relax scheme and
  the border-per-repair idea (`predictor.h:30-32`, `:80`), qpOASES keeps the
  parametric active-set homotopy (`predictor.h:113-115`), Apple keeps
  `zeroTolerance = 1e-4 * eps` as *its* documented default
  (`symmetric_factor.h:288-296`), Intel keeps the quoted `iparm` codes. Nothing
  became an hven claim.
- The MPL-derived files kept every ownership/lifetime callout that matters:
  session-is-the-unit-of-sharing, `adopt()`'s Options round trip, the
  `num_threads`-is-the-only-movable-entry invariant, the fork/`session_id`
  discriminator, and Accelerate's `SparseCleanup`-even-on-failure rule.
- `warm_start.h`'s `hot` ownership contract (shared_ptr keeps a live backend
  session alive; SAME-PROCESS ONLY; null on every predictor output) survived a
  272-line deletion intact, as did `mesh_transfer.h`'s
  `funnel_width`-declined-but-`tr_radius`-carried asymmetry with its full
  reasoning.
- `fault_injection.h`'s HVEN_TESTING inertness proof and its adapter-boundary
  preference-with-escape both survive; the file-level "OBSERVER" → "OBSERVERS"
  correction is a real fix (there are two).
- Two genuine accuracy repairs shipped: `predictor_detail::kDualSignTol`'s
  self-contradictory "absolute (scaled by …)" became
  relative-with-absolute-floor (verified against `predictor.h:985` and `:1007`),
  and `PredictorOptions::fd_step_scale` gained the finiteness precondition the
  guard actually enforces.
