# O12 Mac session — run-sheet DRAFT (settler-drafted, 2026-08-22)

The paper-only piece of the M3 O12 carry: everything Grant's
one-hardware-session needs, compiled from the rulings so the session is
executable without re-derivation. DRAFT — the flagged items get
verified when the session is actually booked; this is not itself a
booking. Sources: phase-C plan §10 O12 row + §12 Q5
(docs/notes/2026-08-15-m3-phase-c-plan.md), gate-C request §4
(docs/notes/2026-08-21-m3-gate-c-review-request.md), the rig-pin ruling
(carry doc §6).

## What the session serves (both in one session, ruled)

1. **H3's Accelerate arm** — the per-backend no-op proof of the hash
   re-key (§12 Q5): Accelerate-vs-Accelerate identity across the
   re-key, captured as two passes.
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
