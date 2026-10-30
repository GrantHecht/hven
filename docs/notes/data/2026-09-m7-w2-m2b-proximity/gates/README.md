# M7 W2 M2b — the gates at the artifact commit, and what each one's numbers were checked against

Run detached in six lock segments (`../wrappers/gates.sh`) on the committed tree
at **`ef6284f8`**, `dirty-files=0`, 2026-09-20 16:59:34 → 21:57:24 America/New_York.
The chain waited on `/tmp/box-build.lock` from 16:59 to 20:36 while the harness
lane held it; a flock waiter is idle, and wall here is informational either way.

**An exit code is not a result.** Each gate's numbers are checked against a
stated expectation below, not merely against rc=0.

| gate | result | checked against |
|---|---|---|
| G1 Debug suite (long test excluded) | `100% tests passed, 0 tests failed out of 2669` | M2's gate at `9a08a715` ran **2662**; `577719a0` adds exactly **7** `TEST(`s (three `RestorationModelProximity`, one `RestorationModel`, one `SqpDriverRestorationProximity`, two `RestorationProximityOption`) and is the ONLY commit touching `tests/` in `9a08a715..ef6284f8`. 2662 + 7 = **2669**. ✔ |
| G2 the long test ALONE | `Continuation.ProbeBudgetBoundsAFailingProposal ... Passed 688.09 sec`, 1/1 | the one test excluded from G1, run with the suite to itself ✔ |
| G3 Release suite (fresh configure) | `100% tests passed, 0 tests failed out of 2670` | M2's Release gate ran **2663**; 2663 + 7 = **2670**, and it equals G1 + G2 (2669 + 1), so the two configs register the same set ✔ |
| G3 library identity | `fed6243691970e4d3433e0818abaf9bb28cd2f9a1e9c84366fb5dfa84a9d93fc` for BOTH `gates-rel/libhven.a` and the install the 35 legs ran on | **the ladder and the certified suite are the same bytes** — the calibration is not measured on a library the suite never saw ✔ |
| G4 U0 replay, 3 arms | walk / ssn / ipm each `cells compared: 27  columns compared: 75  differences: 0` | the M6 close pin (`docs/notes/data/2026-09-m6-close-gate/replay/`); the expected shape is 27/75/0 ×3 ✔ |
| G5 P-SYM, Release and Debug | exit 1 (**expected**); `DIFFERING OBJECTS: 52` = class A **5** + class B **47**, `UNEXPLAINED: 0`, negative control **23**; classifications **byte-identical between configs** | `before = a494b7f5`. The classifier's header list is exactly `git diff --name-only 577719a0^ 577719a0 -- include/` → `detail/globalization/sqp/restoration.h` and `drivers/sqp_solver_types.h`. Class A's 5 are exactly the five `.cpp`/test sources that commit changed. Exit 1 is expected because one new public field moves `sizeof(SqpOptions)` 400 → 416 in every TU that lays it out; **the gate is the classification** ✔ |
| G6 install smoke | `PASSED` — both solvers converged through one generic consumer, 19 standalone-include TUs compiled and linked | the script's own pass line ✔ |
| G6 export contract | `export contract OK for platform=linux, align=32` | called with the three arguments, the third **parsed from THAT SAME build's configure log** (`ALIGN_FROM_THIS_BUILDS_CONFIGURE_LOG=32`), never a literal ✔ |
| G6 clang-format | rc 0 on the seven code files of `577719a0` | `--dry-run -Werror` ✔ |
| G6 `git diff --check` | rc 0, `DIRTY_FILES=0` | the tree the gates certified is the tree that was committed ✔ |

**No stamp is `-dirty`.** A grep for that token over this directory's
transcripts — every file here but this README, which names it — finds nothing,
and the replay CSVs' provenance header reads `binary: ef6284f84179`.

Two tests did not run in either config, in both cases as they do on every gate
of this branch: `FailByDesignControl.ControlsArePresentForWhicheverOldSeamsThisBuildHas`
(Skipped — no OLD-SEAM adapter in this build) and the two `EqpRefinementAb`
cases (Disabled).

## Files

```
gates.txt                      the chain log: HEAD, segment boundaries, every rc
suite-debug.txt                the Debug suite's tail (the long test excluded)
suite-debug-long.txt           the long test, alone
suite-release.txt              the Release suite's tail
replay/head-{walk,ssn,ipm}.csv the three replay arms at this HEAD
replay/comparison.txt          their comparison against the M6 close pin
psym-compare-{Release,Debug}.txt        P-SYM's raw object-by-object output
psym-classification-{Release,Debug}.txt the classification, which IS the gate
install-smoke.txt              the install smoke's tail
export-contract.txt            the export contract's output
```
