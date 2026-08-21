# M3 Gate A — execution review request (phase A: verbatim import)

**From:** the M3 implementer (hven). **To:** the SQP instance (mandatory
execution reviewer), via Grant. **Plan of record:** the reviewer-side
`docs/notes/2026-08-14-hven-m3-plan-revB.md` §3 (phase A) and §11, in the
archived engine tree.

**Branch:** `m3` (PR #5, draft, opened at milestone start for CI coverage).
**Base:** hven main `c6635e1` (the M2 merge). **Source imported:** tag
`phase-7-close` = `4faa1df116da53c9dc68f36635c118f52d39d2b9`, tree verified
docs-only after the tag.

## Commit sequence (each step separately reviewable)

1. `6c44279` — §1.3 rehoming: `solver_interface_specs.h` →
   `include/hven/detail/solvers/` (4 edit sites; existing suite 429/429 after).
2. `c2b770f` — byte-verbatim import: 22 headers → `include/hven/detail/sqp/`,
   tests → `tests/sqp/`, bench harness → `bench/`, the 5 test-pinned baseline
   CSVs → `bench/baselines/` (every file cmp-verified against the tag).
3. `95af33d` — identity collapse (mechanical): the origin namespace →
   `hven::solvers`; includes; the origin macro prefix → `HVEN_SQP_*`;
   origin-prefixed target/binary/schema strings → `hven_sqp_*`; origin-name
   and PSIOPT prose per the retirement ruling (M2's own practice matched:
   historical filename citations into the archive kept verbatim). Baselines
   untouched (pinned artifacts keep origin strings, per the M2
   arm-label/schema-key ruling class).
4. `b409157` — the ONE forced semantic edit, isolated: SQP funnel
   `kFunnelBeta`/`kFunnelKappa` nest under `detail::` (ODR collision with the
   IPM funnel's same-named inline constexprs at different beta, 0.9999 vs
   0.99). Sanctioned mechanism per plan §2. 3 + 25 use sites qualified; no
   value changed. This was the ONLY top-level name collision found (full
   flattened-name intersection of the two engines' headers).
5. `cf65a03` — build wiring: `tests/sqp/` + `bench/` CMake, root SNOPT
   oracle detection (`HVEN_SQP_SNOPT_DIR`/`HVEN_SQP_HAVE_SNOPT`),
   UNIX-only gating.
6. `0ba1b9f` — the phase-A review round (an Opus code-review agent over
   commits 1-5; its verbatim-import check independently reconstructed the
   mechanical rewrite from the declared rule set and matched, both
   directions): retired the stale M2 HVEN_BUILD_BENCH scaffolding guard
   (must-fix — the option aborted the configure), clang-formatted the 4
   files the collapse pushed past the column limit, re-rooted 66
   tests/support/ path citations, fixed one stale scrub line.

## Decisions the review should pass on

- **Phase-A home `include/hven/detail/sqp/`** (flat, names unchanged):
  mechanism-dir placement deferred to phase C where the plan's collision rule
  makes it a reviewed choice; the KktSystem trio never leaves detail/ (phase
  B dissolves it).
- **Compile flags: sandbox-matched on the optimization flags, NOT purely
  sandbox-matched.** The origin suite compiled at plain
  `-O3 -DNDEBUG -std=c++20` (no `-march=native`, no inline-threshold, no
  omit-frame-pointer); every pin/baseline was derived there, and the
  migrated suite matches that. But hven's repo-wide directory definitions
  (EIGEN_INITIALIZE_MATRICES_BY_ZERO, EIGEN_DONT_PARALLELIZE,
  EIGEN_MAX_ALIGN_BYTES=32, FMT_USE_LOCALE=0) **DO apply to the migrated
  suite and did NOT apply in the sandbox** — so the posture is
  sandbox-matched-plus-those-defines, and saying "sandbox-matched" flat
  would overstate it. What makes the difference acceptable is not the rig
  precedent alone (the rig does compile the old seam under the same fixed
  environment with no row moving, testing.md) but the **empirical
  inertness proof phase A itself produced**: the full suite, float pins
  included, and the 57-cell census are byte-identical under those
  defines. Full compile-flags diff:
  `docs/notes/data/2026-08-14-m3-gate-a/compile-flags-diff.txt`. Question
  deferred to this review:
  whether/when the suite moves to unified flags (phase C compiles engine TUs
  into the library under library flags, so it lands there at the latest, as
  a declared re-derivation if any pin moves).
- **Bench built whenever the suite is** (not only HVEN_BUILD_BENCH): the
  suite's WallDeadline subprocess tests invoke the corpus binary, and count
  parity requires it in every configure shape that runs the suite; probes
  ride along per the anti-rot rationale. Flag if you want a different shape.
- **Windows excluded** (tests/sqp + bench): fork/exec in the corpus runner;
  the suite has never existed on Windows. Enablement is future work, not a
  migration step.
- **macOS enabled**: the suite's Accelerate halves compile and run in the
  macOS CI lane (first Apple execution of this increment is gate B's Mac
  leg; CI results before then are watched, not asserted against pins —
  float pins are context-pinned to this machine anyway).

## Gate A evidence

- Full SQP suite, SNOPT-enabled shape (this machine, Release,
  MKL_NUM_THREADS=1): whole-repo ctest 1118 registered = 429 existing +
  689 SQP; 0 failures; SQP contribution 687 executed+passed + 2
  permanently DISABLED_ (EqpRefinementAb.*) — exactly the archived tag's
  figure. Count arithmetic exact (phase A adds 0 tests). Debug
  (linux-clang-debug preset, run by a delegated verification agent):
  1118 registered, 0 failures out of 1116 counted, the did-not-run set
  exactly the expected four (2 designed skips + the 2 permanently
  DISABLED_), suite wall 1285 s.
- no-SNOPT shape (same tree, -DHVEN_SQP_SNOPT_DIR=/nonexistent): 1114
  registered (SQP 685), 0 failures (SQP 683 passed + 2 disabled) —
  exactly the plan §8.2 no-SNOPT figure.
- ScaleF7Slow run explicitly: Release 6/6 passed, 52.6 s; Debug 6/6
  passed, 915 s (confirms the plan's "six excluded" figure; the suite
  grew from the origin's documented three members to six by phase 7,
  dominated by F7ProposalFailureEconomics at 568 s Debug).
- 57-cell walk census, LIVE re-sweep on the migrated engine (~14.7 h wall
  end-to-end; DNF cells burn their 3600 s/phase budgets): 56/57 cells
  byte-identical, one adjudicated wall-deadline flip — full result and
  adjudication in the subsection below.
- `hven_sqp_bench --self-check`: PASSED in Release AND Debug, every
  counter at its expected value (steps 6, majors 30, minors 84,
  factorizations 25, ...).
- Golden rig: native arms green in every full ctest above. Two-seam run
  (native + sqp-old; the psiopt-old arm is blocked by its own pin — tycho
  HEAD moved to its M2 merge, and the rig pins that seam to pre-M2
  060ad213 — which is a tycho-side derivation matter, not a phase-A one):
  1135 tests, EXACTLY the one designed failure
  (P5_InertiaBeforeFactorizationIsAnExplicitState on sqp-old@mkl, the
  documented fail-by-design), FailByDesignControl tests green. The
  sqp-old arm configure-verified at the archived tag
  (4faa1df116da…, tree state verified). The rig itself is untouched by
  phase A (no file under tests/golden_rig/ changed).
- MKL-in-callback verification (plan §7, our half): DONE — zero
  MKL/LAPACK/BLAS symbols in tests/sqp/support/, corpus_cells.h,
  bench_cli.h (the evaluation-callback code is Eigen-only). The engine's
  real MKL surface is exactly kkt_system*.h + lapacke_shim.h (dissolve at
  phase B) + schur_complement.h's dense LAPACKE border (retargets onto
  DenseSymmetricFactor); predictor/ssn_engine/qp_engine mention MKL in
  comments only. SQP-side exposure of M2 parked item m3 closed; tycho's
  half stays on the tycho M2.5 owed-bench list.

### The 57-cell walk census

**Live re-sweep vs
`bench/baselines/2026-08-06-corpus/walk_baseline.csv`:** 57/57 cells
completed (serial, alone, `MKL_NUM_THREADS=1`); the 13 asserted
counter/status columns are byte-identical on **56/57** cells. Merged rows and
the comparator output are committed at `docs/notes/data/2026-08-14-m3-gate-a/`
(`walk_census_gateA.csv`, `compare.txt`).

The single delta is `f7_n10000_path_physics`: the baseline records
`dnf_budget` (SIGKILLed at the 3600 s wall deadline; every counter column is
a `-1` sentinel), while the fresh run reached `Optimal` at 2813.1 s wall
(8798 majors, 19311 minors, KKT 1.400e-10). We adjudicate this a **wall-clock
boundary flip, not a behavioral delta**, on four grounds:

1. The other 10 baseline DNF cells (6 remaining `dnf_budget` + all 4
   `dnf_setup`) reproduced exactly — the deadline mechanism itself behaves
   identically.
2. The fresh environment is systematically faster at identical numerics:
   across the 42 mutually-`Optimal` timed cells the fresh/baseline wall ratio
   is median 0.684 (min 0.528, max 0.994), with every counter column
   byte-identical. At that throughput a baseline solve that exceeded 3600 s
   lands at roughly 2450–2900 s — bracketing the observed 2813 s.
3. Both later frozen resweeps of this walk (2026-08-08 gate battery,
   2026-08-09 resweep) also record `dnf_budget` for this cell: the
   origin-side runs consistently sat just over the deadline, i.e. this is
   consistent machine-side throughput, not cell flakiness.
4. No asserted value is contradicted: the baseline row carries only `-1`
   sentinels, so there are no counters to mismatch — the entire difference
   lives in a status whose content is a wall-clock measurement, and
   wall-clock is informational per the measurement discipline (CLAUDE.md §7).

Proposed ruling for the reviewer: the census satisfies Gate A's byte-identity
requirement on every cell where the baseline asserts numerical content, and
the one flip is a favorable wall-deadline artifact. If the reviewer prefers
the cell pinned rather than left deadline-dependent, the fresh row's counters
are available for a **declared** baseline amendment (per the "intentional
breaks are declared and re-derived explicitly" rule) — we recommend deciding
that at Gate B when the census re-runs anyway.

## Notes for the record

- Two mid-phase directives from Grant, both absorbed: TU-splitting continues
  the IPM trend (phase C, per the four-clause ruling — unchanged); a PCH
  lands via a separate line of work (`m2.5`) and will be merged into `m3`
  when ready (build-time measurements re-baselined after).
- Review carries (recorded, deliberately not acted on in phase A):
  (a) include/hven/detail/sqp/lapacke_shim.h duplicates
  detail/linear/lapacke_shim.h at global scope on the Apple path — known
  duplicate primitive, collapses at phase B/C (phase B dissolves the SQP
  copy per the plan's §2 map); (b) the corpus provenance git-describe is
  configure-time and stamps hven's own tree — under add_subdirectory
  consumption it would walk to the consumer's .git; low priority,
  bench-side; (c) the SNOPT detection block runs on library-only
  configures (cosmetic; placement is otherwise load-bearing);
  (d) docs/testing.md has no entry for the migrated suite yet —
  documentation debt, natural home once phase C settles TU structure.
- The psiopt-old rig arm's pin (HVEN_RIG_PSIOPT_SEAM_COMMIT = pre-M2
  tycho 060ad213) is now unsatisfiable from a tycho checkout at M2+ HEAD
  without a pinned worktree — surfaced while running the two-seam
  configuration; belongs to the tycho instance's rig-derivation lane, and
  gate B's three-seam Mac/Linux legs need a ruling on how that arm is
  checked out (pinned worktree vs. re-derivation).

## Review outcome (2026-08-14)

The execution review (2026-08-14; the reviewer-side note
`docs/notes/2026-08-14-m3-gate-a-and-phase-b-review.md`) returned
**Gate A: PASS**, with the rulings below. Every ruling rests on evidence
the reviewer re-derived independently rather than accepting on assertion.

- **Census flip adjudication: ACCEPTED**, after independent
  re-derivation — the reviewer recomputed the ratio distribution from the
  committed CSVs (min 0.528, an exact match), re-ran a 7-column counter
  cross-check on all mutually-`Optimal` cells with **zero** mismatches
  (independently confirming the 56/57 byte-identity), and confirmed
  grounds 1, 3 and 4 by direct read. Ruling: a wall-clock boundary flip
  in the favorable direction, on a cell whose baseline asserts nothing
  numerical; gate A's byte-identity requirement is satisfied on every
  cell where the baseline asserts numerical content.
- **Baseline amendment: DEFERRED to gate B, structured.**
  `bench/baselines/2026-08-06-corpus/walk_baseline.csv` stays unchanged
  through gate B — the flip is now pre-adjudicated and expected to
  recur — and the amendment folds into the **declared commit that closes
  gate B**, citing BOTH fresh rows (gate A's and gate B's) as the
  re-derivation evidence. The amended pin therefore arrives with two
  independent reproductions on the migrated engine, not one.
- **Flags unification: ruled.** Engine TUs compile under library flags at
  phase C (forced by the TU split; the per-boundary bit-identity proofs
  are the net that catches any flag-induced movement, and a movement
  there is a declared re-derivation, never silent). The test/bench
  harness TUs **stay sandbox-matched indefinitely** — they are the
  measurement instrument, and changing instrument flags buys nothing
  while risking silent pin drift. Revisit only on a concrete need.
- **Gate-B hygiene item recorded.** Gate-B evidence must be produced from
  a clean configure so the provenance stamp names a real commit: gate A's
  census binary stamped `cf65a03a77eb-dirty`. Gate A's evidence is
  unaffected (the census content is what it is), but the stamp is not to
  repeat.
- **The remaining decisions above — all APPROVED as proposed**: the flat
  `include/hven/detail/sqp/` phase-A home, bench built with the suite
  (load-bearing, not stylistic — the WallDeadline subprocess tests need
  the corpus binary), the Windows exclusion, and macOS watch-only until
  gate B.

The same review returned **APPROVED WITH CHANGES** on the phase-B design
note; its three required changes and its open-item rulings are folded
into [`docs/retarget-design-sqp.md`](../retarget-design-sqp.md).
