# Chunk 2 fix round + chunk-1 fix residuals (comment accuracy review)

Branch `sweep/base`. Pull first. Touches only `include/hven/detail/interior/` (section A) and
`include/hven/detail/globalization/` (section B). If chunk 3 is already on the branch, leave it.

Read `.sweep/chunk-2-review.md`: Section A = chunk 2 (3 blocking, 10 should-fix, 6 nits),
Section B = residuals of the chunk-1 fix (2 blocking, 1 should-fix, 4 nits). Apply every
blocking and should-fix finding using the review's "correct text", verified against the code;
apply nits that are one line, list the rest as not applied with a reason.

Blocking, specifically:
- A-1 `barrier_math.h:90-115` — delete the leftover pre-image block (107-115); one docstring only.
- A-2 `iterate_info.h:71-74` — the caller (`SocRecovery::on_step_rejected`) selects the norm;
  `drives_classic_path()` only compares. Name the right actor or drop the sentence.
- A-3 `constraint_function.h:71-73`, `objective_function.h:62-65` — remove the invented
  "per-row locks" clause; the locks are per clash COLUMN and taken only when
  `!unique_constraints_ && VarClashes[active] != -1`. Say exactly that or nothing.
- B-1 `inertia_regularization.h:11-41` — demote the WHOLE block (every continuation line) to
  `//`; no `///` line may remain that would attach to `kProxRegFloor`.
- B-2 `modern_merit.h` — move `@throws std::logic_error` from the constructor (cannot throw) to
  `is_iterate_acceptable` (:666), where the throw is.

Two rules, restated because both recurred:
1. When you REPLACE a block, delete the old one entirely — diff your own file against the
   parent before committing and look for duplicated sentences.
2. Every sentence naming WHO does something (a function, a caller, a lock, a thread) must be
   checked against the code at that line. If you cannot verify the actor, drop the sentence.

Zero code-token change. One commit, subject `docs: comment sweep — interior and globalization
accuracy fixes`. Add a "Fix round" table to `.sweep/chunk-2-report.md` (finding → applied / not
applied + reason) and extend the chunk-1 report's table for B-1/B-2. Push and report the hash.
