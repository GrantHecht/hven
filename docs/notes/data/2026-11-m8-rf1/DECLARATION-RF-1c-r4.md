# DECLARATION-RF-1c-r4 -- the M3 leg (successor to DECLARATION-RF-1c-r3; that file kept unedited)

Committed before the r4 leg is run; never edited afterwards.

The r3 leg (`gate-RF-1c/mutation-r3/leg.out`, `VERDICT MUTATIONS_AS_DECLARED`; control clean, M1, M2, R1 killed by their declared tests) killed M3
(the fall test dropped) only through other tests (`ARestartNeedsTheFloorsPassesOfItsOwn` and three `Rf1RestorationStall` cases), NOT through its
declared killer `Rf1RestorationSeam.AFallingPhaseSurvivesItsWindowByTheFallTest`: the fixture's phase (trust radius 1.0) ended on its sixth pass, so
the pass floor was never past and the verdict never reached the fall test. The r3 verdict is kept as recorded; the declared killer was not honest.

Change (this commit's parent, test only): the fixture starts at trust radius 0.25 (doubling), so the phase takes eight passes with h falling
20 -> 0, outliving the floor by two passes. (Radius 0.02 gives eleven passes that fall under 10% per five and are rightly cut; radius 1.0 six.)

Leg: `mutations_rf1c_r3.sh` unchanged, ids `M3` only (control runs first): `~/.local/bin/box-lane -- bash legs-rf1c/mutations_rf1c_r3.sh gate-RF-1c/mutation-r4
<worktree> <build-rf1/rel> M3`. Bar: control clean; M3 killed **by `AFallingPhaseSurvivesItsWindowByTheFallTest`** (other killers allowed as well).
M1, M2 and R1 are not re-run: their pinning tests are unchanged and the changed fixture does not touch them.
