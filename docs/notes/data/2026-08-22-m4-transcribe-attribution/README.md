# M4 transcription-path attribution — four-arm quiet session (2026-08-22)

Purpose: attribute the tycho-shaped Phase_Transcribe / Phase_Construct / whole-solve
movement observed when tycho consumed the hven M4 head (d71b945) to the hven commit
that introduced it, on a quiet box, with identity evidence.

## Arms (hven commit → tycho build)
- 72baafd — pre-M4 head (tycho cec58e8a)                      [base]
- f07184b — M4 Phase 1 (tycho attr-mid scaffold = cec58e8a + include retargets)
- d3d4ec2 — M4 Task 4/5 head (tycho attr-mid)
- 3f04c0b — M4 Task 6 close (tycho 22a91465)
Full build provenance: provenance-arms.txt. Each arm is a separate bench_all binary built
from the named hven commit with an otherwise identical tycho tree and toolchain.

## Protocol
attr-session.sh: 5 reps, arms alternated per rep (rep r starts at arm r-1 mod 4), one
bench_all run per arm per rep (google-benchmark, 1 repetition each, medians of the 5 reps
reported), box idle at start (load 0.43), lock /tmp/box-build.lock held for the session.
Identity: identity_probe.cpp built per arm; Brach_32seg and PolarLT_128seg at
partitions=1 and default, MKL_NUM_THREADS=1 — objective (17 digits), iteration count,
flag, status.

## Result (median of 5 reps, % vs 72baafd, rep spread)
See session-table.md. Summary:
- Phase_Transcribe_16seg +62.0 / +59.9 / +65.1 % ; _64seg +43.1 / +42.9 / +43.4 %
- Phase_Construct_16seg +4.6 / +4.2 / +4.5 % ; _64seg +12.1 / +11.1 / +12.0 %
- InteriorPointSolver PolarLT_128/256 −0.7 … −2.4 % ; MTMetis cells within ±1 %
- Brach_16seg +2.8 / +3.1 / +1.5 % (spread 6–7 %) ; Brach_32seg +2.1 / +3.3 / +5.9 % (spread 4–9 %)
Attribution: the entire transcribe/construct movement is introduced by 72baafd→f07184b
(Phase 1: per-lay claim-stream digest, eager declaration deep copy, bound digest); the
later legs are flat within spread. Whole-solve is at parity. Identity: objective,
iteration count and flag identical across all four arms at both partition counts
(session/session-probes.txt).

## Status of the regression
Corrected by the layout-cost fix (branch m4-fix-layout-cost): lazy-on-first-read,
epoch-invalidated declaration and digests; bulk digest feed at identical hash values.
Acceptance at the fix's rebased head (hven 6d7433f chain, tycho 22a91465; acceptance-fixhead/,
7 alternated reps vs bench_all-72baafd, box load 0.48): Construct_16seg +0.24 %, Construct_64seg
+0.38 %, Transcribe_16seg +0.99 %, Transcribe_64seg +0.74 % — all inside ±1 %. Whole-solve
−7…−27 % is the feed_index core-hash optimisation (credited separately, not to the M4
contract-cost total). Identity: objective (17 digits), iteration count, flag, status equal
between arms at both partition counts.

## WATCH rule — Brach_32seg
The +2.1→+3.3→+5.9 % drift across the M4 legs is inside its 4–9 % rep spread: no finding.
Because the fix's feed_index optimisation (a shared-core change, credited separately from
the M4 contract-cost total) will mask any Brach_32seg accretion from here on, the WATCH
base is the fix's release commit (the m4 head carrying 6d7433f), and every later M4 task's leg reports Brach_32seg
against that base explicitly (not pooled). Drift out of band against that base →
investigate.

## Defects of record
- The hven IPM wall leg (bench/ipm_time.cpp) printed the convergence flag in its `iters`
  column until the fix's round 1 (68e66f3); earlier IPM-leg records carry no real
  iteration counts. This session's identity comes from the probe, not that leg.
- attr-results/bench-72baafd.json was overwritten at 20:02 by a re-running ladder leg; it
  is consistent with baselines/baseline-72baafd-quiet3.json (IPM medians ratio
  0.995–1.019), which is the authoritative 72baafd record.
- The first session queue (queue-session.sh) never launched; the session was started by
  hand at 22:34. The probe grep in attr-session.sh did not strip ANSI; probes were re-run
  by hand immediately after the session (same binaries, idle box) — session-probes.txt.
