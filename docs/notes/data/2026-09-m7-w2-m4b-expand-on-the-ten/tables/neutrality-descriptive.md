# M7 W2 M4b — `tables/neutrality-descriptive.md`

`RULE.md` §5.2's DESCRIPTIVE neutrality report, written by
`wrappers/table_neutrality_m4b.py` from M4's already-committed rows
`../2026-09-m7-w2-m4-anticycling-arm/rows/{off,E}-<CELL>.txt`.
**No leg was re-run for it. It carries no verdict.** §5.2 says so in
terms: "These three cells receive no verdict. The report states what
happened on them and moves on. They are context for reading the ten,
not a gate in front of them."

**Why it exists.** M4 §5.5 required an arm to reproduce its own `off`
leg BIT FOR BIT on these three cells. Arm E failed that on all three,
and M4's close-out established from the traces WHY: E does not only
decide ties — it admits constraints against a slowly growing
feasibility bound and takes a largest-pivot second pass, so it changes
which constraint enters even where nothing is tied. A bit-equality bar
is unmeetable by an arm of that shape for reasons that have nothing to
do with whether the arm is any good, so `RULE.md` §5 REPLACED it with
this report — which asserts nothing about equality, cannot be passed by
accident, and must be carried whatever it says.

**The subject is arm E as implemented in the pinned M4 binary**, never
"EXPAND" and never "canonical EXPAND".

## §5.2's four items, per cell

| | `READING6` | `SMBANK` | `ZAMB2-8` |
|---|---|---|---|
| **1. terminal status** — off | `optimal` | `optimal` | `optimal` |
| **1. terminal status** — E | `optimal` | `optimal` | `optimal` |
| **2. `f`** — off | `-144.65970057279054` | `-7129292` | `-0.15293592733713887` |
| **2. `f`** — E | `-144.65970057279054` | `-7129292` | `-0.1529359281433445` |
| **2. `f` vs M3c consensus** | **ABSENT** — `READING6` appears nowhere in M3c's committed `census.md` | **ABSENT** — `SMBANK` appears nowhere in M3c's committed `census.md` | **ABSENT** — `ZAMB2-8` appears nowhere in M3c's committed `census.md` |
| **2. `\|f_E − f_off\| / max(1,\|f_off\|)`** | `0.000e+00` | `0.000e+00` | `8.062e-10` |
| **3. `max(feasibility_e, feasibility_i)`** — off | `2.797762e-13` | `9.458745e-11` | `1.795006e-07` |
| **3. `max(feasibility_e, feasibility_i)`** — E | `2.797762e-13` | `9.458745e-11` | `4.570727e-08` |
| **3. that leg's own printed `opt_feas_tol`** | **ABSENT** — this probe does not print it | **ABSENT** — this probe does not print it | **ABSENT** — this probe does not print it |
| **4. minors off** | `3833` | `334` | `6666` |
| **4. minors E** | `3119` | `328` | `9947` |
| **4. minors ratio E / off** | **0.8137** | **0.9820** | **1.4922** |

### Item 2 — the consensus is ABSENT for all three, and that is not a gap

§5.2 requires `f` under E to be read against **M3c's three-lane
consensus, machine-read from M3c's committed `census.md`, never
recomputed**. M3c's census is a census of the cells that FAIL at DEFAULT
options. These three do not fail — that is why M4 chose them as
neutrality cells — so the census has no row for any of them. A grep of
the committed file for the three names returns **0 lines**. No consensus
is read, none is computed, and the item is reported ABSENT rather than
filled from somewhere else.

### Item 3 — `opt_feas_tol` is ABSENT from this probe's row

§5.2 requires the leg's OWN printed `opt_feas_tol`. This probe does not
print the `OPTIONS` block at all: `opt_feas_tol` is one of the 127
fields M4's own OFF-control gate lists under "fields the leg does not
print (ABSENT, never agreement)"
(`../2026-09-m7-w2-m4-anticycling-arm/logs/gate-off.txt`). The
feasibility values are printed above without a bar beside them, and no
bar is substituted from M3c's row or typed in from the rule.

## §6.6's arm counters on these three, and what they say

| counter | `READING6` | `SMBANK` | `ZAMB2-8` |
|---|---|---|---|
| `arm_min_steps` | `799` | `1` | `4187` |
| `arm_unpinned_adds` | `799` | `1` | `4187` |
| `arm_pivot_switches` | `135` | `3` | `306` |
| `arm_lex_drop_switches` | `0` | `0` | `0` |
| `arm_lex_ratio_switches` | `0` | `0` | `0` |
| `arm_resets` | `0` | `0` | `0` |
| `arm_reset_nontrivial_moves` | `0` | `0` | `0` |

* `arm_min_steps == arm_unpinned_adds` on all three, so the ride's
  re-pin defect (README, departure 2) was **not exercised** on any of
  them — which is exactly what the Codex review of M4 reported, and is
  why M4's data could not see it. Five of this task's ten cells do
  exercise it; `tables/arm.md` §3 counts them.
* `arm_resets` is 0 on all three, so no reset ran on them either, and
  M4's data could not see the reset off-by-one (README, departure 1).
  Two of this task's ten cells reached a reset.
* `arm_lex_drop_switches` and `arm_lex_ratio_switches` are 0, as they
  must be: E touches no lexicographic rule.

## §5.3's DECLARED CAUTION, held against what the ten actually did

§5.3 was written BEFORE the ten ran and said, from these three cells:
**"E can lengthen a walk as readily as shorten one."** The ratios it
declared were `READING6` 0.8137, `SMBANK` 0.9820, `ZAMB2-8` 1.4922.

**The caution HELD.** On the ten, the minors ratio E/off runs from
0.7630 (`BRITGAS`) to 1.3387 (`YORKNET`) — E shortened seven walks,
lengthened two, and left one (`ACOPP57`) at exactly 1.0000. Neither a
short leg nor a long one was a surprise, and neither is read as the
arm's general property. See `tables/arm.md` §1 for the ten ratios.

One qualification the caution did not anticipate and this task must
add: `SAWPATH`'s 0.7737 is **not a saving**. Both its legs fired the
300 s deadline, so the two minor counts are two truncations of two
different walks at the same wall-clock stop, not two completed walks.
`verdict.txt` records the same point.
