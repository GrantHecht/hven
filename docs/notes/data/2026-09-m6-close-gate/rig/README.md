# M6 CLOSE GATE — the golden rig, three seams: the pins, the expected failure, the controls

## The seam pins, verified READ-ONLY before the leg (and again by the rig's own configure)
```
tycho      HEAD            48038a2f59243c2bbdc4017c6f70e5f3b56c3fea
           consumed tree   bfb7d30c5cc821069b7af3f9efa077f29474de5e   == the pin (CMakeLists.txt:118)
           scoped status   EMPTY (psiopt/include/tycho/detail/solvers/linear)
tycho_sqp  phase-7-close   4faa1df116da53c9dc68f36635c118f52d39d2b9   == the pin (CMakeLists.txt:53)
           tag object      d3bc25315b19237f8d666be82b8768a0e8693e25   (as recorded, informational)
           checkout HEAD   91c4ec1c5049efa5eb2489425b489212de40fe71   (a later commit; explicitly allowed, CMakeLists.txt:405-408)
           git diff --quiet phase-7-close -- include/tycho_sqp   rc=0
HVEN_RIG_ALLOW_UNPINNED_PSIOPT_SEAM  NOT set (stays OFF)
```

## The rig's own configure-time verdicts (verbatim)
```
-- golden rig: interior-point old-seam arm pinned to consumed tree bfb7d30c5cc821069b7af3f9efa077f29474de5e (psiopt/include/tycho/detail/solvers/linear), matched at HEAD 48038a2f59243c2bbdc4017c6f70e5f3b56c3fea, consumed linear/ tree clean
-- golden rig: interior-point old-seam arm enabled (/home/ghecht/Projects/tycho)
-- golden rig: SQP old-seam arm pinned to phase-7-close (4faa1df116da53c9dc68f36635c118f52d39d2b9), tree state verified
```

## The expected failure — reproduced EXACTLY, and the test name did NOT move
```
The following tests FAILED:
	497 - Arms/InteriorPointTrace.P5_InertiaBeforeFactorizationIsAnExplicitState/sqp-old@mkl (Failed)
Errors while running CTest
```
docs/testing.md:1391-1399 names this entry verbatim; T8.10's rename did not touch it
(tests/golden_rig/traces_interior_point.cpp:262 still spells
P5_InertiaBeforeFactorizationIsAnExplicitState, and seam_registry.cpp:63/:245 still
spell the arm sqp-old).

## The controls — all three green (three is ALL of them on Linux)
```
 1/84 Test #422: FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas ............................   Passed    0.00 sec
 2/84 Test #420: FailByDesignControl.SqpSeamStillZeroFillsItsPreFactorizationInertia ...............................   Passed    0.01 sec
 3/84 Test #421: FailByDesignControl.SqpSeamDeclaresTheInertiaSurfaceTheFailingTraceNeeds ..........................   Passed    0.01 sec
```
The fourth control, FailByDesignControl.PsioptSeamStillZeroFillsItsPreFactorizationInertiaOnApple,
is compiled out on Linux (#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM) && defined(__APPLE__)) and appears
in NO inventory here — 0 matches in the full 2628-test listing.  Apple: UNOBSERVED.

## Both counts, as the brief asks
```
rig-scoped (-R 'Arms/|GoldenRig|FailByDesignControl'):
99% tests passed, 1 tests failed out of 84
  inventory: Total Tests: 84
  arms x cases:
         16 /native@mkl
         16 /native-psiopt-parity@mkl
         16 /native-sqp-parity@mkl
         16 /psiopt-old@mkl
         16 /sqp-old@mkl
    + 3 FailByDesignControl + 1 GoldenRigAudit = 84

whole build-3seam inventory:
99% tests passed, 1 tests failed out of 2626
  inventory: Total Tests: 2628
```

## The docs' 155 is STALE at source — see the report's errata
docs/testing.md:1391-1393 and :1458-1459 give the Linux three-seam run as
"1 tests failed out of 155".  The rig-scoped total at this head is 84:
five arms x 16 Arms/* cases = 80, plus 3 FailByDesignControl and 1 GoldenRigAudit.
The SUBSTANCE of the checklist reproduces exactly — one failure, that exact entry,
three controls green, on Linux — only the total does not.

## The skipped cells in the full run (documented, not failures)
```
The following tests did not run:
	436 - Arms/SqpTrace.T2b_PartialSolvePredicateUnderPerturbation/psiopt-old@mkl (Skipped)
	446 - Arms/SqpTrace.T4_HandleOutlivesItsEmitter/psiopt-old@mkl (Skipped)
	447 - Arms/SqpTrace.T4_HandleOutlivesItsEmitter/sqp-old@mkl (Skipped)
	451 - Arms/SqpTrace.T4b_AdoptRefusesStaleNumerics/psiopt-old@mkl (Skipped)
	452 - Arms/SqpTrace.T4b_AdoptRefusesStaleNumerics/sqp-old@mkl (Skipped)
	502 - Arms/InteriorPointTrace.P6_RefinementStepEvidence/sqp-old@mkl (Skipped)
	1628 - EqpRefinementAb.FootprintRuleProbe (Disabled)
	1629 - EqpRefinementAb.FullBattery (Disabled)

The following tests FAILED:
	497 - Arms/InteriorPointTrace.P5_InertiaBeforeFactorizationIsAnExplicitState/sqp-old@mkl (Failed)
Errors while running CTest
```
