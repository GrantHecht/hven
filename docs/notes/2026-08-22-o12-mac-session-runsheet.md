# O12 Mac session — run sheet (settler-drafted 2026-08-22; booking items RESOLVED 2026-08-22)

The paper-only piece of the M3 O12 carry: everything Grant's
one-hardware-session needs, compiled from the rulings so the session is
executable without re-derivation. The four booking items are RESOLVED
in the final section; the executor works from this sheet as written
and routes anything it names as a judgment call back to the lanes. Sources: phase-C plan §10 O12 row + §12 Q5
(docs/notes/2026-08-15-m3-phase-c-plan.md), gate-C request §4
(docs/notes/2026-08-21-m3-gate-c-review-request.md), the rig-pin ruling
(carry doc §6).

## What the session serves (both in one session, ruled)

1. **H3's Accelerate arm** — the per-backend no-op proof of the hash
   re-key (§12 Q5): Accelerate-vs-Accelerate identity across the
   re-key, captured as two passes. The claim, in H3's own words
   (fb45a00): "HASH VALUES CHANGE; EVERY REUSE DECISION IS PRESERVED" —
   the asserted evidence is reuse-DECISION counters and deterministic
   census columns, never the hash values themselves.
2. **Gate C item 3's rig Mac leg** — the golden rig's Mac legs, run
   under the rig-pin ruling (tree-hash identity, scope
   `psiopt/include/tycho/detail/solvers/linear/`, pin `bfb7d30c5cc8…`).

## Prerequisites — status

- Tycho-side rig-pin ruling closed (2026-08-19): **MET**. This was the
  only booking gate.
- Both H3 arms capturable by checkout: pre-re-key = `bed76b2`'s parent
  regime (capture AT the parent of the re-key; gate C names the pair
  as pre `bed76b2`, post `fb45a00`) — **VERIFY at booking which exact
  commit each pass checks out** (the gate-C sentence names the pair;
  the phase-C O12 row says "H3's parent commit" and "H3 HEAD").
- REACHABILITY (verified 2026-08-22): both commits are ancestors of
  the `m3-branch-final` tag and NOT of main (squash merge) — Grant's
  Mac clone must `git fetch origin tag m3-branch-final` before
  checkout.

## Structure — two passes, one session (ruled shape)

**Pass 1 (pre-re-key capture):** checkout the pre arm; build with
Accelerate (macOS default backend — no MKL on the box); run the
capture set; record everything as OBSERVED values with the standard
provenance stamp (toolchain, hardware, date, commit).

**Pass 2 (post-re-key comparison):** checkout `fb45a00`; same build
regime; same capture set; compare against pass 1. The claim being
proven is per-backend no-op: Accelerate pass 1 vs pass 2 identical on
the asserted columns. Any difference is a finding, not a footnote.

**Rig leg (either pass or both — VERIFY at booking):** the golden rig's
Mac legs against the pinned external checkouts. Needs the two OLD-SEAM
checkouts present on the Mac at their pinned paths/tags
(tests/golden_rig/CMakeLists.txt is the authority for pins and paths).
The escape-hatch provenance fix (R1, `2508b6a`) means the rig reports
the tree it OBSERVED — confirm the observed hash equals the pin in the
session record.

## Capture set — VERIFY at booking (do not improvise on the day)

- The exact artifact list each pass records (suite results, the
  pinned-value captures, any census/replay columns the §12 Q5 reading
  asserts) — pull from H3's proof obligations in the phase-C plan
  before booking; the session record must name its columns and
  protocol the way §7 requires.
- Machine conditions: one process, machine otherwise idle; wall-clock
  informational only; Accelerate values recorded as observed —
  **never fabricated; anything not captured stays UNOBSERVED**.
- Interim evidence already on record (macOS CI runs 32378993026 /
  32384330753, both green) is cited as interim in the session note,
  superseded by the direct observations.

## Outputs

- One committed session note under docs/notes/ (dated), carrying both
  passes' provenance stamps, the comparison table, the rig-leg record
  with observed-vs-pinned tree hash, and the disposition of every
  previously-UNOBSERVED value it captured.
- Register updates: any Accelerate divergence found goes to the
  Accelerate divergence register under its protocol; M3-6's re-open
  trigger is M5's predictor measurement, NOT this session — do not
  conflate.

## Open items to resolve BEFORE booking (the flags above, gathered)

1. Exact checkout for pass 1 (parent-of-`bed76b2` vs `bed76b2`).
2. The definitive capture-set list from H3's proof text.
3. Whether the rig leg runs once or per-pass.
4. Whether any M4-era addition should ride the same session (owner's
   call; M4 acceptance §7 keeps Mac values UNOBSERVED, so nothing M4
   REQUIRES it — but a second session is expensive to book).

## RESOLVED at booking (settler, 2026-08-22; facts verified in-tree)

1. **Pass-1 checkout = `bed76b2` exactly.** H3 is `fb45a00`; `bed76b2`
   is its sole parent (`bed76b2..fb45a00` is one commit), so "H3's
   parent commit" (phase-C O12 row) and the gate-C pair's "pre
   `bed76b2`" name the same commit. Pass 2 = `fb45a00`.
2. **Capture set, per pass (from H3's proof text):**
   (a) the full hven suite, Release AND Debug, as ctest result tables
       with the count chain (registered / executed / passed / skipped /
       disabled) and the verbatim log files committed beside them;
   (b) the corpus walk, all 57 cells, deterministic columns only
       (counters, statuses, residuals) — one process, single-threaded,
       machine otherwise idle; provenance header per §7 (toolchain,
       hardware, date, commit, thread setting); wall-clock recorded as
       informational only;
   (c) the SQP reuse-decision counters the suite surfaces (SSN rebuild
       counts, QP structural-rebuild counts) — these ARE the per-backend
       no-op claim.
   The session note's comparison table is pass-1 vs pass-2 on (a) counts
   and dispositions, (b) every deterministic column byte-for-byte, and
   (c) every counter exactly. Any difference is a FINDING: recorded,
   never adjudicated by the executor, never re-run to make it pass.
3. **Rig Mac leg runs ONCE, at a THIRD checkout: the `m3-branch-final`
   tag** (the tree Gate C declared), independent of the H3 pair; the
   rig's reported OBSERVED tree hash is recorded beside the pin (R1,
   `2508b6a`). `HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM` stays OFF; an absent
   or mismatched OLD-SEAM checkout is routed back, not worked around.
   If gate-C request §4 binds the leg to a different tree, §4 governs —
   the prompt drafter checks this once before dispatch.
4. **No M4-era additions ride the session** (owner's call, pending at
   the time of this amendment; M4 acceptance §7 keeps Mac values
   UNOBSERVED). Candidate noted for the owner: the M4-head
   zero-allocation pin's count half is glibc-only and self-skips on
   macOS — it is UNOBSERVED there by construction and would need a
   fourth checkout (M4 head) to even run its skip; the settler's
   recommendation is NOT to ride.

**Executor terms (evidence-only):** execute as written; a dedicated
branch on the Mac clone, which must `git fetch origin tag
m3-branch-final` first (`bed76b2`/`fb45a00` are not on main); commit
artifacts only; NO source edits; NO push (the tree-token holder pushes
after the artifacts are checked against the shapes above). Route back
without improvising: a build failure at any of the three checkouts on
the current Xcode/Accelerate; a rig pin mismatch or absent OLD-SEAM
checkout; any pass-1 vs pass-2 difference; any value that cannot be
captured (recorded `UNOBSERVED`, never filled). Governance binds the
executor verbatim: never fabricate Accelerate values; the SNOPT source
firewall; `notices/` untouched.
