> Copied verbatim from `.superpowers/` (the round's own report/review) on 2026-10-01; nothing below this preamble is edited.

# M7 W2 P1 review (Fable) -- the incremental Schur C, 1f46e34d on baddb728

Scope: `git diff baddb728 1f46e34d` (schur_complement.h, qp_engine.cpp, test_schur.cpp), read from committed
objects only; the evidence in `.claude/worktrees/agent-a9f44008cb25541ea/.scratch/p1/`. Nothing built or run
except read-only comparisons of existing artifacts (rowcmp.py, u0cmp.sh's `cmp`, cellcmp.py). Tree of the
worktree commit 904350e2 == tree of 1f46e34d (dbc42f71), so the evidence is the reviewed code's.

## (1) Bit-identity -- HOLDS, by inspection
* `append_border` (schur_complement.h): `c_(k,j) = -v_[k].dot(k0inv_v_[j])`, `c_(j,k) = -v_[j].dot(k0inv_v_[k])`
  for every j < k, both computed (not mirrored); `c_(k,k) = -v_[k].dot(k0inv_v_[k]); c_(k,k) += d_[k]`. Operand
  order and the two-step diagonal are the old `rebuild_schur`'s exactly; the old loop evaluated the same (i,j)
  pairs from the same stored vectors.
* `drop_border`: `delete_c_row_col` is double copies only; the surviving block is entry-for-entry what a
  from-scratch assembly of the surviving arrays gives. `restore_c_row_col` checked by hand: column shift-back,
  row shift-back (reverse order), then the saved full-length column and row (diagonal included) written back.
* Every reader consumes the top-left dim() block after a completed `factor_c()`; a failed add leaves the
  half-written row/column beyond dim(); a failed drop restores; `replay_batch` re-appends by `append_border`.
* Factorization input: `ConstMatRef` = `const Eigen::Ref<const Mat>&` (core/types.h:78) binds
  `c_.topLeftCorner(m,m)` without a copy; `factorize_core` does `factors_ = A` (contiguous, lda = n) and
  `dsytrf('L')` -- same bytes, same triangle, same storage as the old contiguous `MatrixXd c`.

## (2) The batch path -- HOLDS
* Readers and their sites (qp_engine.cpp 1f46e34d): `border_solve_or_fall_back` (3129/3134/3157) and
  `border_inertia_verdict` (3227/3235) run after `sync_borders` returns (3088/3114/3156);
  `refine_face_for_verdict` (2726/2789), `dep_factor_view` (3959), `solve` through `solve_bordered_eqp` and
  `ws_independence.h`'s `DepFactorView` all run after. `sync_borders`' own drop loop runs BEFORE `begin_batch`.
  `add_border`'s `solve_vec(kkt_, v)` reads K0 only (kkt_calls.cpp:50), C-independent. `predictor.h` owns its
  own `SchurComplement`, never batched. No reader between adds anywhere.
* Open batch: nothing between `begin_batch()` and the `try` can throw; every exit calls `close_batch`;
  `end_batch` clears `batching_` before `factor_c`, so a throw cannot leave it open; nested `begin_batch` and a
  `drop_border` inside a batch throw `logic_error`; `sync_borders` is the only caller.
* Throw: the replay raises the same `DenseSymmetricFactor::try_factorize` runtime_error the per-add path
  raises at the same add; the stack is that add's rollback (prefix); `factor_`'s final internal state is the same
  sequence (prefix ok, then the failing attempt); the ledger is trimmed to `dim()` and `schur_updates` by the
  same amount -- equal to the per-add count. All-or-nothing holds on add, drop, and batch.
* Failed-drop rollback: reachable only via bad_alloc inside the dense factor's copy (MKL throws on a non-finite
  C at add time so none persists; Apple demotes to singular, no throw). Written correctly; untested, as declared.
* Two residual differences, neither reaching an asserted counter: (i) on a throw, adds after the failing one
  have already spent their K0 solve (MKL `counters_.solve_count`, not read by any QpCounters column) and one
  extra dense attempt is made; (ii) `replay_batch` allocates its tail vectors inside the catch -- a bad_alloc
  there escapes with the full batch on the stack, evidence absent; ledger agreement is still preserved because
  the trim reads `dim()`.

## (3) Exception safety / allocation -- HOLDS
`reserve_all` grows the three arrays and `c_` before any mutation; `drop_border`'s two saved copies are taken
first; the rollbacks are allocation-free. The old reserve-first rule is kept and extended to `c_`.

## (4) Evidence -- SUPPORTED by the raw transcripts
* Corpus: `rowcmp.py F9 A1` 0 of 51 rows changed, border and refactorize; `F9off A1off` 0 of 51 (re-run,
  read-only). The corpus .txt files are byte-identical once the worktree name in the path column is stripped.
* U0: `u0cmp.sh` re-run (cmp only): COMPARISON_BYTE_IDENTICAL against the fix-9 gated replay's comparison.txt;
  the only difference in the runner's own output is the "NC2" header label.
* Suites: Release 2894/2894 (2 Disabled, 1 Skipped); Debug 1448 + 1445 = 2893, 0 failed; the three new tests
  present in both configurations; long test PASS 164.36 s alone. Key 43/43; fixtures 38 passed / 3 skipped by
  name. clang-format build legs OK.
* TRAIN: `cellcmp.py` re-run: 13 cells x 132 columns, 0 differ, against the RE-TAKE 10 CSV rows.
* Tests: `IncrementalCMatchesFromScratchAfterAddsAndDrops` compares `cond_estimate` bits and `neg_eigs` against
  an INDEPENDENT from-scratch assembly plus its own DenseSymmetricFactor (the old rebuild's code); its SOLVE
  comparison is against a fresh stack built by the same new `append_border` path, so it proves drop == add-only,
  not new == old. `BatchedAdds...` and `BatchThatThrows...` (MKL only; Apple UNOBSERVED, correctly not claimed)
  pin the batch contract. Meaningful; one strengthening rider below.

## (5) Part (b) not implemented -- reasoning CORRECT from the code
`solve_eqp` factorizes the bound-eliminated K of dimension `n_free + me + n_working` (qp_engine.cpp:2837-2838);
`repair_temporary_vertex` pins one variable per probe (`pin_at_best_bound` then `probe_inertia`), so `n_free`
drops by one per probe; `analysis_decision` re-analyses on any pattern-hash change (kkt_calls.cpp:12-21) --
one analysis per probe, matching C10's 2506/2508. Reuse across a changing dimension is not expressible through
`hven::linear`'s analyze/factorize surface; the routes are a user permutation (`iparm`, IPARM-SURFACE, out of
scope) or a fixed superset pattern (different numerics). Correctly left alone.

## (6) Hygiene -- CLEAN
Terse INVARIANT and BATCHING paragraphs in the header; no settlement prose; no `.superpowers` pointers in the
diff; `grep -ci co-authored` = 0. The unchanged "the last rebuild threw" message is pinned by a test, as noted.

## Verdict: CLOSED, with non-blocking riders
R1 `tests/sqp/test_schur.cpp` `IncrementalCMatchesFromScratchAfterAddsAndDrops`: also compare a solve through
   the reference factor (`f.solve` on `rhs1 - Vt w`) so the solve-bits check is independent of `append_border`.
R2 `schur_complement.h` `replay_batch`: it can be allocation-free -- C already holds the identical rows, so
   factor the top-left prefixes in order and shrink to the first that throws, instead of re-appending.
R3 Record (ii) of section 2 in the header's throw contract if the batch path is ever extended to drops.

Ledger note: P1 (1f46e34d) reviewed CLOSED -- C and its Bunch-Kaufman input bit-identical to the old rebuild
by inspection; batch path reader-free and exception-closed; corpus 0/51 x2 algebras x E2 on/off, U0
byte-identical, suites 2894/2893, 13 TRAIN cells 0/132 re-verified from raw artifacts; (b) correctly deferred;
riders R1-R3 non-blocking.

SIGNOFF: M7-W2-P1-REVIEW-FABLE
