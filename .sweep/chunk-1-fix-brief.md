# Chunk 1 — fix round (comment accuracy review)

Branch `sweep/base`. Pull first. If chunk 2 is already committed on the branch, fine — this
round touches only `include/hven/detail/globalization/` files.

Read `.sweep/chunk-1-review.md` (17 findings: 2 blocking, 7 should-fix, 8 nits). Apply every
blocking and should-fix finding and every nit that is a one-line change; for each, use the
"correct text" the review gives, verified against the code. Specifically:

- #1 `filter_acceptance.h:94-97` — restate the ceiling with the `basval` clamp exactly as the
  code computes it (basval = 1 unless |phi_ref| > 10); do not paraphrase a formula you cannot
  read off the code.
- #2 `monitored_governor.h:70-72` — re-entry is on sufficient progress alone; remove the
  "no tiny step" condition (it was an Ipopt attribution in the pre-image, not a fact about this
  engine). If an Ipopt-comparison callout is worth keeping, say "Ipopt additionally requires …"
  explicitly.
- #3, #6, #7 — Doxygen form: no promised tags that do not exist, one `@brief` per block, no
  floating `///` blocks (attach to the declaration or demote to `//`).
- #8 `monitored_governor.h:141-145` — restore the lost semantic fact on the BarrierDecision
  fields as a one-line callout.
- #9 `sqp/elastic.h:203-206` — make the multiplier-contamination derivation follow again
  (keep it concise; it is a load-bearing callout).
- #4, #5 — fix the report: add the six-divergence block to the rationale candidates; recompute
  the "total lines" column.
- Remaining nits: apply if one line, else list as "not applied" in the report with one reason each.

Rule reminder: a condensed sentence must stay TRUE of the code. When the original attributed
a behaviour to another solver, the condensation must keep the attribution or drop the
sentence — never turn it into a claim about hven.

Zero code-token change, as always. One commit, subject `docs: comment sweep — globalization
accuracy fixes`. Update `.sweep/chunk-1-report.md` in the same commit with a "Fix round" section
(finding → applied / not applied + reason). Push and report the hash.
