# W5 T8.9r fix3 — R2' idle proof, every retained batch of every round

**R2 IS AMENDED (settler, 2026-09-11).** R2's per-process bar — every foreign pid on the box under 0.5 % of the batch wall — was **unmeetable on this desktop and could not be reached by re-running**: the Wayland compositor alone accrues about 3 % of any window, on 16 logical CPUs, whatever the leg does. Fix1 reported that bar NOT met and took its verdict on a narrower test it had not been granted. astra's fix1 review is right that a disclosed substitution is not a satisfied ruling. **R2' replaces it, and every retained batch of every round is re-audited here under R2' — including the rounds that already published a verdict.**

> **R2' — the PINNED-CORE rule.** Foreign TASK time on the measurement core (`cpu2`) **and** on its SMT sibling (`cpu10`), counted as `user + nice + steal + guest` from `/proc/stat` across the batch, must be under **0.5 %** of the batch's wall. Every foreign pid and state seen in any snapshot is listed, transients included. A foreign `R` is a pause and a **re-snapshot**. A batch that cannot be proven is re-run.

**One tool, one version, everywhere.** `scripts/idle_proof.py` is the attrib4 (nice-inclusive) version with fix3's evidence accounting added — `sha256 3394879dbb519155fa985da793a43eedd825bfc1bc83b6636007e91795b33ab6`, byte-identical in all three places it sits (`scripts/`, `attribution-interior/t84/scripts/`, `attribution-interior/t84/redraw/scripts/`); it succeeds fix2's `7c90914d…`, which succeeded the two `ca107629…` copies. The same code computes every table below. The `ca107629…` version took foreign task time from the `user` bucket alone on the argument that no foreign task on this box is niced; the retained logs falsify that (foreign `SN`/`RN` tasks are present), and a niced foreign task lands in `nice`, exactly where the measurement's own time lands. Fix3 adds no arithmetic at all: it reads the second state source, counts pauses once, and reports what the logs do and do not contain. **Nothing was re-measured for this audit, at fix2 or at fix3** — it is arithmetic on snapshots that were already on disk, and every fraction below is byte-for-byte the fix2 figure.

**WHAT `R` MEANS HERE, AND WHAT FIX3 CHANGED ABOUT IT (settler R13).** The fix1/attrib recipe's `box_guard` paused on a foreign `R`, slept, printed `BOX_PAUSE_GIVEUP` and continued **without re-snapshotting** (`scripts/common.sh`). R2' requires the re-snapshot. Fix2 disclosed that and then deferred the requirement to "the next measurement"; **the settler granted no such retrospective exemption, and fix3 withdraws it.** An `R` with no re-snapshot is now an EVIDENCE gap on that batch, reported as **UNPROVEN-EVIDENCE**, and the flag travels to every number the batch feeds. The fractions are untouched: what CPU such a task consumed on `cpu2` or `cpu10` is already inside the counted buckets, so the bar still says what it said. What the bar cannot say is that the box was watched the way R2' asks, and that is now recorded rather than argued.

**The `R` column reads BOTH sources.** The leg scripts write `FOREIGN_TICK` (the single state character from `/proc/<pid>/stat`) and `FOREIGN_PS` (`ps`'s full string). Fix2 read only the first, so `4022442 Rsl` in `L4-leg1-ssn-r2.log` did not appear. Reading both raises the number of batches with an `R` observation to **10 / 10 / 11 / 3 / 5** across the five rounds.

**A SECOND EVIDENCE GAP, WHICH `PROVENANCE.txt` (G2) ALREADY DISCLOSED AND WHICH IS NOW A FLAG.** Only leg 1's nine wall batches place a snapshot between every A/B alternation. Every other recipe — leg-1 perf, leg 2, the interior leg, the per-cell interior leg, attrib2's `diff-`, attrib3's `screen` — calls `box_guard` after its arm loop, so two timed runs follow each other with no foreign state observed across the switch. Those batches are **UNPROVEN-EVIDENCE** too. The `CPUSTAT`/`CPUTIME_SELF` bracket around every timed run is still present in all of them, and that bracket is what the FRACTION is taken on.

## The per-batch table — every retained batch, every round

`core` and `sibling` are R2''s two fractions (`cpu2`, `cpu10`); `worst foreign` is R2-as-written's worst single foreign pid across the whole box, retained because the amendment does not delete the number it was taken on. `asserts` is what the batch's data is read as: **WALL** (R2' governs), **COUNT** (counters only — deterministic at `MKL_NUM_THREADS=1`, CLAUDE.md §7), **SCREEN** (a probe nothing cites), **SUPERSEDED** (retained, quoted nowhere).

**A batch is UNPROVEN in one of two DIFFERENT ways, and the cell says which.**

* **UNPROVEN-FRACTION** — R2''s bar is measured and exceeded on `cpu2` or `cpu10`. The number is there and it is too big.
* **UNPROVEN-EVIDENCE** — the bar is met, or cannot be computed, but the log does not contain what R2' asks for: a **re-snapshot** after a foreign `R`; a snapshot **between two consecutive timed runs**; or, for four of attrib4's batches (`e3`, `pf-r1..r3`), any **timed-run bracket at all** — `PS_SNAPSHOT` blocks but no `CPUSTAT`/`CPUTIME_SELF` pair, so no pinned-core figure exists to test.

A batch can be both. The two columns therefore need not sum to the batch count.

| round | batch | asserts | core `cpu2` | sibling `cpu10` | worst foreign (box) | foreign pids | transients | states seen | `R` seen | pauses | give-ups | evidence | **R2'** |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| fix1 | `leg1-ipm-r1` | WALL | 0.0428 % | 0.4284 % | 2.9418 % | 196 | 28 | `S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| fix1 | `leg1-ipm-r2` | WALL | 0.0428 % | 0.2570 % | 2.8986 % | 198 | 29 | `S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| fix1 | `leg1-ipm-r3` | WALL | 0.0429 % | 0.1717 % | 2.9130 % | 195 | 34 | `R,S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `leg1-ipm-r3/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| fix1 | `leg1-ssn-r1` | WALL | 0.0279 % | 0.3904 % | 2.8335 % | 196 | 22 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| fix1 | `leg1-ssn-r2` | WALL | 0.0559 % | 0.1117 % | 2.7013 % | 196 | 22 | `Rsl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 4022442 | 0 | 0 | `R` at `leg1-ssn-r2/close` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| fix1 | `leg1-ssn-r3` | WALL | 0.0559 % | 0.2236 % | 2.7317 % | 195 | 25 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| fix1 | `leg1-walk-r1` | WALL | 0.0000 % | 0.1718 % | 2.8304 % | 192 | 40 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| fix1 | `leg1-walk-r2` | WALL | 0.0000 % | 0.2057 % | 2.6108 % | 195 | 53 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722106 | 1 | 1 | `R` at `leg1-walk-r2/close` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| fix1 | `leg1-walk-r3` | WALL | 0.0000 % | 0.1307 % | 2.8228 % | 196 | 54 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| fix1 | `leg1perf-ipm` | COUNT | 0.7503 % | 0.2858 % | 2.9084 % | 201 | 32 | `RN,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2522981 | 0 | 0 | `R` at `leg1perf-ipm/passB-r3` with NO re-snapshot; 30 alternation snapshot(s) missing | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| fix1 | `leg1perf-ssn` | COUNT | 0.6160 % | 0.4107 % | 2.7613 % | 201 | 29 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 30 alternation snapshot(s) missing | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| fix1 | `leg1perf-walk` | COUNT | 1.5957 % | 0.2660 % | 2.8846 % | 206 | 16 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 30 alternation snapshot(s) missing | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ipm-off-passA` | WALL | 0.0177 % | 0.0974 % | 2.6817 % | 195 | 61 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ipm-off-passB` | WALL | 0.0221 % | 0.1063 % | 2.7506 % | 195 | 68 | `R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 3819500,50122,50634 | 2 | 2 | `R` at `leg2-ipm-off-passB/r1`,`leg2-ipm-off-passB/r2`,`leg2-ipm-off-passB/r3` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ipm-sink-passA` | WALL | 0.0265 % | 0.2520 % | 2.6565 % | 200 | 71 | `S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ipm-sink-passB` | WALL | 0.0264 % | 0.2152 % | 2.7786 % | 198 | 74 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ssn-off-passA` | WALL | 0.0245 % | 0.2531 % | 2.7639 % | 198 | 74 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ssn-off-passB` | WALL | 0.0217 % | 0.1304 % | 2.8074 % | 197 | 72 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `leg2-ssn-off-passB/r1` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ssn-sink-passA` | WALL | 0.0218 % | 0.1417 % | 2.6622 % | 197 | 72 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-ssn-sink-passB` | WALL | 0.0217 % | 0.2254 % | 2.6614 % | 196 | 70 | `S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-walk-off-passA` | WALL | 0.0363 % | 0.2901 % | 2.6259 % | 196 | 65 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-walk-off-passB` | WALL | 0.0290 % | 0.2829 % | 2.8549 % | 198 | 57 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-walk-sink-passA` | WALL | 0.0289 % | 0.2025 % | 2.7108 % | 195 | 60 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `leg2-walk-sink-passB` | WALL | 0.0362 % | 0.1593 % | 2.6927 % | 198 | 55 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `leg2-walk-sink-passB/close` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `interior-perfA` | COUNT | 0.3745 % | 0.4659 % | 2.9065 % | 196 | 62 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `interior-perfB` | COUNT | 0.0457 % | 0.3019 % | 2.6969 % | 199 | 56 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `interior-wall` | WALL | 0.0552 % | 0.3034 % | 2.7983 % | 199 | 57 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `interior-cells-r1` | COUNT | 0.3080 % | 0.2536 % | 2.8498 % | 198 | 46 | `RNl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 4022478 | 0 | 0 | `R` at `interior-cells-r1/hs071_x1_fixed` with NO re-snapshot; 22 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `interior-cells-r2` | COUNT | 0.3604 % | 0.2703 % | 2.7448 % | 198 | 50 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 1097257 | 1 | 1 | `R` at `interior-cells-r2/f7_n10000_bound_physics` with NO re-snapshot; 22 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| fix1 | `interior-cells-r3` | COUNT | 0.3613 % | 0.2891 % | 2.8940 % | 196 | 59 | `R,Rl+,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 1097257,50122 | 1 | 1 | `R` at `interior-cells-r3/f7_n1000_bound_neutral`,`interior-cells-r3/f7_n1000_bound_physics` with NO re-snapshot; 22 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| attrib2 | `diff-r1` | COUNT | 0.6828 % | 0.2438 % | 3.1996 % | 197 | 58 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `diff-r1/a05-after` with NO re-snapshot; 11 alternation snapshot(s) missing | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| attrib2 | `diff-r2` | COUNT | 0.1594 % | 0.2085 % | 3.1133 % | 197 | 57 | `Rl+,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 1097257 | 0 | 0 | `R` at `diff-r2/a02-after` with NO re-snapshot; 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| attrib2 | `diff-r3` | COUNT | 0.4164 % | 0.4164 % | 3.1215 % | 194 | 64 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| attrib2 | `diff-r4` | COUNT | 0.1592 % | 0.3062 % | 2.8142 % | 197 | 60 | `S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| attrib2 | `diff-r5` | COUNT | 0.1346 % | 0.4282 % | 2.8736 % | 194 | 61 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 116643 | 1 | 1 | `R` at `diff-r5/close` with NO re-snapshot; 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| attrib2 | `perfA-r1` | COUNT | 0.1120 % | 0.2660 % | 2.8348 % | 197 | 53 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib2 | `perfA-r2` | COUNT | 0.0984 % | 0.3232 % | 2.7806 % | 194 | 58 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `perfA-r2/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib2 | `perfA-r3` | COUNT | 0.0979 % | 0.3077 % | 2.7152 % | 197 | 50 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `perfA-r3/a04-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib2 | `perfB-r1` | COUNT | 0.3837 % | 0.2302 % | 2.8145 % | 197 | 27 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib2 | `perfB-r2` | COUNT | 0.0756 % | 0.7559 % | 2.8655 % | 197 | 36 | `R,RN,S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314,2855082,2856614 | 1 | 1 | `R` at `perfB-r2/a04-after`,`perfB-r2/a01-after` with NO re-snapshot | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| attrib2 | `perfB-r3` | COUNT | 0.0389 % | 0.4673 % | 2.9909 % | 201 | 26 | `S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib2 | `wall-r1` | WALL | 0.0561 % | 0.3084 % | 2.8939 % | 197 | 52 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib2 | `wall-r2` | WALL | 0.0565 % | 0.3251 % | 2.8943 % | 197 | 54 | `R,Rsl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122,50573 | 1 | 1 | `R` at `wall-r2/a05-after`,`wall-r2/a06-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib2 | `wall-r3` | WALL | 0.0837 % | 0.3208 % | 2.6858 % | 197 | 52 | `D,Ds,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `wall-r3/a06-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib2 | `wall-r4` | WALL | 0.0561 % | 0.3363 % | 2.9071 % | 197 | 55 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314 | 1 | 1 | `R` at `wall-r4/a08-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib2 | `wall-r5` | WALL | 0.0837 % | 0.3348 % | 2.7404 % | 194 | 59 | `R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722044,2722106 | 1 | 1 | `R` at `wall-r5/a04-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `alloc-r1` | WALL | 0.1174 % | 0.3131 % | 2.6148 % | 199 | 29 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `alloc-r2` | WALL | 0.0765 % | 0.3442 % | 2.9402 % | 202 | 25 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `alloc-r3` | WALL | 0.0775 % | 0.2326 % | 2.7397 % | 198 | 25 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `alloc-r4` | WALL | 0.1201 % | 0.3203 % | 2.9340 % | 200 | 30 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `alloc-r4/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `alloc-r5` | WALL | 0.0389 % | 0.4669 % | 2.9164 % | 200 | 28 | `RN,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 3064933 | 0 | 0 | `R` at `alloc-r5/b2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `wallpf-r1` | WALL | 0.0000 % | 0.3897 % | 2.7027 % | 203 | 13 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314 | 0 | 0 | `R` at `wallpf-r1/h1-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `wallpf-r2` | WALL | 0.0789 % | 0.0789 % | 2.8050 % | 199 | 21 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `wallpf-r3` | WALL | 0.1543 % | 0.1543 % | 2.7491 % | 205 | 10 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `wallpf-r4` | WALL | 0.1577 % | 0.3943 % | 2.8011 % | 202 | 15 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `wallpf-r5` | WALL | 0.0777 % | 0.4662 % | 2.8276 % | 203 | 19 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `mem-r1` | COUNT | 0.0512 % | 0.5123 % | 2.8585 % | 202 | 22 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `mem-r2` | COUNT | 0.0498 % | 0.2990 % | 3.1432 % | 201 | 24 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `mem-r3` | COUNT | 0.1563 % | 0.4169 % | 2.8999 % | 204 | 18 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `mem-r4` | COUNT | 0.0509 % | 0.2547 % | 2.7956 % | 203 | 20 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `mem-r5` | COUNT | 0.0997 % | 0.3988 % | 2.7878 % | 200 | 25 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `mem-r6` | COUNT | 0.1036 % | 1.5018 % | 3.3551 % | 203 | 20 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `perf` | COUNT | 0.6452 % | 1.3978 % | 3.2997 % | 206 | 12 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `perfstat-r1` | COUNT | 0.0769 % | 0.4613 % | 2.7544 % | 199 | 35 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `perfstat-r2` | COUNT | 0.1040 % | 0.4421 % | 2.8152 % | 199 | 33 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `perfstat-r3` | COUNT | 0.1037 % | 0.3112 % | 2.8487 % | 200 | 35 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 1097257 | 1 | 1 | `R` at `perfstat-r3/b06-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `perfrec-r1` | COUNT | 0.0779 % | 0.3115 % | 2.7643 % | 207 | 10 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `perfrec-r2` | COUNT | 0.0786 % | 0.7862 % | 2.7197 % | 203 | 16 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `perfrec-r2/open` with NO re-snapshot | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| attrib3 | `perfrec-r3` | COUNT | 0.0000 % | 0.6240 % | 2.7701 % | 207 | 10 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `screen` | SCREEN | 0.1167 % | 0.1946 % | 2.6955 % | 199 | 27 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 3019998 | 0 | 0 | `R` at `screen/open` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| attrib3 | `wall-r1` | WALL | 0.1290 % | 0.6706 % | 2.9350 % | 200 | 33 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `wall-r2` | WALL | 0.0779 % | 0.1039 % | 2.7180 % | 199 | 32 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314 | 0 | 0 | `R` at `wall-r2/b03-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `wall-r3` | WALL | 0.1037 % | 0.3888 % | 2.6651 % | 199 | 35 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `wall-r4` | WALL | 0.0775 % | 0.4910 % | 2.9665 % | 199 | 37 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `wall-r4/b04-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `wall-r5` | WALL | 0.2776 % | 0.3534 % | 2.7637 % | 200 | 34 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `xwall-r1` | WALL | 0.1176 % | 0.2744 % | 2.8723 % | 199 | 28 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `xwall-r2` | WALL | 0.1168 % | 0.2726 % | 2.8152 % | 200 | 27 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 4022442 | 1 | 1 | `R` at `xwall-r2/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `xwall-r3` | WALL | 0.1547 % | 0.3868 % | 2.9793 % | 199 | 27 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `xwall-r4` | WALL | 0.0782 % | 0.2735 % | 2.8996 % | 200 | 25 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `xwall-r5` | WALL | 0.1134 % | 0.1134 % | 2.7388 % | 199 | 28 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `ywall-r1` | WALL | 0.1220 % | 0.4270 % | 2.6630 % | 197 | 35 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314 | 0 | 0 | `R` at `ywall-r1/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3 | `ywall-r2` | WALL | 0.0934 % | 0.5291 % | 2.7417 % | 198 | 38 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `ywall-r3` | WALL | 0.0620 % | 0.8065 % | 2.7864 % | 200 | 29 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | **UNPROVEN-FRACTION** |
| attrib3 | `ywall-r4` | WALL | 0.0938 % | 0.1877 % | 2.7518 % | 199 | 32 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3 | `ywall-r5` | WALL | 0.1557 % | 0.5293 % | 2.9208 % | 199 | 34 | `R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314,50122 | 1 | 1 | `R` at `ywall-r5/open`,`ywall-r5/f1-after` with NO re-snapshot | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| attrib3-superseded | `xwall-r1` | SUPERSEDED | 0.0760 % | 0.2279 % | 2.8581 % | 199 | 29 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3-superseded | `xwall-r2` | SUPERSEDED | 0.0391 % | 0.4299 % | 2.9994 % | 197 | 33 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `xwall-r2/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3-superseded | `xwall-r3` | SUPERSEDED | 0.1163 % | 0.3101 % | 2.7446 % | 199 | 29 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `xwall-r3/e2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib3-superseded | `xwall-r4` | SUPERSEDED | 0.0392 % | 0.3922 % | 2.7984 % | 200 | 26 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib3-superseded | `xwall-r5` | SUPERSEDED | 1.1227 % | 0.2710 % | 3.0802 % | 199 | 28 | `R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `xwall-r5/e3-after`,`xwall-r5/close` with NO re-snapshot | **UNPROVEN-FRACTION + UNPROVEN-EVIDENCE** |
| attrib4 | `e3` | COUNT | — (no timed run bracketed) | — (no timed run bracketed) | 2.6908 % | 195 | 34 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | no timed-run bracket | **UNPROVEN-EVIDENCE** |
| attrib4 | `pf-r1` | COUNT | — (no timed run bracketed) | — (no timed run bracketed) | 2.8307 % | 198 | 28 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | no timed-run bracket | **UNPROVEN-EVIDENCE** |
| attrib4 | `pf-r2` | COUNT | — (no timed run bracketed) | — (no timed run bracketed) | 2.7301 % | 198 | 27 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `pf-r2/open` with NO re-snapshot; no timed-run bracket | **UNPROVEN-EVIDENCE** |
| attrib4 | `pf-r3` | COUNT | — (no timed run bracketed) | — (no timed run bracketed) | 2.6998 % | 198 | 27 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | no timed-run bracket | **UNPROVEN-EVIDENCE** |
| attrib4 | `wall-r1` | WALL | 0.0379 % | 0.4173 % | 3.1094 % | 196 | 40 | `R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 1 | 1 | `R` at `wall-r1/a2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib4 | `wall-r2` | WALL | 0.0756 % | 0.3403 % | 2.7787 % | 198 | 32 | `S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | none | 0 | 0 | complete | PROVEN |
| attrib4 | `wall-r3` | WALL | 0.0748 % | 0.2619 % | 2.8155 % | 198 | 31 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 2722314 | 0 | 0 | `R` at `wall-r3/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib4 | `wall-r4` | WALL | 0.0374 % | 0.4867 % | 2.7514 % | 199 | 30 | `Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 50122 | 0 | 0 | `R` at `wall-r4/a2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| attrib4 | `wall-r5` | WALL | 0.0377 % | 0.3390 % | 3.2082 % | 198 | 31 | `Rl+,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+` | 1097257 | 0 | 0 | `R` at `wall-r5/a4-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |

## The audit, per round

Proven = the fraction bar met AND the evidence complete. The two unproven columns overlap: a batch can fail both.

| round | WALL batches | WALL proven | WALL unproven-FRACTION | WALL unproven-EVIDENCE | COUNT/other batches | COUNT/other proven | per-pid appendix |
|---|---:|---:|---:|---:|---:|---:|---|
| fix1 | 22 | **6** | 0 | 16 | 8 | 0 | [`logs/idle-proof-pids-fix1.md`](idle-proof-pids-fix1.md) |
| attrib2 | 5 | **1** | 0 | 4 | 11 | 3 | [`logs/idle-proof-pids-attrib2.md`](idle-proof-pids-attrib2.md) |
| attrib3 | 25 | **14** | 4 | 8 | 14 | 7 | [`logs/idle-proof-pids-attrib3.md`](idle-proof-pids-attrib3.md) |
| attrib3-superseded | 0 | **0** | 0 | 0 | 5 | 2 | [`logs/idle-proof-pids-attrib3-superseded.md`](idle-proof-pids-attrib3-superseded.md) |
| attrib4 | 5 | **1** | 0 | 4 | 4 | 0 | [`logs/idle-proof-pids-attrib4.md`](idle-proof-pids-attrib4.md) |

### The WALL-asserting batches whose FRACTION R2' does not meet

| round | batch | core `cpu2` | sibling `cpu10` |
|---|---|---|---|
| attrib3 | `wall-r1` | 0.1290 % | 0.6706 % |
| attrib3 | `ywall-r2` | 0.0934 % | 0.5291 % |
| attrib3 | `ywall-r3` | 0.0620 % | 0.8065 % |
| attrib3 | `ywall-r5` | 0.1557 % | 0.5293 % |

**These are FLAGGED, not re-measured** — the settler's ruling owes no re-measurement, and a flag that says which rounds a number rests on is worth more than a re-run taken to make a table read clean. Every one of them fails on the SMT SIBLING, not on the measurement core, and by 0.03 to 0.31 percentage points. The reading carries the flag at each place these batches feed a number.

### The WALL-asserting batches whose EVIDENCE is short of R2' (fix3)

| round | batch | what is missing |
|---|---|---|
| fix1 | `leg1-ipm-r3` | `R` at `leg1-ipm-r3/open` with NO re-snapshot |
| fix1 | `leg1-ssn-r2` | `R` at `leg1-ssn-r2/close` with NO re-snapshot |
| fix1 | `leg1-walk-r2` | `R` at `leg1-walk-r2/close` with NO re-snapshot |
| fix1 | `leg2-ipm-off-passA` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-ipm-off-passB` | `R` at `leg2-ipm-off-passB/r1`,`leg2-ipm-off-passB/r2`,`leg2-ipm-off-passB/r3` with NO re-snapshot; 3 alternation snapshot(s) missing |
| fix1 | `leg2-ipm-sink-passA` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-ipm-sink-passB` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-ssn-off-passA` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-ssn-off-passB` | `R` at `leg2-ssn-off-passB/r1` with NO re-snapshot; 3 alternation snapshot(s) missing |
| fix1 | `leg2-ssn-sink-passA` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-ssn-sink-passB` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-walk-off-passA` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-walk-off-passB` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-walk-sink-passA` | 3 alternation snapshot(s) missing |
| fix1 | `leg2-walk-sink-passB` | `R` at `leg2-walk-sink-passB/close` with NO re-snapshot; 3 alternation snapshot(s) missing |
| fix1 | `interior-wall` | 6 alternation snapshot(s) missing |
| attrib2 | `wall-r2` | `R` at `wall-r2/a05-after`,`wall-r2/a06-after` with NO re-snapshot |
| attrib2 | `wall-r3` | `R` at `wall-r3/a06-after` with NO re-snapshot |
| attrib2 | `wall-r4` | `R` at `wall-r4/a08-after` with NO re-snapshot |
| attrib2 | `wall-r5` | `R` at `wall-r5/a04-after` with NO re-snapshot |
| attrib3 | `alloc-r4` | `R` at `alloc-r4/open` with NO re-snapshot |
| attrib3 | `alloc-r5` | `R` at `alloc-r5/b2-after` with NO re-snapshot |
| attrib3 | `wallpf-r1` | `R` at `wallpf-r1/h1-after` with NO re-snapshot |
| attrib3 | `wall-r2` | `R` at `wall-r2/b03-after` with NO re-snapshot |
| attrib3 | `wall-r4` | `R` at `wall-r4/b04-after` with NO re-snapshot |
| attrib3 | `xwall-r2` | `R` at `xwall-r2/open` with NO re-snapshot |
| attrib3 | `ywall-r1` | `R` at `ywall-r1/open` with NO re-snapshot |
| attrib3 | `ywall-r5` | `R` at `ywall-r5/open`,`ywall-r5/f1-after` with NO re-snapshot |
| attrib4 | `wall-r1` | `R` at `wall-r1/a2-after` with NO re-snapshot |
| attrib4 | `wall-r3` | `R` at `wall-r3/open` with NO re-snapshot |
| attrib4 | `wall-r4` | `R` at `wall-r4/a2-after` with NO re-snapshot |
| attrib4 | `wall-r5` | `R` at `wall-r5/a4-after` with NO re-snapshot |

**These too are FLAGGED, not re-measured.** The gap is in the RECORD, not in the numbers: the fractions these batches report are the fractions they measured, and CLAUDE.md §7's own reason for the solo rule — that a busy neighbour can only cost the measurement time — means an unwatched moment biases a wall number in the SLOW direction, never the fast one. Where such a batch feeds a number, the reading says so.

### Rounds whose retained snapshots cannot support R2' at all

* **round 1 (superseded)** (`logs/round1-superseded/L*.log`): Round 1's legs predate R2 entirely: their solo evidence is the `pgrep` audit astra's I1 refused, and they carry no `PS_SNAPSHOT` block. Every number they produced was SUPERSEDED by the fix1 re-measurement and is quoted nowhere in the reading. **R2' cannot be computed for them and is not claimed.**
* **T8.9r-attrib5 (section 14)** (`attribution-interior/t84/single-row/logs/*.log`): The single-row leg's batch logs carry `PGREP_INLOCK`, `FOREGROUND_START`/`_END` and `ARGV_LEN` but no `PS_SNAPSHOT`/`CPUSTAT` bracket. **That leg asserts no wall clock** — section 14 says so in its own closing paragraph, and its elapsed figures are printed as informational under CLAUDE.md section 7 -- so R2' has nothing to govern there. Its instruction and branch counts are the asserted currency and are scheduling-invariant at `MKL_NUM_THREADS=1`. **R2' is NOT claimed for that leg, and its wall figures stay informational.**

### Where every foreign pid is listed

R2' asks for every foreign pid and state seen in any snapshot, LISTED. That is one file per round, written by `idle_proof.py --pids` from the same snapshots the fractions come from, ~200 pids per batch:

* fix1 — [`logs/idle-proof-pids-fix1.md`](idle-proof-pids-fix1.md)
* attrib2 — [`logs/idle-proof-pids-attrib2.md`](idle-proof-pids-attrib2.md)
* attrib3 — [`logs/idle-proof-pids-attrib3.md`](idle-proof-pids-attrib3.md)
* attrib3-superseded — [`logs/idle-proof-pids-attrib3-superseded.md`](idle-proof-pids-attrib3-superseded.md)
* attrib4 — [`logs/idle-proof-pids-attrib4.md`](idle-proof-pids-attrib4.md)

The five-busiest list in each appendix below is a summary of those files, not a substitute for them.

---

# Appendix — `idle_proof.py`'s output, verbatim, per round

Each block below is the tool's own stdout for that round's logs, unedited. The tool's own preamble still describes R2 AS WRITTEN (Test 1) and the pinned-core test (Test 2); under R2' **Test 2 is the rule** and Test 1 is the superseded bar, retained because the amendment does not delete the numbers it was taken on.

## fix round 1 -- the reading's own legs (reading.md sections 1-9)

```
# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| L4-leg1-ipm-r1.log | leg1-ipm-r1 | 48.27 | 4 | 196 | 28 | 1.420 (pid 50122) | **2.9418 %** | S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| L4-leg1-ipm-r2.log | leg1-ipm-r2 | 48.30 | 4 | 198 | 29 | 1.400 (pid 50122) | **2.8986 %** | S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| L4-leg1-ipm-r3.log | leg1-ipm-r3 | 53.21 | 4 | 195 | 34 | 1.550 (pid 50122) | **2.9130 %** | R,S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| L4-leg1-ssn-r1.log | leg1-ssn-r1 | 37.41 | 4 | 196 | 22 | 1.060 (pid 50122) | **2.8335 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| L4-leg1-ssn-r2.log | leg1-ssn-r2 | 37.39 | 4 | 196 | 22 | 1.010 (pid 50122) | **2.7013 %** | Rsl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 4022442 | 0 | 0 | 0 | 0 | **NOT met** |
| L4-leg1-ssn-r3.log | leg1-ssn-r3 | 37.34 | 4 | 195 | 25 | 1.020 (pid 50122) | **2.7317 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| L4-leg1-walk-r1.log | leg1-walk-r1 | 414.78 | 4 | 192 | 40 | 11.740 (pid 50122) | **2.8304 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| L4-leg1-walk-r2.log | leg1-walk-r2 | 414.82 | 4 | 195 | 53 | 10.830 (pid 50122) | **2.6108 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722106 | 1 | 1 | 0 | 0 | **NOT met** |
| L4-leg1-walk-r3.log | leg1-walk-r3 | 414.84 | 4 | 196 | 54 | 11.710 (pid 50122) | **2.8228 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| L5-leg1perf-ipm.log | leg1perf-ipm | 32.32 | 8 | 201 | 32 | 0.940 (pid 50122) | **2.9084 %** | RN,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2522981 | 0 | 0 | 0 | 30 | **NOT met** |
| L5-leg1perf-ssn.log | leg1perf-ssn | 28.61 | 8 | 201 | 29 | 0.790 (pid 50122) | **2.7613 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 30 | **NOT met** |
| L5-leg1perf-walk.log | leg1perf-walk | 15.60 | 8 | 206 | 16 | 0.450 (pid 50122) | **2.8846 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 30 | **NOT met** |
| L6-leg2-ipm-off-passA.log | leg2-ipm-off-passA | 228.21 | 5 | 195 | 61 | 6.120 (pid 50122) | **2.6817 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-ipm-off-passB.log | leg2-ipm-off-passB | 238.13 | 5 | 195 | 68 | 6.550 (pid 50122) | **2.7506 %** | R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 3819500,50122,50634 | 2 | 2 | 0 | 3 | **NOT met** |
| L6-leg2-ipm-sink-passA.log | leg2-ipm-sink-passA | 228.50 | 5 | 200 | 71 | 6.070 (pid 50122) | **2.6565 %** | S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-ipm-sink-passB.log | leg2-ipm-sink-passB | 229.97 | 5 | 198 | 74 | 6.390 (pid 50122) | **2.7786 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-ssn-off-passA.log | leg2-ssn-off-passA | 369.77 | 5 | 198 | 74 | 10.220 (pid 50122) | **2.7639 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-ssn-off-passB.log | leg2-ssn-off-passB | 370.45 | 5 | 197 | 72 | 10.400 (pid 50122) | **2.8074 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-ssn-sink-passA.log | leg2-ssn-sink-passA | 369.24 | 5 | 197 | 72 | 9.830 (pid 50122) | **2.6622 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-ssn-sink-passB.log | leg2-ssn-sink-passB | 370.48 | 5 | 196 | 70 | 9.860 (pid 50122) | **2.6614 %** | S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-walk-off-passA.log | leg2-walk-off-passA | 140.14 | 5 | 196 | 65 | 3.680 (pid 50122) | **2.6259 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-walk-off-passB.log | leg2-walk-off-passB | 140.11 | 5 | 198 | 57 | 4.000 (pid 50122) | **2.8549 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-walk-sink-passA.log | leg2-walk-sink-passA | 140.55 | 5 | 195 | 60 | 3.810 (pid 50122) | **2.7108 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 3 | **NOT met** |
| L6-leg2-walk-sink-passB.log | leg2-walk-sink-passB | 140.38 | 5 | 198 | 55 | 3.780 (pid 50122) | **2.6927 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 3 | **NOT met** |
| L7-interior-perfA.log | interior-perfA | 111.82 | 5 | 196 | 62 | 3.250 (pid 50122) | **2.9065 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 6 | **NOT met** |
| L7-interior-perfB.log | interior-perfB | 111.61 | 5 | 199 | 56 | 3.010 (pid 50122) | **2.6969 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 6 | **NOT met** |
| L7-interior-wall.log | interior-wall | 111.14 | 5 | 199 | 57 | 3.110 (pid 50122) | **2.7983 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 6 | **NOT met** |
| L8-interior-cells-r1.log | interior-cells-r1 | 62.11 | 13 | 198 | 46 | 1.770 (pid 50122) | **2.8498 %** | RNl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 4022478 | 0 | 0 | 0 | 22 | **NOT met** |
| L8-interior-cells-r2.log | interior-cells-r2 | 67.40 | 13 | 198 | 50 | 1.850 (pid 50122) | **2.7448 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 1097257 | 1 | 1 | 0 | 22 | **NOT met** |
| L8-interior-cells-r3.log | interior-cells-r3 | 67.38 | 13 | 196 | 59 | 1.950 (pid 50122) | **2.8940 %** | R,Rl+,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 1097257,50122 | 1 | 1 | 0 | 22 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| L4-leg1-ipm-r1.log | leg1-ipm-r1 | 2 | 46.68 | **0.020** | **0.0428 %** | **0.200** | **0.4284 %** | 0.000 | 0.030 | 41.53 | 45.87 | 0.020 | 0.230 | complete | PINNED-CLEAN |
| L4-leg1-ipm-r2.log | leg1-ipm-r2 | 2 | 46.69 | **0.020** | **0.0428 %** | **0.120** | **0.2570 %** | 0.000 | 0.060 | 41.50 | 45.90 | 0.000 | 0.230 | complete | PINNED-CLEAN |
| L4-leg1-ipm-r3.log | leg1-ipm-r3 | 2 | 46.60 | **0.020** | **0.0429 %** | **0.080** | **0.1717 %** | 0.000 | 0.070 | 41.46 | 45.82 | 0.020 | 0.240 | `R` at `leg1-ipm-r3/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| L4-leg1-ssn-r1.log | leg1-ssn-r1 | 2 | 35.86 | **0.010** | **0.0279 %** | **0.140** | **0.3904 %** | 0.000 | 0.060 | 30.08 | 35.09 | 0.000 | 0.180 | complete | PINNED-CLEAN |
| L4-leg1-ssn-r2.log | leg1-ssn-r2 | 2 | 35.80 | **0.020** | **0.0559 %** | **0.040** | **0.1117 %** | 0.000 | 0.020 | 30.10 | 35.05 | 0.010 | 0.180 | `R` at `leg1-ssn-r2/close` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| L4-leg1-ssn-r3.log | leg1-ssn-r3 | 2 | 35.78 | **0.020** | **0.0559 %** | **0.080** | **0.2236 %** | 0.000 | 0.040 | 30.18 | 35.00 | 0.000 | 0.170 | complete | PINNED-CLEAN |
| L4-leg1-walk-r1.log | leg1-walk-r1 | 2 | 413.20 | **0.000** | **0.0000 %** | **0.710** | **0.1718 %** | 0.000 | 0.270 | 398.25 | 410.29 | 0.330 | 2.220 | complete | PINNED-CLEAN |
| L4-leg1-walk-r2.log | leg1-walk-r2 | 2 | 413.19 | **0.000** | **0.0000 %** | **0.850** | **0.2057 %** | 0.000 | 0.170 | 398.34 | 410.18 | 0.220 | 2.250 | `R` at `leg1-walk-r2/close` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| L4-leg1-walk-r3.log | leg1-walk-r3 | 2 | 413.20 | **0.000** | **0.0000 %** | **0.540** | **0.1307 %** | 0.000 | 0.220 | 398.01 | 410.21 | 0.310 | 2.230 | complete | PINNED-CLEAN |
| L5-leg1perf-ipm.log | leg1perf-ipm | 36 | 27.99 | **0.210** | **0.7503 %** | **0.080** | **0.2858 %** | 0.000 | 0.070 | 24.52 | 27.60 | 0.160 | 0.150 | `R` at `leg1perf-ipm/passB-r3` with NO re-snapshot; 30 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| L5-leg1perf-ssn.log | leg1perf-ssn | 36 | 24.35 | **0.150** | **0.6160 %** | **0.100** | **0.4107 %** | 0.000 | 0.060 | 20.05 | 23.92 | 0.170 | 0.120 | 30 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| L5-leg1perf-walk.log | leg1perf-walk | 36 | 11.28 | **0.180** | **1.5957 %** | **0.030** | **0.2660 %** | 0.000 | 0.020 | 9.02 | 10.97 | 0.160 | 0.060 | 30 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| L6-leg2-ipm-off-passA.log | leg2-ipm-off-passA | 6 | 225.98 | **0.040** | **0.0177 %** | **0.220** | **0.0974 %** | 0.000 | 0.080 | 223.62 | 224.42 | 0.040 | 1.490 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ipm-off-passB.log | leg2-ipm-off-passB | 6 | 225.84 | **0.050** | **0.0221 %** | **0.240** | **0.1063 %** | 0.000 | 0.080 | 223.46 | 224.25 | 0.050 | 1.520 | `R` at `leg2-ipm-off-passB/r1`,`leg2-ipm-off-passB/r2`,`leg2-ipm-off-passB/r3` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ipm-sink-passA.log | leg2-ipm-sink-passA | 6 | 226.19 | **0.060** | **0.0265 %** | **0.570** | **0.2520 %** | 0.000 | 0.340 | 223.81 | 224.57 | 0.040 | 1.530 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ipm-sink-passB.log | leg2-ipm-sink-passB | 6 | 227.66 | **0.060** | **0.0264 %** | **0.490** | **0.2152 %** | 0.000 | 0.110 | 225.33 | 226.05 | 0.040 | 1.530 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ssn-off-passA.log | leg2-ssn-off-passA | 6 | 367.49 | **0.090** | **0.0245 %** | **0.930** | **0.2531 %** | 0.000 | 0.380 | 363.23 | 364.88 | 0.050 | 2.480 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ssn-off-passB.log | leg2-ssn-off-passB | 6 | 368.17 | **0.080** | **0.0217 %** | **0.480** | **0.1304 %** | 0.000 | 0.190 | 363.82 | 365.57 | 0.050 | 2.470 | `R` at `leg2-ssn-off-passB/r1` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ssn-sink-passA.log | leg2-ssn-sink-passA | 6 | 366.97 | **0.080** | **0.0218 %** | **0.520** | **0.1417 %** | 0.000 | 0.210 | 362.68 | 364.40 | 0.050 | 2.470 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-ssn-sink-passB.log | leg2-ssn-sink-passB | 6 | 368.22 | **0.080** | **0.0217 %** | **0.830** | **0.2254 %** | 0.000 | 0.330 | 363.93 | 365.60 | 0.060 | 2.470 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-walk-off-passA.log | leg2-walk-off-passA | 6 | 137.88 | **0.050** | **0.0363 %** | **0.400** | **0.2901 %** | 0.010 | 0.220 | 136.74 | 136.96 | 0.020 | 0.860 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-walk-off-passB.log | leg2-walk-off-passB | 6 | 137.88 | **0.040** | **0.0290 %** | **0.390** | **0.2829 %** | 0.000 | 0.130 | 136.76 | 136.97 | 0.030 | 0.860 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-walk-sink-passA.log | leg2-walk-sink-passA | 6 | 138.30 | **0.040** | **0.0289 %** | **0.280** | **0.2025 %** | 0.000 | 0.180 | 136.95 | 137.38 | 0.030 | 0.870 | 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L6-leg2-walk-sink-passB.log | leg2-walk-sink-passB | 6 | 138.14 | **0.050** | **0.0362 %** | **0.220** | **0.1593 %** | 0.000 | 0.140 | 137.06 | 137.21 | 0.040 | 0.860 | `R` at `leg2-walk-sink-passB/close` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L7-interior-perfA.log | interior-perfA | 9 | 109.47 | **0.410** | **0.3745 %** | **0.510** | **0.4659 %** | 0.440 | 0.280 | 97.73 | 108.03 | 0.510 | 0.520 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L7-interior-perfB.log | interior-perfB | 9 | 109.31 | **0.050** | **0.0457 %** | **0.330** | **0.3019 %** | 0.000 | 0.080 | 98.64 | 108.73 | 0.030 | 0.510 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L7-interior-wall.log | interior-wall | 9 | 108.77 | **0.060** | **0.0552 %** | **0.330** | **0.3034 %** | 0.000 | 0.150 | 98.37 | 108.21 | 0.040 | 0.480 | 6 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L8-interior-cells-r1.log | interior-cells-r1 | 33 | 55.20 | **0.170** | **0.3080 %** | **0.140** | **0.2536 %** | 0.000 | 0.100 | 50.23 | 54.74 | 0.110 | 0.230 | `R` at `interior-cells-r1/hs071_x1_fixed` with NO re-snapshot; 22 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L8-interior-cells-r2.log | interior-cells-r2 | 33 | 55.49 | **0.200** | **0.3604 %** | **0.150** | **0.2703 %** | 0.000 | 0.080 | 50.58 | 54.99 | 0.130 | 0.240 | `R` at `interior-cells-r2/f7_n10000_bound_physics` with NO re-snapshot; 22 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| L8-interior-cells-r3.log | interior-cells-r3 | 33 | 55.35 | **0.200** | **0.3613 %** | **0.160** | **0.2891 %** | 0.000 | 0.080 | 50.49 | 54.89 | 0.150 | 0.250 | `R` at `interior-cells-r3/f7_n1000_bound_neutral`,`interior-cells-r3/f7_n1000_bound_physics` with NO re-snapshot; 22 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |

## The five busiest foreign processes per batch

**Every foreign pid of every batch, with every state observed for it, is `logs/idle-proof-pids-fix1.md`** (settler R13). The five below are a summary of that file, not a substitute for it.

- L4-leg1-ipm-r1.log / leg1-ipm-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2491876,2491915,2491917,2491929,2491941,2491943,2491953,2491955,2491958,2491970,2492703,2492708,2492713,2492715,2492718,2492720,2492722,2493369,2493380,2493383,2493390,2493412,2493414,2493461,2493465,2493472,2493569,2493617
- L4-leg1-ipm-r1.log / leg1-ipm-r1 (window 48.27 s): pid 50122 +1.42 s; pid 1097257 +0.54 s; pid 2721602 +0.28 s; pid 2722314 +0.20 s; pid 4022442 +0.13 s
- L4-leg1-ipm-r2.log / leg1-ipm-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2491952,2492718,2492720,2492722,2493369,2493380,2493383,2493390,2493412,2493414,2493461,2493569,2495570,2495577,2495584,2495587,2495590,2495596,2495598,2495621,2495625,2496287,2496303,2496306,2496310,2496315,2496318,2496320,2496366
- L4-leg1-ipm-r2.log / leg1-ipm-r2 (window 48.30 s): pid 50122 +1.40 s; pid 1097257 +0.53 s; pid 2722314 +0.21 s; pid 4022442 +0.17 s; pid 2722912 +0.09 s
- L4-leg1-ipm-r3.log / leg1-ipm-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2493465,2493472,2493617,2495584,2495621,2495625,2496287,2496303,2496306,2496310,2496315,2496318,2496320,2496366,2498315,2498319,2498344,2498359,2498363,2498367,2498369,2498372,2498376,2498378,2499070,2499078,2499086,2499088,2499093,2499096,2499099,2499102,2499105,2499108
- L4-leg1-ipm-r3.log / leg1-ipm-r3 (window 53.21 s): pid 50122 +1.55 s; pid 1097257 +0.53 s; pid 2722314 +0.21 s; pid 4022442 +0.16 s; pid 2721602 +0.14 s
- L4-leg1-ipm-r3.log / leg1-ipm-r3: BOX_PAUSE 2026-09-11T15:34:45Z tag=leg1-ipm-r3/open try=1 -- a foreign process is in state R.
- L4-leg1-ipm-r3.log / leg1-ipm-r3: BOX_PAUSE_GIVEUP 2026-09-11T15:34:50Z tag=leg1-ipm-r3/open -- still R after 1 pauses.
- L4-leg1-ssn-r1.log / leg1-ssn-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2498367,2499070,2499078,2499086,2499088,2499096,2499099,2499102,2499105,2499108,2501030,2501056,2501060,2501065,2501437,2501725,2501734,2501738,2501741,2501747,2501750,2501775
- L4-leg1-ssn-r1.log / leg1-ssn-r1 (window 37.41 s): pid 50122 +1.06 s; pid 1097257 +0.42 s; pid 2721602 +0.27 s; pid 2722314 +0.18 s; pid 4022442 +0.11 s
- L4-leg1-ssn-r2.log / leg1-ssn-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2499093,2501065,2501437,2501725,2501738,2501741,2501747,2501750,2501775,2503437,2503701,2503703,2503709,2503712,2503715,2503719,2504023,2504368,2504386,2504402,2504405,2504416
- L4-leg1-ssn-r2.log / leg1-ssn-r2 (window 37.39 s): pid 50122 +1.01 s; pid 1097257 +0.41 s; pid 2721602 +0.34 s; pid 2722314 +0.14 s; pid 4022442 +0.12 s
- L4-leg1-ssn-r3.log / leg1-ssn-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2501734,2503709,2503712,2503715,2503719,2504023,2504368,2504386,2504402,2504405,2504416,2506341,2506346,2506352,2506355,2506358,2506363,2506366,2507037,2507044,2507052,2507053,2507057,2507060,2507063
- L4-leg1-ssn-r3.log / leg1-ssn-r3 (window 37.34 s): pid 50122 +1.02 s; pid 1097257 +0.41 s; pid 2722314 +0.16 s; pid 4022442 +0.12 s; pid 2722196 +0.12 s
- L4-leg1-walk-r1.log / leg1-walk-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2491874,2491878,2506352,2507037,2507044,2507052,2507053,2507057,2507060,2507063,2508217,2508325,2508326,2508985,2508990,2509259,2509268,2509270,2509289,2509299,2509301,2509305,2509307,2509310,2509312,2509315,2510110,2510113,2510114,2510245,2510249,2510254,2510256,2510275,2510279,2510281,2510283,2510285,2510287,2510289
- L4-leg1-walk-r1.log / leg1-walk-r1 (window 414.78 s): pid 50122 +11.74 s; pid 1097257 +3.58 s; pid 2721602 +1.98 s; pid 2722314 +1.70 s; pid 4022442 +1.04 s
- L4-leg1-walk-r2.log / leg1-walk-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2616185,2616217,2616219,2621559,2621581,2621596,2621603,2621608,2621610,2621612,2621615,2621617,2621619,2621621,2621622,2621624,2622320,2622485,2622486,2622487,2622647,2622656,2622667,2622670,2622679,2622683,2622685,2622687,2622689,2622691,2622692,2622694,2622696,2622698,2622700,2623894,2623924,2623926,2623932,2623941,2623943,2623952,2623955,2623963,2623964,2623966,2623968,2623971,2623975,2623976,2623979,2623980,2623982
- L4-leg1-walk-r2.log / leg1-walk-r2 (window 414.82 s): pid 50122 +10.83 s; pid 1097257 +3.78 s; pid 2721602 +2.21 s; pid 2722314 +1.55 s; pid 4022442 +0.92 s
- L4-leg1-walk-r2.log / leg1-walk-r2: BOX_PAUSE 2026-09-11T17:09:03Z tag=leg1-walk-r2/close try=1 -- a foreign process is in state R.
- L4-leg1-walk-r2.log / leg1-walk-r2: BOX_PAUSE_GIVEUP 2026-09-11T17:09:08Z tag=leg1-walk-r2/close -- still R after 1 pauses.
- L4-leg1-walk-r3.log / leg1-walk-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2512494,2512495,2512496,2513688,2513690,2513698,2515075,2515079,2515081,2515085,2515089,2515093,2515097,2515100,2515107,2515109,2515110,2515112,2515114,2515843,2515844,2515871,2516120,2516160,2516166,2516169,2516171,2516173,2516176,2516179,2516181,2516183,2516185,2516188,2516204,2516207,2517107,2517109,2517131,2517196,2517241,2517249,2517254,2517256,2517258,2517260,2517262,2517265,2517266,2517268,2517270,2517272,2517275,2517293
- L4-leg1-walk-r3.log / leg1-walk-r3 (window 414.84 s): pid 50122 +11.71 s; pid 1097257 +3.45 s; pid 2721602 +2.03 s; pid 2722314 +1.51 s; pid 4022442 +0.87 s
- L5-leg1perf-ipm.log / leg1perf-ipm: TRANSIENT pids (present in some snapshot, not in both ends): 2517254,2517258,2517265,2517268,2517272,2517275,2517293,2518602,2518604,2518605,2518607,2518609,2518611,2519301,2519314,2519338,2519742,2520032,2520056,2520069,2520071,2520756,2520792,2521501,2521526,2522198,2522213,2522215,2522251,2522968,2522994,2523248
- L5-leg1perf-ipm.log / leg1perf-ipm (window 32.32 s): pid 50122 +0.94 s; pid 1097257 +0.36 s; pid 2721602 +0.26 s; pid 1312476 +0.14 s; pid 2722314 +0.13 s
- L5-leg1perf-ssn.log / leg1perf-ssn: TRANSIENT pids (present in some snapshot, not in both ends): 2518612,2519742,2520056,2520071,2520792,2521501,2521526,2522198,2522213,2522215,2522251,2522968,2522994,2524978,2525002,2525015,2525699,2525702,2525703,2526410,2527155,2527190,2527424,2527749,2527874,2527909,2527911,2528594,2528629
- L5-leg1perf-ssn.log / leg1perf-ssn (window 28.61 s): pid 50122 +0.79 s; pid 1097257 +0.27 s; pid 2722314 +0.15 s; pid 1312476 +0.11 s; pid 1861779 +0.10 s
- L5-leg1perf-walk.log / leg1perf-walk: TRANSIENT pids (present in some snapshot, not in both ends): 2525002,2525699,2525703,2526410,2527424,2527749,2527909,2527911,2530633,2531350,2531384,2532080,2532484,2532500,2532817,2534206
- L5-leg1perf-walk.log / leg1perf-walk (window 15.60 s): pid 50122 +0.45 s; pid 1097257 +0.18 s; pid 1861779 +0.10 s; pid 1312476 +0.10 s; pid 2722314 +0.07 s
- L6-leg2-ipm-off-passA.log / leg2-ipm-off-passA: TRANSIENT pids (present in some snapshot, not in both ends): 2544996,2544999,2545021,2547830,2548528,2548545,2548547,2548549,2548551,2548556,2548557,2548558,2548562,2548564,2548567,2548588,2550628,2550638,2550642,2550663,2550665,2550667,2550669,2550671,2550673,2550674,2550676,2550678,2550680,2550682,2551382,2551445,2551448,2551450,2551452,2551454,2551456,2551459,2551462,2551464,2551469,2551470,2551478,2551495,2551497,2551498,2552156,2552209,2552232,2552234,2552258,2552260,2552264,2552266,2552269,2552271,2552273,2552275,2552278,2552280,2552281
- L6-leg2-ipm-off-passA.log / leg2-ipm-off-passA (window 228.21 s): pid 50122 +6.12 s; pid 1097257 +1.83 s; pid 2721602 +0.92 s; pid 2722314 +0.81 s; pid 4022442 +0.40 s
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: TRANSIENT pids (present in some snapshot, not in both ends): 2551470,2551478,2552156,2552209,2552234,2552258,2552260,2552264,2552266,2552269,2552271,2552273,2552275,2552278,2552280,2552281,2554276,2554336,2554338,2554340,2554353,2554355,2554358,2554360,2554363,2554380,2554384,2554385,2554395,2554397,2555079,2555118,2555166,2555168,2555179,2555184,2555185,2555187,2555189,2555191,2555193,2555194,2555196,2555198,2555204,2555207,2555988,2556003,2556018,2556020,2556043,2556044,2556047,2556050,2556053,2556054,2556056,2556060,2556061,2556064,2556066,2556068,2556070,2556072,2556082,2556085,2556098,2556207
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB (window 238.13 s): pid 50122 +6.55 s; pid 1097257 +2.02 s; pid 2721602 +1.16 s; pid 2722314 +0.88 s; pid 2721730 +0.67 s
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE 2026-09-11T16:15:24Z tag=leg2-ipm-off-passB/r1 try=1 -- a foreign process is in state R.
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE 2026-09-11T16:16:45Z tag=leg2-ipm-off-passB/r2 try=1 -- a foreign process is in state R.
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE_GIVEUP 2026-09-11T16:15:29Z tag=leg2-ipm-off-passB/r1 -- still R after 1 pauses.
- L6-leg2-ipm-off-passB.log / leg2-ipm-off-passB: BOX_PAUSE_GIVEUP 2026-09-11T16:16:50Z tag=leg2-ipm-off-passB/r2 -- still R after 1 pauses.
- L6-leg2-ipm-sink-passA.log / leg2-ipm-sink-passA: TRANSIENT pids (present in some snapshot, not in both ends): 2555988,2556018,2556020,2556047,2556050,2556053,2556056,2556060,2556066,2556068,2556070,2556072,2556082,2556085,2556098,2556207,2558190,2558299,2558386,2558394,2558490,2558537,2558543,2558545,2558547,2558553,2558556,2558558,2558567,2558569,2558571,2558576,2558579,2558596,2558598,2558600,2558602,2558604,2559389,2559408,2559430,2559432,2559435,2559437,2559440,2559444,2559446,2559447,2559451,2559453,2559455,2559457,2559458,2559464,2560194,2560271,2560275,2560277,2560281,2560285,2560287,2560289,2560293,2560295,2560298,2560301,2560318,2560345,2560346,2560348,2560350
- L6-leg2-ipm-sink-passA.log / leg2-ipm-sink-passA (window 228.50 s): pid 50122 +6.07 s; pid 1097257 +2.00 s; pid 2721602 +1.18 s; pid 2722314 +1.05 s; pid 2722044 +0.64 s
- L6-leg2-ipm-sink-passB.log / leg2-ipm-sink-passB: TRANSIENT pids (present in some snapshot, not in both ends): 2556043,2556044,2556054,2560194,2560271,2560275,2560277,2560281,2560285,2560287,2560289,2560293,2560295,2560298,2560301,2560318,2560345,2560346,2560348,2560350,2562445,2562505,2562508,2562511,2562513,2562518,2562520,2562522,2562524,2562526,2562528,2562530,2562531,2562533,2562534,2562535,2562536,2562539,2562541,2562544,2563325,2563327,2563329,2563344,2563350,2563352,2563355,2563359,2563361,2563378,2563380,2563381,2563383,2563393,2563395,2563399,2563400,2564144,2564193,2564195,2564198,2564206,2564207,2564209,2564214,2564216,2564218,2564219,2564221,2564224,2564226,2564228,2564230,2564233
- L6-leg2-ipm-sink-passB.log / leg2-ipm-sink-passB (window 229.97 s): pid 50122 +6.39 s; pid 1097257 +2.04 s; pid 2721602 +1.15 s; pid 2722314 +0.86 s; pid 4022442 +0.58 s
- L6-leg2-ssn-off-passA.log / leg2-ssn-off-passA: TRANSIENT pids (present in some snapshot, not in both ends): 2562534,2562535,2562536,2564144,2564193,2564195,2564198,2564206,2564207,2564214,2564216,2564218,2564219,2564221,2564224,2564226,2564228,2564230,2564233,2565565,2566412,2566468,2566471,2566476,2566481,2566485,2566487,2566491,2566492,2566494,2566499,2566501,2566503,2566506,2566510,2566512,2566529,2567234,2567238,2567239,2567372,2567424,2567432,2567434,2567436,2567439,2567443,2567448,2567450,2567452,2567455,2567457,2567459,2567477,2567478,2567480,2567483,2568309,2568360,2568365,2568367,2568372,2568377,2568379,2568381,2568389,2568392,2568394,2568396,2568413,2568414,2568416,2568418,2568422
- L6-leg2-ssn-off-passA.log / leg2-ssn-off-passA (window 369.77 s): pid 50122 +10.22 s; pid 1097257 +3.38 s; pid 2721602 +1.73 s; pid 2722314 +1.38 s; pid 4022442 +0.97 s
- L6-leg2-ssn-off-passB.log / leg2-ssn-off-passB: TRANSIENT pids (present in some snapshot, not in both ends): 2556003,2567234,2567238,2567239,2568309,2568360,2568365,2568367,2568372,2568377,2568379,2568381,2568389,2568392,2568394,2568396,2568413,2568414,2568416,2568418,2568422,2570575,2570597,2570603,2570604,2570630,2570634,2570637,2570642,2570647,2570649,2570651,2570654,2570656,2570658,2570668,2570676,2570678,2570680,2570684,2571526,2571538,2571543,2571545,2571548,2571551,2571553,2571555,2571572,2571573,2571575,2571577,2571581,2571584,2571586,2571588,2572441,2572460,2572462,2572464,2572472,2572474,2572477,2572479,2572490,2572494,2572496,2572498,2572500,2572501,2572503,2572505
- L6-leg2-ssn-off-passB.log / leg2-ssn-off-passB (window 370.45 s): pid 50122 +10.40 s; pid 1097257 +3.34 s; pid 2721602 +1.87 s; pid 2722314 +1.38 s; pid 4022442 +0.84 s
- L6-leg2-ssn-sink-passA.log / leg2-ssn-sink-passA: TRANSIENT pids (present in some snapshot, not in both ends): 2570597,2570603,2570604,2572441,2572460,2572462,2572464,2572474,2572477,2572479,2572490,2572494,2572496,2572498,2572500,2572501,2572503,2572505,2573572,2574504,2574508,2574509,2574668,2574701,2574718,2574720,2574723,2574727,2574729,2574731,2574733,2574734,2574736,2574738,2574740,2574743,2574745,2575565,2575598,2575615,2575617,2575620,2575623,2575625,2575628,2575630,2575639,2575641,2575643,2575645,2575648,2575651,2575654,2576437,2576441,2576442,2576481,2576522,2576532,2576534,2576540,2576542,2576544,2576547,2576548,2576550,2576552,2576554,2576557,2576560,2576563,2576565
- L6-leg2-ssn-sink-passA.log / leg2-ssn-sink-passA (window 369.24 s): pid 50122 +9.83 s; pid 1097257 +3.21 s; pid 2721602 +1.76 s; pid 2722314 +1.41 s; pid 4022442 +0.83 s
- L6-leg2-ssn-sink-passB.log / leg2-ssn-sink-passB: TRANSIENT pids (present in some snapshot, not in both ends): 2555079,2576437,2576441,2576442,2576481,2576522,2576532,2576534,2576540,2576542,2576544,2576547,2576548,2576550,2576552,2576554,2576557,2576560,2576563,2576565,2578718,2578771,2578773,2578778,2578781,2578783,2578785,2578789,2578791,2578794,2578797,2578800,2578802,2578805,2579595,2579642,2579649,2579778,2579829,2579831,2579834,2579836,2579854,2579856,2579859,2579862,2579865,2579869,2579870,2579871,2579873,2579874,2579875,2579891,2579893,2580770,2580772,2580774,2580782,2580784,2580787,2580790,2580793,2580796,2580798,2580799,2580815,2580817,2580819,2580820
- L6-leg2-ssn-sink-passB.log / leg2-ssn-sink-passB (window 370.48 s): pid 50122 +9.86 s; pid 1097257 +3.35 s; pid 2721602 +1.82 s; pid 2722314 +1.39 s; pid 4022442 +0.85 s
- L6-leg2-walk-off-passA.log / leg2-walk-off-passA: TRANSIENT pids (present in some snapshot, not in both ends): 2517107,2517109,2517131,2524190,2527155,2527190,2527874,2528594,2528629,2529918,2530633,2531350,2531384,2532080,2532484,2532500,2532817,2534206,2536288,2536293,2536296,2536298,2536331,2536333,2536335,2536336,2536338,2536340,2536342,2536343,2536345,2536348,2536350,2537060,2537080,2537082,2537084,2537086,2537088,2537091,2537092,2537094,2537097,2537099,2537102,2537120,2537122,2537794,2537807,2537810,2537817,2537830,2537854,2537856,2537857,2537862,2537864,2537867,2537869,2537879,2537881,2537883,2537886,2537888,2537890
- L6-leg2-walk-off-passA.log / leg2-walk-off-passA (window 140.14 s): pid 50122 +3.68 s; pid 1097257 +1.13 s; pid 2721602 +0.95 s; pid 2722314 +0.51 s; pid 2721730 +0.28 s
- L6-leg2-walk-off-passB.log / leg2-walk-off-passB: TRANSIENT pids (present in some snapshot, not in both ends): 2513318,2537794,2537810,2537854,2537856,2537862,2537864,2537867,2537869,2537879,2537881,2537883,2537886,2537888,2537890,2539051,2539905,2539907,2539909,2539926,2539929,2539932,2539937,2539940,2539956,2539958,2539959,2539963,2539968,2540636,2540668,2540682,2540687,2540689,2540690,2540692,2540694,2540696,2540699,2540700,2540702,2540704,2540707,2541388,2541392,2541394,2541411,2541414,2541416,2541418,2541436,2541438,2541439,2541441,2541443,2541445,2541448
- L6-leg2-walk-off-passB.log / leg2-walk-off-passB (window 140.11 s): pid 50122 +4.00 s; pid 1097257 +1.19 s; pid 2721602 +0.64 s; pid 2722314 +0.54 s; pid 4022442 +0.28 s
- L6-leg2-walk-sink-passA.log / leg2-walk-sink-passA: TRANSIENT pids (present in some snapshot, not in both ends): 2537817,2537830,2537857,2541394,2541414,2541416,2541436,2541438,2541441,2541443,2541445,2541448,2542744,2542746,2542748,2542751,2542752,2543460,2543483,2543489,2543492,2543494,2543497,2543500,2543503,2543523,2543525,2543526,2543528,2543538,2544196,2544245,2544247,2544251,2544254,2544257,2544264,2544266,2544268,2544271,2544273,2544274,2544276,2544278,2544965,2544967,2544969,2544985,2544989,2544991,2544993,2544996,2544998,2544999,2545016,2545019,2545020,2545021,2545023,2545025
- L6-leg2-walk-sink-passA.log / leg2-walk-sink-passA (window 140.55 s): pid 50122 +3.81 s; pid 1097257 +1.18 s; pid 2721602 +0.62 s; pid 2722314 +0.53 s; pid 4022442 +0.27 s
- L6-leg2-walk-sink-passB.log / leg2-walk-sink-passB: TRANSIENT pids (present in some snapshot, not in both ends): 2544268,2544965,2544967,2544969,2544985,2544989,2544991,2544993,2544998,2545016,2545019,2545020,2545023,2545025,2546995,2547027,2547030,2547035,2547050,2547052,2547054,2547056,2547058,2547059,2547062,2547064,2547066,2547070,2547775,2547782,2547785,2547799,2547801,2547819,2547822,2547823,2547825,2547827,2547829,2547830,2547832,2547834,2548525,2548528,2548545,2548547,2548549,2548551,2548556,2548557,2548558,2548562,2548564,2548567,2548588
- L6-leg2-walk-sink-passB.log / leg2-walk-sink-passB (window 140.38 s): pid 50122 +3.78 s; pid 1097257 +1.13 s; pid 2721602 +0.85 s; pid 2722314 +0.54 s; pid 4022442 +0.28 s
- L7-interior-perfA.log / interior-perfA: TRANSIENT pids (present in some snapshot, not in both ends): 2579870,2579871,2579875,2583637,2584375,2584377,2584395,2584397,2584399,2584401,2584418,2584422,2584424,2584427,2584430,2584433,2586451,2586457,2586475,2586480,2586483,2586486,2586488,2586509,2586520,2586522,2586523,2586539,2586541,2586544,2587227,2587236,2587240,2587261,2587264,2587267,2587268,2587269,2587271,2587279,2587287,2587289,2587291,2587309,2587310,2587312,2587314,2587316,2588038,2588054,2588056,2588060,2588078,2588080,2588082,2588084,2588085,2588095,2588099,2588101,2588105,2588363
- L7-interior-perfA.log / interior-perfA (window 111.82 s): pid 50122 +3.25 s; pid 1097257 +1.20 s; pid 2721602 +0.63 s; pid 2722314 +0.48 s; pid 4022442 +0.32 s
- L7-interior-perfB.log / interior-perfB: TRANSIENT pids (present in some snapshot, not in both ends): 2623894,2623941,2623943,2623952,2623963,2623966,2623968,2623971,2623975,2623982,2625290,2625292,2625293,2625295,2626030,2626032,2626034,2626043,2626045,2626047,2626065,2626067,2626068,2626070,2626072,2626081,2626083,2626092,2626786,2626810,2626812,2626814,2626817,2626819,2626848,2626850,2626852,2626861,2626865,2626866,2626868,2626887,2627555,2627561,2627563,2627584,2627593,2627595,2627604,2627607,2627626,2627627,2627629,2627631,2627633,2627637
- L7-interior-perfB.log / interior-perfB (window 111.61 s): pid 50122 +3.01 s; pid 1097257 +1.21 s; pid 2721602 +0.57 s; pid 2722314 +0.49 s; pid 2721730 +0.48 s
- L7-interior-wall.log / interior-wall: TRANSIENT pids (present in some snapshot, not in both ends): 2580770,2580772,2580774,2580782,2580787,2580790,2580793,2580796,2580798,2580799,2580815,2580817,2580819,2580820,2582151,2582810,2582813,2582816,2582831,2582840,2582842,2582843,2582859,2582861,2582878,2582880,2582882,2582884,2582902,2583607,2583625,2583626,2583636,2583637,2583639,2583654,2583656,2583657,2583659,2583661,2583664,2583667,2583669,2584358,2584375,2584377,2584395,2584397,2584399,2584401,2584402,2584418,2584422,2584424,2584427,2584430,2584433
- L7-interior-wall.log / interior-wall (window 111.14 s): pid 50122 +3.11 s; pid 1097257 +1.21 s; pid 2721602 +0.61 s; pid 2722314 +0.51 s; pid 4022442 +0.30 s
- L8-interior-cells-r1.log / interior-cells-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2654518,2655296,2655298,2655485,2656626,2656628,2656630,2656645,2656647,2656648,2656650,2656652,2656671,2656673,2657363,2658066,2658082,2658205,2658298,2658770,2660164,2660194,2660870,2660886,2660901,2661577,2661595,2662295,2662297,2662299,2663004,2663020,2663036,2663038,2663040,2663717,2663719,2663736,2663752,2663754,2663755,2664431,2664433,2664449,2664458,2664788
- L8-interior-cells-r1.log / interior-cells-r1 (window 62.11 s): pid 50122 +1.77 s; pid 1097257 +0.60 s; pid 2721602 +0.28 s; pid 2722314 +0.26 s; pid 1312476 +0.18 s
- L8-interior-cells-r2.log / interior-cells-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2660194,2663036,2663038,2663040,2663717,2663736,2663752,2663754,2663755,2664431,2664433,2664449,2664458,2664788,2666474,2666491,2666860,2666880,2667179,2668588,2668884,2669305,2669578,2669979,2669995,2670012,2670692,2670708,2670731,2671443,2671691,2672119,2672120,2672136,2672151,2672821,2672823,2672835,2672837,2672838,2672854,2672856,2672872,2672881,2673572,2673588,2673590,2673592,2673594,2673611
- L8-interior-cells-r2.log / interior-cells-r2 (window 67.40 s): pid 50122 +1.85 s; pid 1097257 +0.68 s; pid 2721602 +0.36 s; pid 2722314 +0.27 s; pid 4022442 +0.17 s
- L8-interior-cells-r2.log / interior-cells-r2: BOX_PAUSE 2026-09-11T17:16:27Z tag=interior-cells-r2/f7_n10000_bound_physics try=1 -- a foreign process is in state R.
- L8-interior-cells-r2.log / interior-cells-r2: BOX_PAUSE_GIVEUP 2026-09-11T17:16:32Z tag=interior-cells-r2/f7_n10000_bound_physics -- still R after 1 pauses.
- L8-interior-cells-r3.log / interior-cells-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2623976,2623979,2623980,2641707,2644634,2644738,2645299,2645317,2645334,2645337,2645339,2645707,2646019,2646021,2646045,2646047,2646049,2646072,2648087,2648782,2648812,2648814,2649384,2649488,2649490,2649530,2650240,2650922,2650924,2651634,2651636,2651659,2651675,2652279,2653076,2653091,2653093,2653109,2653111,2653129,2653833,2653834,2654515,2654516,2654518,2654520,2654522,2654537,2654541,2654543,2654569,2654570,2654572,2654581,2655278,2655294,2655296,2655298,2655485
- L8-interior-cells-r3.log / interior-cells-r3 (window 67.38 s): pid 50122 +1.95 s; pid 1097257 +0.67 s; pid 2721602 +0.29 s; pid 2722314 +0.27 s; pid 4022442 +0.19 s
- L8-interior-cells-r3.log / interior-cells-r3: BOX_PAUSE 2026-09-11T17:13:23Z tag=interior-cells-r3/f7_n1000_bound_neutral try=1 -- a foreign process is in state R.
- L8-interior-cells-r3.log / interior-cells-r3: BOX_PAUSE_GIVEUP 2026-09-11T17:13:28Z tag=interior-cells-r3/f7_n1000_bound_neutral -- still R after 1 pauses.

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 30 | 6 | 3 | 24 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
```

## T8.9r-attrib2 -- the eleven-arm interior leg (section 11)

```
# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| D1-diff-r1.log | diff-r1 | 88.76 | 13 | 197 | 58 | 2.840 (pid 50122) | **3.1996 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 11 | **NOT met** |
| D2-diff-r2.log | diff-r2 | 88.33 | 13 | 197 | 57 | 2.750 (pid 50122) | **3.1133 %** | Rl+,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 1097257 | 0 | 0 | 0 | 11 | **NOT met** |
| D3-diff-r3.log | diff-r3 | 88.42 | 13 | 194 | 64 | 2.760 (pid 50122) | **3.1215 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 11 | **NOT met** |
| D4-diff-r4.log | diff-r4 | 88.48 | 13 | 197 | 60 | 2.490 (pid 50122) | **2.8142 %** | S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 11 | **NOT met** |
| D5-diff-r5.log | diff-r5 | 88.39 | 13 | 194 | 61 | 2.540 (pid 50122) | **2.8736 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 116643 | 1 | 1 | 0 | 11 | **NOT met** |
| PA1-perfA-r1.log | perfA-r1 | 77.96 | 13 | 197 | 53 | 2.210 (pid 50122) | **2.8348 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| PA2-perfA-r2.log | perfA-r2 | 77.68 | 13 | 194 | 58 | 2.160 (pid 50122) | **2.7806 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 0 | **NOT met** |
| PA3-perfA-r3.log | perfA-r3 | 78.08 | 13 | 197 | 50 | 2.120 (pid 50122) | **2.7152 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 0 | **NOT met** |
| PB1-perfB-r1.log | perfB-r1 | 28.78 | 6 | 197 | 27 | 0.810 (pid 50122) | **2.8145 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| PB2-perfB-r2.log | perfB-r2 | 34.20 | 6 | 197 | 36 | 0.980 (pid 50122) | **2.8655 %** | R,RN,S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314,2855082,2856614 | 1 | 1 | 0 | 0 | **NOT met** |
| PB3-perfB-r3.log | perfB-r3 | 28.42 | 6 | 201 | 26 | 0.850 (pid 50122) | **2.9909 %** | S,S+,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| W1-wall-r1.log | wall-r1 | 77.75 | 13 | 197 | 52 | 2.250 (pid 50122) | **2.8939 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| W2-wall-r2.log | wall-r2 | 82.23 | 13 | 197 | 54 | 2.380 (pid 50122) | **2.8943 %** | R,Rsl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122,50573 | 1 | 1 | 0 | 0 | **NOT met** |
| W3-wall-r3.log | wall-r3 | 78.19 | 13 | 197 | 52 | 2.100 (pid 50122) | **2.6858 %** | D,Ds,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 0 | **NOT met** |
| W4-wall-r4.log | wall-r4 | 82.90 | 13 | 197 | 55 | 2.410 (pid 50122) | **2.9071 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314 | 1 | 1 | 0 | 0 | **NOT met** |
| W5-wall-r5.log | wall-r5 | 83.20 | 13 | 194 | 59 | 2.280 (pid 50122) | **2.7404 %** | R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722044,2722106 | 1 | 1 | 0 | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| D1-diff-r1.log | diff-r1 | 22 | 82.02 | **0.560** | **0.6828 %** | **0.200** | **0.2438 %** | 0.340 | 0.110 | 73.46 | 80.40 | 0.690 | 0.400 | `R` at `diff-r1/a05-after` with NO re-snapshot; 11 alternation snapshot(s) missing | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| D2-diff-r2.log | diff-r2 | 22 | 81.55 | **0.130** | **0.1594 %** | **0.170** | **0.2085 %** | 0.000 | 0.090 | 74.08 | 80.97 | 0.070 | 0.390 | `R` at `diff-r2/a02-after` with NO re-snapshot; 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| D3-diff-r3.log | diff-r3 | 22 | 81.65 | **0.340** | **0.4164 %** | **0.340** | **0.4164 %** | 0.150 | 0.230 | 73.80 | 80.67 | 0.260 | 0.370 | 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| D4-diff-r4.log | diff-r4 | 22 | 81.65 | **0.130** | **0.1592 %** | **0.250** | **0.3062 %** | 0.000 | 0.120 | 74.28 | 81.10 | 0.080 | 0.380 | 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| D5-diff-r5.log | diff-r5 | 22 | 81.73 | **0.110** | **0.1346 %** | **0.350** | **0.4282 %** | 0.000 | 0.190 | 74.17 | 81.13 | 0.110 | 0.360 | `R` at `diff-r5/close` with NO re-snapshot; 11 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| PA1-perfA-r1.log | perfA-r1 | 11 | 71.44 | **0.080** | **0.1120 %** | **0.190** | **0.2660 %** | 0.000 | 0.110 | 64.41 | 71.01 | 0.070 | 0.320 | complete | PINNED-CLEAN |
| PA2-perfA-r2.log | perfA-r2 | 11 | 71.16 | **0.070** | **0.0984 %** | **0.230** | **0.3232 %** | 0.000 | 0.050 | 64.25 | 70.75 | 0.060 | 0.330 | `R` at `perfA-r2/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| PA3-perfA-r3.log | perfA-r3 | 11 | 71.50 | **0.070** | **0.0979 %** | **0.220** | **0.3077 %** | 0.010 | 0.070 | 64.49 | 71.09 | 0.050 | 0.320 | `R` at `perfA-r3/a04-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| PB1-perfB-r1.log | perfB-r1 | 4 | 26.06 | **0.100** | **0.3837 %** | **0.060** | **0.2302 %** | 0.060 | 0.010 | 23.46 | 25.76 | 0.090 | 0.120 | complete | PINNED-CLEAN |
| PB2-perfB-r2.log | perfB-r2 | 4 | 26.46 | **0.020** | **0.0756 %** | **0.200** | **0.7559 %** | 0.000 | 0.110 | 23.98 | 26.32 | 0.010 | 0.120 | `R` at `perfB-r2/a04-after`,`perfB-r2/a01-after` with NO re-snapshot | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| PB3-perfB-r3.log | perfB-r3 | 4 | 25.68 | **0.010** | **0.0389 %** | **0.120** | **0.4673 %** | 0.000 | 0.060 | 23.22 | 25.53 | 0.000 | 0.120 | complete | PINNED-CLEAN |
| W1-wall-r1.log | wall-r1 | 11 | 71.34 | **0.040** | **0.0561 %** | **0.220** | **0.3084 %** | 0.000 | 0.120 | 64.57 | 70.94 | 0.070 | 0.310 | complete | PINNED-CLEAN |
| W2-wall-r2.log | wall-r2 | 11 | 70.75 | **0.040** | **0.0565 %** | **0.230** | **0.3251 %** | 0.000 | 0.130 | 64.16 | 70.39 | 0.030 | 0.340 | `R` at `wall-r2/a05-after`,`wall-r2/a06-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W3-wall-r3.log | wall-r3 | 11 | 71.70 | **0.060** | **0.0837 %** | **0.230** | **0.3208 %** | 0.000 | 0.110 | 64.93 | 71.31 | 0.050 | 0.330 | `R` at `wall-r3/a06-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W4-wall-r4.log | wall-r4 | 11 | 71.36 | **0.040** | **0.0561 %** | **0.240** | **0.3363 %** | 0.000 | 0.090 | 64.62 | 71.00 | 0.030 | 0.310 | `R` at `wall-r4/a08-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W5-wall-r5.log | wall-r5 | 11 | 71.69 | **0.060** | **0.0837 %** | **0.240** | **0.3348 %** | 0.000 | 0.070 | 64.89 | 71.30 | 0.060 | 0.310 | `R` at `wall-r5/a04-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |

## The five busiest foreign processes per batch

**Every foreign pid of every batch, with every state observed for it, is `logs/idle-proof-pids-attrib2.md`** (settler R13). The five below are a summary of that file, not a substitute for it.

- D1-diff-r1.log / diff-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2777755,2777817,2777819,2777823,2777825,2777828,2777837,2777846,2777850,2777852,2777854,2777856,2777858,2778542,2778544,2778545,2778558,2779219,2779222,2779224,2779237,2779460,2779913,2779921,2779923,2780104,2780594,2780597,2781269,2781271,2781272,2781274,2781276,2781948,2781950,2781956,2781965,2782644,2782647,2782649,2782662,2782826,2783322,2783328,2783497,2783999,2784001,2784003,2784405,2784676,2784678,2784680,2784690,2785368,2785370,2785372,2785374,2785392
- D1-diff-r1.log / diff-r1 (window 88.76 s): pid 50122 +2.84 s; pid 1097257 +0.93 s; pid 2722314 +0.41 s; pid 2721602 +0.36 s; pid 4022442 +0.29 s
- D2-diff-r2.log / diff-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2783328,2786715,2786717,2786722,2786724,2786727,2786738,2786750,2786755,2786757,2786759,2786761,2786763,2787453,2787455,2787696,2788126,2788128,2788133,2788135,2788807,2788809,2788819,2788838,2789499,2789501,2789503,2789505,2790180,2790181,2790183,2790196,2790857,2790859,2790861,2790875,2791552,2791563,2791566,2791568,2792240,2792242,2792244,2792258,2792919,2792921,2792924,2792926,2793599,2793601,2793603,2793613,2793630,2794333,2794335,2794337,2794350
- D2-diff-r2.log / diff-r2 (window 88.33 s): pid 50122 +2.75 s; pid 1097257 +0.84 s; pid 2722314 +0.40 s; pid 2721602 +0.38 s; pid 4022442 +0.27 s
- D3-diff-r3.log / diff-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2777647,2777757,2777758,2792258,2792924,2792926,2793599,2793601,2793603,2793613,2793630,2794333,2794335,2794337,2794350,2795637,2796319,2796320,2796322,2796335,2796995,2796997,2797000,2797002,2797016,2797691,2797695,2797697,2797699,2798373,2798375,2798377,2798391,2799051,2799052,2799054,2799055,2799058,2799060,2799734,2799736,2799738,2799748,2799757,2800428,2800430,2800432,2800445,2801106,2801109,2801781,2801783,2801785,2801788,2802403,2802491,2802494,2802510,2803184,2803186,2803188,2803191,2803193,2803206
- D3-diff-r3.log / diff-r3 (window 88.42 s): pid 50122 +2.76 s; pid 1097257 +0.93 s; pid 2722314 +0.58 s; pid 2721602 +0.55 s; pid 4022442 +0.25 s
- D4-diff-r4.log / diff-r4: TRANSIENT pids (present in some snapshot, not in both ends): 2835622,2835629,2835761,2835796,2835802,2835804,2835808,2835816,2835838,2835839,2835843,2835848,2835859,2835861,2835863,2835864,2836555,2836557,2836559,2836562,2837244,2837246,2837266,2837267,2837280,2837950,2837952,2837954,2837956,2838638,2838641,2838642,2838655,2839325,2839329,2839331,2839345,2840030,2840032,2840033,2840035,2840716,2840718,2840720,2841401,2841402,2841431,2841433,2841435,2842139,2842142,2842144,2842163,2842834,2842836,2842838,2842852,2843513,2843515,2843519
- D4-diff-r4.log / diff-r4 (window 88.48 s): pid 50122 +2.49 s; pid 1097257 +0.96 s; pid 2721602 +0.54 s; pid 2722314 +0.36 s; pid 3819500 +0.30 s
- D5-diff-r5.log / diff-r5: TRANSIENT pids (present in some snapshot, not in both ends): 2834635,2835397,2835400,2841433,2841435,2842139,2842144,2842163,2842836,2842838,2842852,2843513,2843515,2843519,2844617,2844965,2845493,2845495,2845510,2846008,2846180,2846198,2846200,2846212,2846873,2846875,2846877,2846879,2847551,2847552,2847566,2848227,2848229,2848231,2848245,2848913,2848930,2848935,2848937,2849529,2849608,2849610,2849612,2849642,2850318,2850321,2850322,2850324,2850337,2850999,2851001,2851011,2851031,2851066,2851692,2851694,2851722,2852365,2852367,2852369,2852370
- D5-diff-r5.log / diff-r5 (window 88.39 s): pid 50122 +2.54 s; pid 1097257 +0.90 s; pid 2721602 +0.40 s; pid 2722314 +0.39 s; pid 3819500 +0.27 s
- D5-diff-r5.log / diff-r5: BOX_PAUSE 2026-09-11T18:25:35Z tag=diff-r5/close try=1 -- a foreign process is in state R.
- D5-diff-r5.log / diff-r5: BOX_PAUSE_GIVEUP 2026-09-11T18:25:40Z tag=diff-r5/close -- still R after 1 pauses.
- PA1-perfA-r1.log / perfA-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2748015,2749332,2749344,2749348,2750022,2750024,2750026,2751328,2751337,2751339,2751340,2751342,2751344,2752022,2752024,2752027,2752030,2752700,2752702,2752704,2752798,2753172,2753359,2753363,2754019,2754020,2754022,2754024,2754682,2754684,2754687,2754689,2754761,2755358,2755361,2755733,2756018,2756020,2756676,2756679,2756680,2756707,2757335,2757337,2757340,2757997,2758000,2758030,2758679,2758681,2758683,2758685,2758687
- PA1-perfA-r1.log / perfA-r1 (window 77.96 s): pid 50122 +2.21 s; pid 1097257 +0.79 s; pid 2722314 +0.35 s; pid 2721602 +0.35 s; pid 4022442 +0.21 s
- PA2-perfA-r2.log / perfA-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2735991,2738649,2738650,2756707,2757335,2757337,2757340,2757997,2758000,2758030,2758679,2758681,2758683,2758685,2758687,2759998,2760676,2760677,2760679,2760681,2761338,2761340,2761342,2761344,2761346,2762017,2762019,2762022,2762678,2762680,2763338,2763340,2763341,2763342,2763344,2763632,2763999,2764001,2764004,2764660,2764677,2764693,2765333,2765336,2765338,2765340,2765997,2765998,2766000,2766002,2766006,2766659,2766661,2766666,2766668,2767340,2767350,2767353
- PA2-perfA-r2.log / perfA-r2 (window 77.68 s): pid 50122 +2.16 s; pid 1097257 +0.79 s; pid 2722314 +0.37 s; pid 2721602 +0.29 s; pid 4022442 +0.23 s
- PA3-perfA-r3.log / perfA-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2765340,2766000,2766659,2766661,2766666,2766668,2767340,2767350,2767353,2768652,2768654,2768657,2768659,2769337,2769341,2769997,2769998,2770000,2770002,2770658,2770660,2770663,2770665,2771344,2771346,2771348,2772004,2772006,2772008,2772677,2772679,2772680,2772683,2773070,2773339,2773341,2773350,2773999,2774001,2774016,2774672,2774675,2774677,2774679,2775353,2775355,2775576,2776009,2776011,2776014
- PA3-perfA-r3.log / perfA-r3 (window 78.08 s): pid 50122 +2.12 s; pid 1097257 +0.80 s; pid 2721602 +0.62 s; pid 2722314 +0.34 s; pid 4022442 +0.23 s
- PB1-perfB-r1.log / perfB-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2822571,2822603,2822607,2822611,2822630,2822633,2822635,2822637,2822639,2822642,2822643,2822645,2823116,2823322,2823324,2823757,2823980,2824000,2824002,2824658,2824660,2824662,2824664,2824666,2825324,2825326,2825327
- PB1-perfB-r1.log / perfB-r1 (window 28.78 s): pid 50122 +0.81 s; pid 2721602 +0.27 s; pid 1097257 +0.26 s; pid 2722314 +0.13 s; pid 4022442 +0.10 s
- PB2-perfB-r2.log / perfB-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2849642,2851011,2851031,2851694,2851722,2852365,2852367,2853104,2853666,2853669,2853671,2853673,2853678,2854357,2854366,2854378,2854379,2855035,2855037,2855039,2855041,2855044,2855734,2855741,2855838,2855867,2855907,2855910,2855911,2855914,2855917,2856586,2856588,2856599,2856624,2856797
- PB2-perfB-r2.log / perfB-r2 (window 34.20 s): pid 50122 +0.98 s; pid 1097257 +0.39 s; pid 2721602 +0.26 s; pid 2722314 +0.17 s; pid 4022442 +0.09 s
- PB2-perfB-r2.log / perfB-r2: BOX_PAUSE 2026-09-11T18:25:59Z tag=perfB-r2/a04-after try=1 -- a foreign process is in state R.
- PB2-perfB-r2.log / perfB-r2: BOX_PAUSE_GIVEUP 2026-09-11T18:26:04Z tag=perfB-r2/a04-after -- still R after 1 pauses.
- PB3-perfB-r3.log / perfB-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2855035,2855039,2855044,2855838,2855910,2855911,2855914,2855917,2856586,2856588,2856599,2856624,2856797,2858599,2858601,2858781,2859268,2859270,2859939,2859940,2859942,2859945,2860083,2860609,2860612,2860621
- PB3-perfB-r3.log / perfB-r3 (window 28.42 s): pid 50122 +0.85 s; pid 1097257 +0.29 s; pid 2721602 +0.27 s; pid 2722314 +0.12 s; pid 4022442 +0.09 s
- W1-wall-r1.log / wall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2742620,2742626,2742628,2742651,2742653,2742658,2742660,2742663,2742671,2742673,2742674,2742676,2742790,2743354,2743356,2743358,2743667,2744012,2744031,2744686,2744688,2744690,2744692,2745347,2745349,2745510,2746002,2746005,2746007,2746154,2746665,2746668,2747346,2747348,2747350,2747352,2747354,2747356,2748015,2748017,2748672,2748674,2748676,2749332,2749334,2749344,2749346,2749348,2750019,2750022,2750024,2750026
- W1-wall-r1.log / wall-r1 (window 77.75 s): pid 50122 +2.25 s; pid 1097257 +0.83 s; pid 2721602 +0.61 s; pid 2722314 +0.37 s; pid 4022442 +0.24 s
- W2-wall-r2.log / wall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2707657,2707659,2707661,2707668,2707671,2707688,2707689,2707695,2707698,2707699,2707701,2707714,2708107,2708389,2708391,2708393,2709050,2709052,2709054,2709056,2709058,2709730,2709732,2709734,2710389,2710391,2711046,2711049,2711050,2711052,2711700,2711702,2711714,2711717,2711720,2712390,2712392,2712395,2712397,2712399,2712414,2713054,2713056,2713710,2713712,2713714,2714372,2714374,2714384,2714388,2714390,2715079,2715082,2715083
- W2-wall-r2.log / wall-r2 (window 82.23 s): pid 50122 +2.38 s; pid 1097257 +0.99 s; pid 2721602 +0.53 s; pid 2722314 +0.41 s; pid 4022442 +0.19 s
- W2-wall-r2.log / wall-r2: BOX_PAUSE 2026-09-11T17:50:37Z tag=wall-r2/a06-after try=1 -- a foreign process is in state R.
- W2-wall-r2.log / wall-r2: BOX_PAUSE_GIVEUP 2026-09-11T17:50:42Z tag=wall-r2/a06-after -- still R after 1 pauses.
- W3-wall-r3.log / wall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2713054,2713712,2713714,2714372,2714374,2714384,2714388,2714390,2715079,2715082,2715083,2715924,2716367,2717044,2717046,2717165,2717699,2717701,2717703,2717705,2718361,2718364,2719033,2719035,2719037,2719040,2719042,2719044,2719701,2719703,2720357,2720359,2720361,2720669,2720780,2721016,2721018,2721020,2721799,2721802,2721804,2721806,2722653,2722656,2723359,2723360,2723362,2723364,2724019,2724022,2724027,2724029
- W3-wall-r3.log / wall-r3 (window 78.19 s): pid 50122 +2.10 s; pid 1097257 +0.85 s; pid 2722314 +0.35 s; pid 2721602 +0.29 s; pid 4022442 +0.23 s
- W4-wall-r4.log / wall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 2719701,2721802,2721806,2722653,2722656,2723359,2723362,2723364,2724019,2724022,2724027,2724029,2725950,2726008,2726010,2726013,2726015,2726017,2726673,2726675,2727329,2727331,2727333,2727336,2727338,2727993,2727995,2727997,2728670,2728672,2728675,2728677,2728857,2729324,2729336,2729337,2729339,2729341,2729996,2729998,2730002,2730657,2730673,2730675,2731210,2731346,2731348,2731350,2732007,2732009,2732136,2732664,2732666,2732671,2732672
- W4-wall-r4.log / wall-r4 (window 82.90 s): pid 50122 +2.41 s; pid 1097257 +0.82 s; pid 2722314 +0.37 s; pid 2721602 +0.30 s; pid 4022442 +0.25 s
- W4-wall-r4.log / wall-r4: BOX_PAUSE 2026-09-11T17:53:26Z tag=wall-r4/a08-after try=1 -- a foreign process is in state R.
- W4-wall-r4.log / wall-r4: BOX_PAUSE_GIVEUP 2026-09-11T17:53:31Z tag=wall-r4/a08-after -- still R after 1 pauses.
- W5-wall-r5.log / wall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 2707653,2707711,2707712,2731210,2731346,2731350,2732007,2732009,2732136,2732664,2732666,2732671,2732672,2733965,2733968,2733970,2734664,2734666,2734668,2734670,2735325,2735335,2735990,2735991,2735992,2735995,2735997,2736652,2736654,2736658,2736660,2737329,2737331,2737333,2737988,2737990,2737994,2738649,2738650,2738652,2738654,2738655,2738657,2739328,2739333,2739334,2739989,2739991,2739993,2740164,2740665,2740667,2740670,2740672,2741327,2741478,2741974,2741976,2741978
- W5-wall-r5.log / wall-r5 (window 83.20 s): pid 50122 +2.28 s; pid 1097257 +0.87 s; pid 2721602 +0.54 s; pid 2722314 +0.36 s; pid 4022442 +0.24 s
- W5-wall-r5.log / wall-r5: BOX_PAUSE 2026-09-11T17:55:35Z tag=wall-r5/a04-after try=1 -- a foreign process is in state R.
- W5-wall-r5.log / wall-r5: BOX_PAUSE_GIVEUP 2026-09-11T17:55:40Z tag=wall-r5/a04-after -- still R after 1 pauses.

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 16 | 4 | 2 | 12 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
```

## T8.9r-attrib3 -- inside T8.4 (section 12)

```
# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A1-alloc-r1.log | alloc-r1 | 28.30 | 6 | 199 | 29 | 0.740 (pid 50122) | **2.6148 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| A2-alloc-r2.log | alloc-r2 | 28.91 | 6 | 202 | 25 | 0.850 (pid 50122) | **2.9402 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| A3-alloc-r3.log | alloc-r3 | 28.47 | 6 | 198 | 25 | 0.780 (pid 50122) | **2.7397 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| A4-alloc-r4.log | alloc-r4 | 32.72 | 6 | 200 | 30 | 0.960 (pid 50122) | **2.9340 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| A5-alloc-r5.log | alloc-r5 | 28.46 | 6 | 200 | 28 | 0.830 (pid 50122) | **2.9164 %** | RN,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 3064933 | 0 | 0 | 0 | 0 | **NOT met** |
| F1-wallpf-r1.log | wallpf-r1 | 14.43 | 4 | 203 | 13 | 0.390 (pid 50122) | **2.7027 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314 | 0 | 0 | 0 | 0 | **NOT met** |
| F2-wallpf-r2.log | wallpf-r2 | 14.26 | 4 | 199 | 21 | 0.400 (pid 50122) | **2.8050 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| F3-wallpf-r3.log | wallpf-r3 | 14.55 | 4 | 205 | 10 | 0.400 (pid 50122) | **2.7491 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| F4-wallpf-r4.log | wallpf-r4 | 14.28 | 4 | 202 | 15 | 0.400 (pid 50122) | **2.8011 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| F5-wallpf-r5.log | wallpf-r5 | 14.50 | 4 | 203 | 19 | 0.410 (pid 50122) | **2.8276 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| M1-mem-r1.log | mem-r1 | 21.69 | 5 | 202 | 22 | 0.620 (pid 50122) | **2.8585 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| M2-mem-r2.log | mem-r2 | 22.27 | 5 | 201 | 24 | 0.700 (pid 50122) | **3.1432 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| M3-mem-r3.log | mem-r3 | 21.38 | 5 | 204 | 18 | 0.620 (pid 50122) | **2.8999 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| M4-mem-r4.log | mem-r4 | 21.82 | 5 | 203 | 20 | 0.610 (pid 50122) | **2.7956 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| M5-mem-r5.log | mem-r5 | 22.24 | 5 | 200 | 25 | 0.620 (pid 50122) | **2.7878 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| M6-mem-r6.log | mem-r6 | 21.46 | 5 | 203 | 20 | 0.720 (pid 50122) | **3.3551 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P1-perf.log | perf | 10.91 | 4 | 206 | 12 | 0.360 (pid 50122) | **3.2997 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P2-perfstat-r1.log | perfstat-r1 | 42.84 | 8 | 199 | 35 | 1.180 (pid 50122) | **2.7544 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P2-perfstat-r2.log | perfstat-r2 | 42.27 | 8 | 199 | 33 | 1.190 (pid 50122) | **2.8152 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P2-perfstat-r3.log | perfstat-r3 | 47.39 | 8 | 200 | 35 | 1.350 (pid 50122) | **2.8487 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 1097257 | 1 | 1 | 0 | 0 | **NOT met** |
| P3-perfrec-r1.log | perfrec-r1 | 14.47 | 4 | 207 | 10 | 0.400 (pid 50122) | **2.7643 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P3-perfrec-r2.log | perfrec-r2 | 14.34 | 4 | 203 | 16 | 0.390 (pid 50122) | **2.7197 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 0 | **NOT met** |
| P3-perfrec-r3.log | perfrec-r3 | 14.44 | 4 | 207 | 10 | 0.400 (pid 50122) | **2.7701 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| S1-screen-malloc.log | screen | 26.34 | 2 | 199 | 27 | 0.710 (pid 50122) | **2.6955 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 3019998 | 0 | 0 | 0 | 3 | **NOT met** |
| W1-wall-r1.log | wall-r1 | 42.59 | 8 | 200 | 33 | 1.250 (pid 50122) | **2.9350 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| W2-wall-r2.log | wall-r2 | 42.31 | 8 | 199 | 32 | 1.150 (pid 50122) | **2.7180 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314 | 0 | 0 | 0 | 0 | **NOT met** |
| W3-wall-r3.log | wall-r3 | 42.40 | 8 | 199 | 35 | 1.130 (pid 50122) | **2.6651 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| W4-wall-r4.log | wall-r4 | 47.53 | 8 | 199 | 37 | 1.410 (pid 50122) | **2.9665 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| W5-wall-r5.log | wall-r5 | 43.42 | 8 | 200 | 34 | 1.200 (pid 50122) | **2.7637 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| X1-xwall-r1.log | xwall-r1 | 28.20 | 6 | 199 | 28 | 0.810 (pid 50122) | **2.8723 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| X2-xwall-r2.log | xwall-r2 | 33.39 | 6 | 200 | 27 | 0.940 (pid 50122) | **2.8152 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 4022442 | 1 | 1 | 0 | 0 | **NOT met** |
| X3-xwall-r3.log | xwall-r3 | 28.53 | 6 | 199 | 27 | 0.850 (pid 50122) | **2.9793 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| X4-xwall-r4.log | xwall-r4 | 28.28 | 6 | 200 | 25 | 0.820 (pid 50122) | **2.8996 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| X5-xwall-r5.log | xwall-r5 | 29.21 | 6 | 199 | 28 | 0.800 (pid 50122) | **2.7388 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| Y1-ywall-r1.log | ywall-r1 | 36.05 | 7 | 197 | 35 | 0.960 (pid 50122) | **2.6630 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314 | 0 | 0 | 0 | 0 | **NOT met** |
| Y2-ywall-r2.log | ywall-r2 | 35.38 | 7 | 198 | 38 | 0.970 (pid 50122) | **2.7417 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| Y3-ywall-r3.log | ywall-r3 | 35.53 | 7 | 200 | 29 | 0.990 (pid 50122) | **2.7864 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| Y4-ywall-r4.log | ywall-r4 | 35.25 | 7 | 199 | 32 | 0.970 (pid 50122) | **2.7518 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| Y5-ywall-r5.log | ywall-r5 | 40.40 | 7 | 199 | 34 | 1.180 (pid 50122) | **2.9208 %** | R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314,50122 | 1 | 1 | 0 | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| A1-alloc-r1.log | alloc-r1 | 4 | 25.55 | **0.030** | **0.1174 %** | **0.080** | **0.3131 %** | 0.010 | 0.040 | 23.90 | 25.42 | 0.020 | 0.100 | complete | PINNED-CLEAN |
| A2-alloc-r2.log | alloc-r2 | 4 | 26.15 | **0.020** | **0.0765 %** | **0.090** | **0.3442 %** | 0.000 | 0.060 | 24.48 | 26.00 | 0.030 | 0.120 | complete | PINNED-CLEAN |
| A3-alloc-r3.log | alloc-r3 | 4 | 25.80 | **0.020** | **0.0775 %** | **0.060** | **0.2326 %** | 0.000 | 0.020 | 24.16 | 25.66 | 0.020 | 0.120 | complete | PINNED-CLEAN |
| A4-alloc-r4.log | alloc-r4 | 4 | 24.98 | **0.030** | **0.1201 %** | **0.080** | **0.3203 %** | 0.000 | 0.050 | 23.31 | 24.84 | 0.010 | 0.110 | `R` at `alloc-r4/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| A5-alloc-r5.log | alloc-r5 | 4 | 25.70 | **0.010** | **0.0389 %** | **0.120** | **0.4669 %** | 0.000 | 0.020 | 24.01 | 25.56 | 0.030 | 0.110 | `R` at `alloc-r5/b2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| F1-wallpf-r1.log | wallpf-r1 | 2 | 12.83 | **0.000** | **0.0000 %** | **0.050** | **0.3897 %** | 0.000 | 0.030 | 11.52 | 12.76 | 0.010 | 0.060 | `R` at `wallpf-r1/h1-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| F2-wallpf-r2.log | wallpf-r2 | 2 | 12.67 | **0.010** | **0.0789 %** | **0.010** | **0.0789 %** | 0.000 | 0.010 | 11.33 | 12.61 | 0.010 | 0.060 | complete | PINNED-CLEAN |
| F3-wallpf-r3.log | wallpf-r3 | 2 | 12.96 | **0.020** | **0.1543 %** | **0.020** | **0.1543 %** | 0.000 | 0.020 | 11.60 | 12.89 | 0.010 | 0.060 | complete | PINNED-CLEAN |
| F4-wallpf-r4.log | wallpf-r4 | 2 | 12.68 | **0.020** | **0.1577 %** | **0.050** | **0.3943 %** | 0.000 | 0.040 | 11.36 | 12.61 | 0.010 | 0.060 | complete | PINNED-CLEAN |
| F5-wallpf-r5.log | wallpf-r5 | 2 | 12.87 | **0.010** | **0.0777 %** | **0.060** | **0.4662 %** | 0.000 | 0.000 | 11.56 | 12.80 | 0.010 | 0.050 | complete | PINNED-CLEAN |
| M1-mem-r1.log | mem-r1 | 3 | 19.52 | **0.010** | **0.0512 %** | **0.100** | **0.5123 %** | 0.000 | 0.020 | 17.40 | 19.39 | 0.030 | 0.090 | complete | **UNPROVEN-FRACTION** |
| M2-mem-r2.log | mem-r2 | 3 | 20.07 | **0.010** | **0.0498 %** | **0.060** | **0.2990 %** | 0.000 | 0.020 | 17.92 | 19.96 | 0.000 | 0.090 | complete | PINNED-CLEAN |
| M3-mem-r3.log | mem-r3 | 3 | 19.19 | **0.030** | **0.1563 %** | **0.080** | **0.4169 %** | 0.010 | 0.030 | 17.06 | 19.08 | 0.020 | 0.100 | complete | PINNED-CLEAN |
| M4-mem-r4.log | mem-r4 | 3 | 19.63 | **0.010** | **0.0509 %** | **0.050** | **0.2547 %** | 0.000 | 0.020 | 17.57 | 19.52 | 0.020 | 0.090 | complete | PINNED-CLEAN |
| M5-mem-r5.log | mem-r5 | 3 | 20.06 | **0.020** | **0.0997 %** | **0.080** | **0.3988 %** | 0.000 | 0.030 | 17.99 | 19.94 | 0.010 | 0.080 | complete | PINNED-CLEAN |
| M6-mem-r6.log | mem-r6 | 3 | 19.31 | **0.020** | **0.1036 %** | **0.290** | **1.5018 %** | 0.000 | 0.260 | 17.12 | 19.18 | 0.020 | 0.100 | complete | **UNPROVEN-FRACTION** |
| P1-perf.log | perf | 2 | 9.30 | **0.060** | **0.6452 %** | **0.130** | **1.3978 %** | 0.010 | 0.120 | 8.38 | 9.12 | 0.040 | 0.070 | complete | **UNPROVEN-FRACTION** |
| P2-perfstat-r1.log | perfstat-r1 | 6 | 39.02 | **0.030** | **0.0769 %** | **0.180** | **0.4613 %** | 0.000 | 0.100 | 35.40 | 38.79 | 0.030 | 0.180 | complete | PINNED-CLEAN |
| P2-perfstat-r2.log | perfstat-r2 | 6 | 38.45 | **0.040** | **0.1040 %** | **0.170** | **0.4421 %** | 0.000 | 0.070 | 34.66 | 38.22 | 0.010 | 0.180 | complete | PINNED-CLEAN |
| P2-perfstat-r3.log | perfstat-r3 | 6 | 38.56 | **0.040** | **0.1037 %** | **0.120** | **0.3112 %** | 0.000 | 0.070 | 34.80 | 38.31 | 0.020 | 0.170 | `R` at `perfstat-r3/b06-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| P3-perfrec-r1.log | perfrec-r1 | 2 | 12.84 | **0.010** | **0.0779 %** | **0.040** | **0.3115 %** | 0.000 | 0.010 | 11.53 | 12.74 | 0.010 | 0.080 | complete | PINNED-CLEAN |
| P3-perfrec-r2.log | perfrec-r2 | 2 | 12.72 | **0.010** | **0.0786 %** | **0.100** | **0.7862 %** | 0.000 | 0.040 | 11.42 | 12.61 | 0.010 | 0.080 | `R` at `perfrec-r2/open` with NO re-snapshot | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |
| P3-perfrec-r3.log | perfrec-r3 | 2 | 12.82 | **0.000** | **0.0000 %** | **0.080** | **0.6240 %** | 0.000 | 0.050 | 11.53 | 12.73 | 0.000 | 0.070 | complete | **UNPROVEN-FRACTION** |
| S1-screen-malloc.log | screen | 4 | 25.70 | **0.030** | **0.1167 %** | **0.050** | **0.1946 %** | 0.000 | 0.030 | 24.05 | 25.53 | 0.030 | 0.120 | `R` at `screen/open` with NO re-snapshot; 3 alternation snapshot(s) missing | **UNPROVEN-EVIDENCE** |
| W1-wall-r1.log | wall-r1 | 6 | 38.77 | **0.050** | **0.1290 %** | **0.260** | **0.6706 %** | 0.000 | 0.120 | 34.86 | 38.53 | 0.040 | 0.180 | complete | **UNPROVEN-FRACTION** |
| W2-wall-r2.log | wall-r2 | 6 | 38.51 | **0.030** | **0.0779 %** | **0.040** | **0.1039 %** | 0.010 | 0.020 | 34.17 | 37.97 | 0.330 | 0.200 | `R` at `wall-r2/b03-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W3-wall-r3.log | wall-r3 | 6 | 38.58 | **0.040** | **0.1037 %** | **0.150** | **0.3888 %** | 0.030 | 0.100 | 34.24 | 38.03 | 0.350 | 0.180 | complete | PINNED-CLEAN |
| W4-wall-r4.log | wall-r4 | 6 | 38.70 | **0.030** | **0.0775 %** | **0.190** | **0.4910 %** | 0.050 | 0.030 | 34.32 | 38.14 | 0.350 | 0.150 | `R` at `wall-r4/b04-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W5-wall-r5.log | wall-r5 | 6 | 39.62 | **0.110** | **0.2776 %** | **0.140** | **0.3534 %** | 0.010 | 0.070 | 35.42 | 39.04 | 0.290 | 0.200 | complete | PINNED-CLEAN |
| X1-xwall-r1.log | xwall-r1 | 4 | 25.51 | **0.030** | **0.1176 %** | **0.070** | **0.2744 %** | 0.000 | 0.010 | 22.94 | 25.37 | 0.010 | 0.130 | complete | PINNED-CLEAN |
| X2-xwall-r2.log | xwall-r2 | 4 | 25.68 | **0.030** | **0.1168 %** | **0.070** | **0.2726 %** | 0.000 | 0.030 | 23.09 | 25.53 | 0.030 | 0.130 | `R` at `xwall-r2/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| X3-xwall-r3.log | xwall-r3 | 4 | 25.85 | **0.040** | **0.1547 %** | **0.100** | **0.3868 %** | 0.000 | 0.040 | 23.28 | 25.68 | 0.030 | 0.120 | complete | PINNED-CLEAN |
| X4-xwall-r4.log | xwall-r4 | 4 | 25.59 | **0.020** | **0.0782 %** | **0.070** | **0.2735 %** | 0.000 | 0.040 | 23.01 | 25.45 | 0.030 | 0.110 | complete | PINNED-CLEAN |
| X5-xwall-r5.log | xwall-r5 | 4 | 26.46 | **0.030** | **0.1134 %** | **0.030** | **0.1134 %** | 0.000 | 0.010 | 23.86 | 26.32 | 0.020 | 0.120 | complete | PINNED-CLEAN |
| Y1-ywall-r1.log | ywall-r1 | 5 | 32.79 | **0.040** | **0.1220 %** | **0.140** | **0.4270 %** | 0.000 | 0.050 | 29.53 | 32.59 | 0.020 | 0.160 | `R` at `ywall-r1/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| Y2-ywall-r2.log | ywall-r2 | 5 | 32.13 | **0.030** | **0.0934 %** | **0.170** | **0.5291 %** | 0.000 | 0.090 | 28.91 | 31.92 | 0.040 | 0.150 | complete | **UNPROVEN-FRACTION** |
| Y3-ywall-r3.log | ywall-r3 | 5 | 32.24 | **0.020** | **0.0620 %** | **0.260** | **0.8065 %** | 0.000 | 0.090 | 29.10 | 32.05 | 0.020 | 0.160 | complete | **UNPROVEN-FRACTION** |
| Y4-ywall-r4.log | ywall-r4 | 5 | 31.97 | **0.030** | **0.0938 %** | **0.060** | **0.1877 %** | 0.000 | 0.030 | 28.73 | 31.78 | 0.020 | 0.150 | complete | PINNED-CLEAN |
| Y5-ywall-r5.log | ywall-r5 | 5 | 32.12 | **0.050** | **0.1557 %** | **0.170** | **0.5293 %** | 0.000 | 0.070 | 28.97 | 31.93 | 0.020 | 0.130 | `R` at `ywall-r5/open`,`ywall-r5/f1-after` with NO re-snapshot | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |

## The five busiest foreign processes per batch

**Every foreign pid of every batch, with every state observed for it, is `logs/idle-proof-pids-attrib3.md`** (settler R13). The five below are a summary of that file, not a substitute for it.

- A1-alloc-r1.log / alloc-r1: TRANSIENT pids (present in some snapshot, not in both ends): 3043895,3045230,3045235,3045237,3045242,3045244,3045246,3045248,3045250,3045254,3045256,3045258,3045260,3045264,3045959,3045960,3045962,3045964,3045966,3046629,3046631,3046633,3046635,3047298,3047307,3047974,3047976,3047978,3047981
- A1-alloc-r1.log / alloc-r1 (window 28.30 s): pid 50122 +0.74 s; pid 1097257 +0.32 s; pid 2721602 +0.26 s; pid 2722314 +0.14 s; pid 1861779 +0.09 s
- A2-alloc-r2.log / alloc-r2: TRANSIENT pids (present in some snapshot, not in both ends): 3045959,3045962,3045966,3046629,3046631,3046635,3047298,3047307,3047974,3047976,3047978,3049973,3049974,3049976,3049978,3049980,3049982,3050644,3051306,3051308,3051309,3051979,3051981,3051983,3051985
- A2-alloc-r2.log / alloc-r2 (window 28.91 s): pid 50122 +0.85 s; pid 1097257 +0.35 s; pid 1861779 +0.19 s; pid 2722314 +0.13 s; pid 4022442 +0.07 s
- A3-alloc-r3.log / alloc-r3: TRANSIENT pids (present in some snapshot, not in both ends): 3068044,3068046,3068048,3068052,3068067,3068069,3068071,3068073,3068075,3068091,3068093,3068095,3068766,3068767,3069423,3069424,3069426,3070080,3070098,3070100,3070102,3070104,3070106,3070732,3070767
- A3-alloc-r3.log / alloc-r3 (window 28.47 s): pid 50122 +0.78 s; pid 1097257 +0.31 s; pid 2721602 +0.26 s; pid 2722314 +0.14 s; pid 4022442 +0.08 s
- A4-alloc-r4.log / alloc-r4: TRANSIENT pids (present in some snapshot, not in both ends): 3053737,3054022,3054025,3054029,3054748,3054977,3055201,3055410,3055414,3055415,3056082,3056084,3056152,3058139,3058156,3058157,3058159,3058161,3058174,3058176,3058846,3058854,3058901,3060210,3060212,3060874,3060914,3060916,3060919,3060923
- A4-alloc-r4.log / alloc-r4 (window 32.72 s): pid 50122 +0.96 s; pid 1861779 +0.39 s; pid 1097257 +0.37 s; pid 1312476 +0.14 s; pid 2722314 +0.13 s
- A4-alloc-r4.log / alloc-r4: BOX_PAUSE 2026-09-11T19:39:17Z tag=alloc-r4/open try=1 -- a foreign process is in state R.
- A4-alloc-r4.log / alloc-r4: BOX_PAUSE_GIVEUP 2026-09-11T19:39:22Z tag=alloc-r4/open -- still R after 1 pauses.
- A5-alloc-r5.log / alloc-r5: TRANSIENT pids (present in some snapshot, not in both ends): 3055408,3058156,3058161,3058174,3058846,3058854,3058901,3060210,3060212,3060874,3060914,3060916,3060923,3062913,3062914,3062916,3062918,3062920,3063583,3063585,3063587,3063590,3063833,3064255,3064923,3064925,3064927,3064944
- A5-alloc-r5.log / alloc-r5 (window 28.46 s): pid 50122 +0.83 s; pid 1097257 +0.34 s; pid 2721602 +0.26 s; pid 1861779 +0.15 s; pid 2722314 +0.14 s
- F1-wallpf-r1.log / wallpf-r1: TRANSIENT pids (present in some snapshot, not in both ends): 3067443,3069424,3069426,3070098,3070102,3070732,3072446,3072712,3073373,3073375,3073377,3073378,3073380
- F1-wallpf-r1.log / wallpf-r1 (window 14.43 s): pid 50122 +0.39 s; pid 1097257 +0.18 s; pid 2722314 +0.06 s; pid 1861779 +0.06 s; pid 4022442 +0.05 s
- F2-wallpf-r2.log / wallpf-r2: TRANSIENT pids (present in some snapshot, not in both ends): 3067441,3067444,3068089,3070100,3070104,3070106,3070767,3072446,3072712,3073373,3074747,3075342,3075344,3075345,3075347,3075349,3075351,3075949,3076006,3076008,3076217
- F2-wallpf-r2.log / wallpf-r2 (window 14.26 s): pid 50122 +0.40 s; pid 1097257 +0.16 s; pid 3819500 +0.05 s; pid 2722314 +0.05 s; pid 1861779 +0.04 s
- F3-wallpf-r3.log / wallpf-r3: TRANSIENT pids (present in some snapshot, not in both ends): 3073375,3073377,3073380,3075342,3075351,3077959,3077960,3078616,3078618,3078620
- F3-wallpf-r3.log / wallpf-r3 (window 14.55 s): pid 50122 +0.40 s; pid 1097257 +0.17 s; pid 2722314 +0.09 s; pid 4022442 +0.04 s; pid 2722106 +0.04 s
- F4-wallpf-r4.log / wallpf-r4: TRANSIENT pids (present in some snapshot, not in both ends): 3075344,3075347,3075349,3076008,3076217,3077959,3077960,3080034,3080584,3080587,3080589,3080591,3080593,3081127,3081248
- F4-wallpf-r4.log / wallpf-r4 (window 14.28 s): pid 50122 +0.40 s; pid 2721602 +0.25 s; pid 1097257 +0.16 s; pid 4022442 +0.05 s; pid 2722314 +0.05 s
- F5-wallpf-r5.log / wallpf-r5: TRANSIENT pids (present in some snapshot, not in both ends): 3035219,3038254,3038545,3038547,3039858,3039860,3039862,3040543,3040548,3041214,3043219,3043220,3043222,3043224,3043226,3043891,3043893,3043895,3043897
- F5-wallpf-r5.log / wallpf-r5 (window 14.50 s): pid 50122 +0.41 s; pid 1097257 +0.15 s; pid 2722314 +0.06 s; pid 2722106 +0.05 s; pid 4022442 +0.04 s
- M1-mem-r1.log / mem-r1: TRANSIENT pids (present in some snapshot, not in both ends): 3009571,3009573,3009595,3009612,3009616,3009618,3009620,3009625,3009627,3009634,3009636,3010319,3010322,3010325,3010986,3011021,3011023,3011027,3011032,3011039,3011403,3011706
- M1-mem-r1.log / mem-r1 (window 21.69 s): pid 50122 +0.62 s; pid 1097257 +0.27 s; pid 1861779 +0.16 s; pid 2722314 +0.11 s; pid 1312476 +0.07 s
- M2-mem-r2.log / mem-r2: TRANSIENT pids (present in some snapshot, not in both ends): 3009623,3010322,3010325,3010986,3011021,3011023,3011027,3011032,3011039,3011403,3013190,3013342,3013680,3014341,3014343,3014345,3014347,3014350,3015012,3015083,3015084,3015086,3015089,3015091
- M2-mem-r2.log / mem-r2 (window 22.27 s): pid 50122 +0.70 s; pid 1861779 +0.21 s; pid 1097257 +0.21 s; pid 2722314 +0.10 s; pid 4022442 +0.06 s
- M3-mem-r3.log / mem-r3: TRANSIENT pids (present in some snapshot, not in both ends): 3009629,3011706,3013342,3014341,3014343,3014345,3014347,3015084,3015089,3017068,3017070,3017072,3017556,3017732,3017734,3018397,3018400,3018403
- M3-mem-r3.log / mem-r3 (window 21.38 s): pid 50122 +0.62 s; pid 1097257 +0.26 s; pid 2721602 +0.25 s; pid 2722314 +0.12 s; pid 1861779 +0.10 s
- M4-mem-r4.log / mem-r4: TRANSIENT pids (present in some snapshot, not in both ends): 3020670,3020799,3020815,3021486,3021489,3021490,3021516,3021518,3021545,3021940,3022241,3022243,3022245,3022247,3022910,3022911,3022913,3022915,3022918,3023577
- M4-mem-r4.log / mem-r4 (window 21.82 s): pid 50122 +0.61 s; pid 1097257 +0.27 s; pid 2721602 +0.26 s; pid 1861779 +0.10 s; pid 2722314 +0.09 s
- M5-mem-r5.log / mem-r5: TRANSIENT pids (present in some snapshot, not in both ends): 3021543,3021548,3021552,3021940,3022243,3022247,3022910,3022911,3022915,3022918,3023577,3024369,3024976,3025554,3025558,3025791,3026075,3026250,3026252,3026254,3026256,3026918,3026920,3026923,3026924
- M5-mem-r5.log / mem-r5 (window 22.24 s): pid 50122 +0.62 s; pid 1097257 +0.27 s; pid 1861779 +0.15 s; pid 2722314 +0.11 s; pid 4022442 +0.06 s
- M6-mem-r6.log / mem-r6: TRANSIENT pids (present in some snapshot, not in both ends): 3024976,3025554,3025558,3025791,3026075,3026252,3026254,3026256,3026924,3028797,3028897,3029105,3029556,3029558,3029562,3030236,3030237,3030239,3030241,3030243
- M6-mem-r6.log / mem-r6 (window 21.46 s): pid 50122 +0.72 s; pid 1097257 +0.24 s; pid 2722314 +0.09 s; pid 1861779 +0.09 s; pid 4022442 +0.06 s
- P1-perf.log / perf: TRANSIENT pids (present in some snapshot, not in both ends): 2894673,2894684,2894693,2894731,2897325,2897333,2900638,2900649,2900658,2900660,2901316,2901318
- P1-perf.log / perf (window 10.91 s): pid 50122 +0.36 s; pid 1097257 +0.10 s; pid 4022442 +0.04 s; pid 2722314 +0.04 s; pid 1861779 +0.04 s
- P2-perfstat-r1.log / perfstat-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2902632,2902654,2902666,2902675,2902680,2902682,2902684,2902686,2902688,2902690,2902692,2902694,2902697,2903373,2903389,2903398,2903400,2904005,2904060,2904457,2904719,2904721,2904723,2904725,2905386,2905388,2905390,2905909,2906048,2906052,2906158,2906724,2906725,2906727,2906739
- P2-perfstat-r1.log / perfstat-r1 (window 42.84 s): pid 50122 +1.18 s; pid 1097257 +0.45 s; pid 2721602 +0.34 s; pid 2722314 +0.20 s; pid 4022442 +0.14 s
- P2-perfstat-r2.log / perfstat-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2904719,2904721,2904725,2905388,2905390,2905909,2906048,2906052,2906158,2906724,2906725,2906727,2906739,2908708,2908710,2908712,2909371,2909373,2909816,2910034,2910038,2910041,2910709,2910718,2910720,2910721,2910723,2911382,2911384,2911387,2911389,2911391,2912053
- P2-perfstat-r2.log / perfstat-r2 (window 42.27 s): pid 50122 +1.19 s; pid 1097257 +0.47 s; pid 2721602 +0.27 s; pid 4022442 +0.18 s; pid 2722314 +0.17 s
- P2-perfstat-r3.log / perfstat-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2909816,2910038,2910041,2910709,2910720,2910723,2911382,2911384,2911387,2911389,2912053,2913129,2914023,2914025,2914027,2914687,2914689,2914691,2914692,2914694,2914696,2915356,2915358,2915360,2916020,2916494,2916673,2916675,2916677,2916688,2916696,2916699,2917371,2917373,2917375
- P2-perfstat-r3.log / perfstat-r3 (window 47.39 s): pid 50122 +1.35 s; pid 1097257 +0.48 s; pid 2722314 +0.21 s; pid 4022442 +0.14 s; pid 2721730 +0.13 s
- P2-perfstat-r3.log / perfstat-r3: BOX_PAUSE 2026-09-11T19:04:38Z tag=perfstat-r3/b06-after try=1 -- a foreign process is in state R.
- P2-perfstat-r3.log / perfstat-r3: BOX_PAUSE_GIVEUP 2026-09-11T19:04:43Z tag=perfstat-r3/b06-after -- still R after 1 pauses.
- P3-perfrec-r1.log / perfrec-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2976209,2976212,2976217,2976235,2976238,2976920,2977577,2977579,2977581,2977583
- P3-perfrec-r1.log / perfrec-r1 (window 14.47 s): pid 50122 +0.40 s; pid 1097257 +0.17 s; pid 2722314 +0.06 s; pid 3819500 +0.05 s; pid 1861779 +0.04 s
- P3-perfrec-r2.log / perfrec-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2976226,2976236,2976240,2976242,2976244,2976246,2976920,2977577,2978943,2979565,2979575,2979576,2979578,2979580,2980238,2980240
- P3-perfrec-r2.log / perfrec-r2 (window 14.34 s): pid 50122 +0.39 s; pid 1097257 +0.13 s; pid 2722314 +0.06 s; pid 4022442 +0.05 s; pid 2722044 +0.05 s
- P3-perfrec-r3.log / perfrec-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2977579,2977581,2977583,2979575,2979576,2982208,2982209,2982869,2982871,2982874
- P3-perfrec-r3.log / perfrec-r3 (window 14.44 s): pid 50122 +0.40 s; pid 2721602 +0.26 s; pid 1097257 +0.11 s; pid 2722314 +0.08 s; pid 2722912 +0.06 s
- S1-screen-malloc.log / screen: TRANSIENT pids (present in some snapshot, not in both ends): 3019856,3019882,3019914,3019919,3019923,3019926,3019928,3019930,3019944,3019960,3019961,3019963,3019965,3019968,3020664,3020668,3020670,3020672,3020687,3020737,3020740,3020742,3020778,3020794,3020796,3020797,3020799
- S1-screen-malloc.log / screen (window 26.34 s): pid 50122 +0.71 s; pid 2721602 +0.26 s; pid 1861779 +0.26 s; pid 1097257 +0.26 s; pid 2722314 +0.13 s
- W1-wall-r1.log / wall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2866679,2866690,2866694,2866716,2866719,2866722,2866811,2866815,2866818,2866829,2866831,2867445,2867515,2867517,2867680,2868178,2868194,2868196,2868205,2868208,2868869,2868871,2868874,2869534,2869536,2870199,2870200,2870202,2870467,2870864,2870866,2870882,2870883
- W1-wall-r1.log / wall-r1 (window 42.59 s): pid 50122 +1.25 s; pid 1097257 +0.44 s; pid 2722314 +0.20 s; pid 4022442 +0.11 s; pid 1312476 +0.11 s
- W2-wall-r2.log / wall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2866825,2868869,2868874,2869536,2870199,2870200,2870202,2870467,2870864,2870866,2870882,2870883,2872187,2872866,2872868,2872885,2873530,2873533,2873534,2874196,2874199,2874201,2874862,2874872,2874874,2874883,2874885,2875124,2875544,2876160,2876205,2876207
- W2-wall-r2.log / wall-r2 (window 42.31 s): pid 50122 +1.15 s; pid 1097257 +0.41 s; pid 2721602 +0.26 s; pid 2722314 +0.19 s; pid 4022442 +0.10 s
- W3-wall-r3.log / wall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2873533,2874199,2874201,2874862,2874872,2874874,2874883,2874885,2875544,2876160,2876205,2876207,2877797,2878185,2878188,2878190,2878192,2878854,2878864,2878866,2878883,2879543,2879545,2879726,2880206,2880210,2880212,2880215,2880883,2880885,2880887,2881548,2881551,2881554,2882109
- W3-wall-r3.log / wall-r3 (window 42.40 s): pid 50122 +1.13 s; pid 1097257 +0.44 s; pid 2721602 +0.36 s; pid 2722314 +0.21 s; pid 2721730 +0.15 s
- W4-wall-r4.log / wall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 2878883,2879545,2880206,2880210,2880212,2880215,2880883,2880885,2880887,2881548,2881551,2881554,2882109,2883547,2883549,2883550,2883552,2884207,2884209,2884211,2884222,2884224,2884886,2884888,2884891,2885251,2885554,2885556,2885572,2885573,2886234,2886237,2886239,2886903,2886905,2886908,2887159
- W4-wall-r4.log / wall-r4 (window 47.53 s): pid 50122 +1.41 s; pid 1097257 +0.50 s; pid 2721602 +0.28 s; pid 2722314 +0.19 s; pid 4022442 +0.15 s
- W4-wall-r4.log / wall-r4: BOX_PAUSE 2026-09-11T18:53:26Z tag=wall-r4/b04-after try=1 -- a foreign process is in state R.
- W4-wall-r4.log / wall-r4: BOX_PAUSE_GIVEUP 2026-09-11T18:53:31Z tag=wall-r4/b04-after -- still R after 1 pauses.
- W5-wall-r5.log / wall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 2884886,2884891,2885251,2885554,2885556,2885572,2886234,2886237,2886239,2886903,2886905,2887159,2888886,2888890,2888892,2889330,2889553,2889555,2889571,2889573,2889583,2890244,2890247,2890249,2890910,2891359,2891569,2891572,2891574,2891576,2892237,2892242,2892251,2892260
- W5-wall-r5.log / wall-r5 (window 43.42 s): pid 50122 +1.20 s; pid 1097257 +0.45 s; pid 2722314 +0.20 s; pid 4022442 +0.15 s; pid 1312476 +0.11 s
- X1-xwall-r1.log / xwall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2957339,2958666,2958669,2958672,2959981,2959997,2960000,2960002,2960009,2960011,2960013,2960015,2960482,2960699,2960702,2960704,2961366,2961382,2961391,2961393,2961394,2961725,2962056,2962339,2962716,2962718,2962721,2962723
- X1-xwall-r1.log / xwall-r1 (window 28.20 s): pid 50122 +0.81 s; pid 2721602 +0.35 s; pid 1097257 +0.29 s; pid 2721730 +0.12 s; pid 2722314 +0.11 s
- X2-xwall-r2.log / xwall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2960699,2960704,2961366,2961382,2961391,2961393,2961725,2962056,2962339,2962716,2962721,2962723,2964697,2964699,2964702,2964716,2964718,2964727,2965395,2965396,2965398,2965400,2966061,2966063,2966065,2966725,2966728
- X2-xwall-r2.log / xwall-r2 (window 33.39 s): pid 50122 +0.94 s; pid 1097257 +0.33 s; pid 2722314 +0.15 s; pid 4022442 +0.08 s; pid 1861779 +0.07 s
- X2-xwall-r2.log / xwall-r2: BOX_PAUSE 2026-09-11T19:17:08Z tag=xwall-r2/open try=1 -- a foreign process is in state R.
- X2-xwall-r2.log / xwall-r2: BOX_PAUSE_GIVEUP 2026-09-11T19:17:13Z tag=xwall-r2/open -- still R after 1 pauses.
- X3-xwall-r3.log / xwall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2962718,2964702,2964716,2964727,2965395,2965398,2965400,2966061,2966063,2966065,2966725,2966728,2968257,2968706,2968709,2968712,2969381,2969390,2969392,2969393,2969395,2970059,2970061,2970063,2970065,2970282,2970725
- X3-xwall-r3.log / xwall-r3 (window 28.53 s): pid 50122 +0.85 s; pid 1097257 +0.30 s; pid 2721602 +0.26 s; pid 2722314 +0.11 s; pid 4022442 +0.08 s
- X4-xwall-r4.log / xwall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 2968257,2968709,2968712,2969381,2969392,2969395,2970059,2970061,2970063,2970282,2970725,2972046,2972710,2972713,2972716,2973377,2973386,2973395,2973396,2973398,2973400,2974069,2974071,2974073,2974733
- X4-xwall-r4.log / xwall-r4 (window 28.28 s): pid 50122 +0.82 s; pid 1097257 +0.31 s; pid 2722314 +0.13 s; pid 1312476 +0.09 s; pid 4022442 +0.07 s
- X5-xwall-r5.log / xwall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 2949970,2952632,2952648,2953313,2953982,2953984,2953986,2954278,2954653,2954656,2954658,2954741,2955955,2956650,2956666,2956668,2956669,2956671,2957333,2957335,2957337,2957339,2957341,2958003,2958021,2958666,2958669,2958672
- X5-xwall-r5.log / xwall-r5 (window 29.21 s): pid 50122 +0.80 s; pid 1097257 +0.30 s; pid 2721602 +0.27 s; pid 2722314 +0.14 s; pid 4022442 +0.07 s
- Y1-ywall-r1.log / ywall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2984263,2984265,2984266,2985169,2985174,2985209,2985212,2985216,2985218,2985220,2985225,2985228,2985242,2985248,2985762,2985932,2985935,2986610,2986612,2986613,2986615,2986617,2986619,2987283,2987285,2987287,2987947,2987948,2987949,2987950,2987952,2988613,2988615,2988617,2988621
- Y1-ywall-r1.log / ywall-r1 (window 36.05 s): pid 50122 +0.96 s; pid 1097257 +0.42 s; pid 2722314 +0.18 s; pid 4022442 +0.13 s; pid 1861779 +0.07 s
- Y2-ywall-r2.log / ywall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2864212,2985223,2986612,2986615,2986617,2987283,2987285,2987287,2987947,2987952,2988613,2988615,2988617,2988621,2990620,2990621,2990623,2990625,2990627,2991336,2991359,2991474,2991509,2991511,2991571,2991573,2992262,2992263,2992983,2992987,2992989,2993736,2993753,2993754,2993755,2993757,2993759,2993888
- Y2-ywall-r2.log / ywall-r2 (window 35.38 s): pid 50122 +0.97 s; pid 1861779 +0.75 s; pid 1097257 +0.42 s; pid 2721602 +0.27 s; pid 2722314 +0.17 s
- Y3-ywall-r3.log / ywall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2991359,2991474,2991511,2991573,2992983,2992987,2992989,2993736,2993753,2993754,2993755,2993757,2993759,2993888,2995809,2996013,2996297,2996496,2997167,2997186,2997188,2997190,2997193,2997876,2997893,2997894,2997896,2997898,2998504
- Y3-ywall-r3.log / ywall-r3 (window 35.53 s): pid 50122 +0.99 s; pid 1861779 +0.59 s; pid 1097257 +0.43 s; pid 2722314 +0.14 s; pid 4022442 +0.10 s
- Y4-ywall-r4.log / ywall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 2991571,2996297,2997186,2997188,2997190,2997193,2997876,2997893,2997894,2997896,2997898,2998504,2999407,2999655,3000590,3000592,3000596,3000597,3001299,3001303,3001305,3001970,3001987,3001988,3002004,3002073,3002667,3002669,3003335,3003337,3003339,3003341
- Y4-ywall-r4.log / ywall-r4 (window 35.25 s): pid 50122 +0.97 s; pid 1097257 +0.43 s; pid 1861779 +0.35 s; pid 2721602 +0.27 s; pid 2722314 +0.17 s
- Y5-ywall-r5.log / ywall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 3000590,3001303,3001305,3001970,3001987,3001988,3002073,3002667,3002669,3003335,3003337,3003339,3003341,3004046,3005361,3005363,3005483,3005630,3006032,3006048,3006705,3006707,3006709,3006711,3006723,3006725,3006728,3006730,3007396,3007405,3008109,3008112,3008115,3008118
- Y5-ywall-r5.log / ywall-r5 (window 40.40 s): pid 50122 +1.18 s; pid 1097257 +0.47 s; pid 1861779 +0.42 s; pid 2721602 +0.34 s; pid 2722314 +0.19 s
- Y5-ywall-r5.log / ywall-r5: BOX_PAUSE 2026-09-11T19:28:51Z tag=ywall-r5/f1-after try=1 -- a foreign process is in state R.
- Y5-ywall-r5.log / ywall-r5: BOX_PAUSE_GIVEUP 2026-09-11T19:28:56Z tag=ywall-r5/f1-after -- still R after 1 pauses.

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 39 | 21 | 9 | 11 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
```

## T8.9r-attrib3, the SUPERSEDED argv-lock round set (retained, quoted nowhere)

```
# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| X1-xwall-r1.log | xwall-r1 | 29.04 | 6 | 199 | 29 | 0.830 (pid 50122) | **2.8581 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| X2-xwall-r2.log | xwall-r2 | 33.34 | 6 | 197 | 33 | 1.000 (pid 50122) | **2.9994 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| X3-xwall-r3.log | xwall-r3 | 33.52 | 6 | 199 | 29 | 0.920 (pid 50122) | **2.7446 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| X4-xwall-r4.log | xwall-r4 | 28.23 | 6 | 200 | 26 | 0.790 (pid 50122) | **2.7984 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| X5-xwall-r5.log | xwall-r5 | 28.57 | 6 | 199 | 28 | 0.880 (pid 50122) | **3.0802 %** | R,Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| X1-xwall-r1.log | xwall-r1 | 4 | 26.33 | **0.020** | **0.0760 %** | **0.060** | **0.2279 %** | 0.000 | 0.040 | 23.71 | 26.14 | 0.040 | 0.140 | complete | PINNED-CLEAN |
| X2-xwall-r2.log | xwall-r2 | 4 | 25.59 | **0.010** | **0.0391 %** | **0.110** | **0.4299 %** | 0.000 | 0.020 | 23.00 | 25.45 | 0.020 | 0.110 | `R` at `xwall-r2/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| X3-xwall-r3.log | xwall-r3 | 4 | 25.80 | **0.030** | **0.1163 %** | **0.080** | **0.3101 %** | 0.000 | 0.070 | 23.21 | 25.64 | 0.030 | 0.130 | `R` at `xwall-r3/e2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| X4-xwall-r4.log | xwall-r4 | 4 | 25.50 | **0.010** | **0.0392 %** | **0.100** | **0.3922 %** | 0.000 | 0.080 | 22.95 | 25.36 | 0.030 | 0.110 | complete | PINNED-CLEAN |
| X5-xwall-r5.log | xwall-r5 | 4 | 25.83 | **0.290** | **1.1227 %** | **0.070** | **0.2710 %** | 0.320 | 0.040 | 22.71 | 25.14 | 0.280 | 0.120 | `R` at `xwall-r5/e3-after`,`xwall-r5/close` with NO re-snapshot | **UNPROVEN-FRACTION** + **UNPROVEN-EVIDENCE** |

## The five busiest foreign processes per batch

**Every foreign pid of every batch, with every state observed for it, is `logs/idle-proof-pids-attrib3-superseded.md`** (settler R13). The five below are a summary of that file, not a substitute for it.

- X1-xwall-r1.log / xwall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 2919699,2919737,2919740,2919743,2919745,2919747,2919767,2919774,2919776,2919778,2919780,2919782,2919783,2920461,2920472,2920474,2920476,2921138,2921141,2921157,2921159,2921160,2921448,2921823,2921825,2922486,2922488,2922490,2922504
- X1-xwall-r1.log / xwall-r1 (window 29.04 s): pid 50122 +0.83 s; pid 1097257 +0.34 s; pid 2721602 +0.26 s; pid 2722314 +0.11 s; pid 4022442 +0.09 s
- X2-xwall-r2.log / xwall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 2918873,2918883,2918884,2920472,2920476,2921138,2921141,2921157,2921159,2921448,2921823,2921825,2922486,2922490,2922504,2924479,2924481,2924484,2924498,2924500,2924502,2925178,2925179,2925181,2925183,2925844,2925846,2925848,2925849,2926509,2926510,2926511,2926515
- X2-xwall-r2.log / xwall-r2 (window 33.34 s): pid 50122 +1.00 s; pid 1097257 +0.30 s; pid 2722314 +0.17 s; pid 4022442 +0.07 s; pid 3819500 +0.07 s
- X2-xwall-r2.log / xwall-r2: BOX_PAUSE 2026-09-11T19:11:08Z tag=xwall-r2/open try=1 -- a foreign process is in state R.
- X2-xwall-r2.log / xwall-r2: BOX_PAUSE_GIVEUP 2026-09-11T19:11:13Z tag=xwall-r2/open -- still R after 1 pauses.
- X3-xwall-r3.log / xwall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 2922488,2924484,2924498,2924502,2925178,2925183,2925844,2925846,2925848,2926511,2926515,2927611,2928314,2928495,2928498,2929160,2929176,2929178,2929179,2929181,2929844,2929846,2929848,2929850,2929853,2930517,2931143,2931172,2931175
- X3-xwall-r3.log / xwall-r3 (window 33.52 s): pid 50122 +0.92 s; pid 1097257 +0.32 s; pid 2721602 +0.27 s; pid 2722044 +0.20 s; pid 2722314 +0.13 s
- X3-xwall-r3.log / xwall-r3: BOX_PAUSE 2026-09-11T19:12:10Z tag=xwall-r3/e2-after try=1 -- a foreign process is in state R.
- X3-xwall-r3.log / xwall-r3: BOX_PAUSE_GIVEUP 2026-09-11T19:12:15Z tag=xwall-r3/e2-after -- still R after 1 pauses.
- X4-xwall-r4.log / xwall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 2929160,2929178,2929181,2929844,2929846,2929848,2929853,2930517,2931143,2931172,2931175,2932351,2932508,2932510,2932526,2932527,2932529,2932946,2933198,2933859,2933861,2933863,2934524,2934528,2934530,2934532
- X4-xwall-r4.log / xwall-r4 (window 28.23 s): pid 50122 +0.79 s; pid 1097257 +0.34 s; pid 2722044 +0.16 s; pid 2722314 +0.11 s; pid 1312476 +0.08 s
- X5-xwall-r5.log / xwall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 2929850,2932351,2932510,2932526,2932946,2933198,2933859,2933861,2933863,2934524,2934528,2934530,2934532,2936516,2936526,2936542,2936544,2936545,2937206,2937219,2937870,2937872,2937874,2937876,2938331,2938541,2938543,2938546
- X5-xwall-r5.log / xwall-r5 (window 28.57 s): pid 50122 +0.88 s; pid 2721602 +0.34 s; pid 1097257 +0.30 s; pid 2721730 +0.15 s; pid 2722314 +0.11 s
- X5-xwall-r5.log / xwall-r5: BOX_PAUSE 2026-09-11T19:13:14Z tag=xwall-r5/close try=1 -- a foreign process is in state R.
- X5-xwall-r5.log / xwall-r5: BOX_PAUSE_GIVEUP 2026-09-11T19:13:19Z tag=xwall-r5/close -- still R after 1 pauses.

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 5 | 2 | 1 | 3 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
```

## T8.9r-attrib4 -- the redraw experiment (section 13)

```
# W5 T8.9r fix1 -- R2 idle proof, per timed batch

Foreign CPU time across each batch's window, from the batch's own `PS_SNAPSHOT` blocks. A batch is PROVEN when the worst foreign CPU-time delta is under 0.5 %% of the window's wall AND no foreign process was seen in state `R`.

**FIX ROUND 3 (settler R13).** `FOREIGN_PS` is read beside `FOREIGN_TICK`, so `ps` states such as `Rsl` are seen; pauses and give-ups are separate columns; every foreign pid and its observed states is listed per batch in the appendix file; and a batch whose EVIDENCE is short of R2' -- an `R` with no re-snapshot, or a missing alternation snapshot -- is UNPROVEN-EVIDENCE, reported apart from UNPROVEN-FRACTION. **No fraction, delta or bar-verdict moved.**

## Test 1 (R2, as written) -- foreign CPU time anywhere on the box, per batch

**TRANSIENTS AND STATES ARE DISCLOSED (attrib4).** A pid present in one snapshot of a batch but not the other cannot have its CPU delta differenced; the column counts them rather than dropping them silently, and EVERY foreign state seen in any snapshot is listed, not only `R`.

| log | batch | window wall (s) | snapshots | foreign pids | transients | worst foreign cputime delta (s) | worst fraction | states seen | `R` seen | pauses | give-ups | re-snapshots | alternation gaps | R2-as-written |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| E3-perf.log | e3 | 72.47 | 2 | 195 | 34 | 1.950 (pid 50122) | **2.6908 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P1-pf-r1.log | pf-r1 | 56.17 | 2 | 198 | 28 | 1.590 (pid 50122) | **2.8307 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| P2-pf-r2.log | pf-r2 | 61.17 | 2 | 198 | 27 | 1.670 (pid 50122) | **2.7301 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| P3-pf-r3.log | pf-r3 | 56.67 | 2 | 198 | 27 | 1.530 (pid 50122) | **2.6998 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| W1-wall-r1.log | wall-r1 | 34.09 | 6 | 196 | 40 | 1.060 (pid 50122) | **3.1094 %** | R,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 1 | 1 | 0 | 0 | **NOT met** |
| W2-wall-r2.log | wall-r2 | 29.15 | 6 | 198 | 32 | 0.810 (pid 50122) | **2.7787 %** | S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | none | 0 | 0 | 0 | 0 | **NOT met** |
| W3-wall-r3.log | wall-r3 | 29.48 | 6 | 198 | 31 | 0.830 (pid 50122) | **2.8155 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 2722314 | 0 | 0 | 0 | 0 | **NOT met** |
| W4-wall-r4.log | wall-r4 | 29.44 | 6 | 199 | 30 | 0.810 (pid 50122) | **2.7514 %** | Rl,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 50122 | 0 | 0 | 0 | 0 | **NOT met** |
| W5-wall-r5.log | wall-r5 | 29.30 | 6 | 198 | 31 | 0.940 (pid 50122) | **3.2082 %** | Rl+,S,S<,S<Ls,S<Lsl,S<sl,SLl,SLsl,SN,SNl,SNl+,SNs,SNsl,Sl,Sl+,Ss,Ss+,Ssl,Ssl+ | 1097257 | 0 | 0 | 0 | 0 | **NOT met** |

## Test 2 -- the PINNED CORE, per timed run (the decisive one)

`cpu2` busy minus the run's own user+sys is the foreign CPU time that landed on the core the solve was pinned to; `cpu10` is the SMT sibling. Both are read from /proc/stat immediately before and after EACH timed run.

**Resolution.** /proc/stat counts in 10 ms ticks, so over a window of W seconds the smallest non-zero fraction the test can see is 0.01/W. A single leg-1 perf run is 0.1-0.5 s long, where one tick IS 2-10 %%, and a per-run percentage there measures the clock, not the box. The BATCH AGGREGATE -- all of a batch's foreign task time over all of its timed wall -- is the figure with enough resolution to carry the 0.5 %% bar, and it is what the verdict is taken on. The worst single run is printed beside it.

**AND THE MEASURED PROCESS IS NICED, WHICH MAKES THE TEST DIRECT.** This agent's shell runs niced, so the solve's user time lands in /proc/stat's `nice` bucket; every foreign user task on this box is UN-niced and lands in `user`. **The `user` column below is therefore foreign user-task time on the pinned core with nothing of the measurement in it and nothing subtracted.** `system` beyond the run's own `sys` is kernel-side work done FOR the measurement -- process creation, PMU programming, CSV writeback in kworker context -- and it scales with the number of processes a batch launches, not with its wall; it is reported, not counted as foreign.

**THE NICE-INCLUSIVE REPAIR (attrib4).** The two bold columns are the REPAIRED figure: `user + nice + steal + guest` on the core, with only the run's own `user` seconds subtracted on cpu2 and nothing subtracted on cpu10. The `user`-only columns beside them are the SUPERSEDED figure the earlier proof took its verdict on; they are printed so the two can be compared and are not what any verdict here rests on.

**THE VERDICT HAS TWO PARTS (fix3).** The FRACTION part is R2''s bar and is unchanged. The EVIDENCE part asks whether the log contains what R2' requires: a re-snapshot after every foreign `R`, and a snapshot between consecutive timed runs. A batch is PINNED-CLEAN only when both hold; otherwise the cell names which part failed, and both can fail at once.

| log | batch | runs | timed wall (s) | **cpu2 foreign TASK (s)** | **fraction** | **cpu10 TASK (s)** | **fraction** | cpu2 user-only (s) | cpu10 user-only (s) | cpu2 nice (s) | self user+sys (s) | cpu2 sys beyond self (s) | cpu2 irq+sirq (s) | evidence | verdict |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| E3-perf.log | e3 | 0 | - | - | - | - | - | - | - | - | - | - | - | no timed-run bracket | **UNPROVEN-EVIDENCE (no timed run bracketed)** |
| P1-pf-r1.log | pf-r1 | 0 | - | - | - | - | - | - | - | - | - | - | - | no timed-run bracket | **UNPROVEN-EVIDENCE (no timed run bracketed)** |
| P2-pf-r2.log | pf-r2 | 0 | - | - | - | - | - | - | - | - | - | - | - | `R` at `pf-r2/open` with NO re-snapshot; no timed-run bracket | **UNPROVEN-EVIDENCE (no timed run bracketed)** |
| P3-pf-r3.log | pf-r3 | 0 | - | - | - | - | - | - | - | - | - | - | - | no timed-run bracket | **UNPROVEN-EVIDENCE (no timed run bracketed)** |
| W1-wall-r1.log | wall-r1 | 4 | 26.36 | **0.010** | **0.0379 %** | **0.110** | **0.4173 %** | 0.000 | 0.020 | 23.75 | 26.21 | 0.020 | 0.130 | `R` at `wall-r1/a2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W2-wall-r2.log | wall-r2 | 4 | 26.45 | **0.020** | **0.0756 %** | **0.090** | **0.3403 %** | 0.000 | 0.050 | 23.82 | 26.30 | 0.030 | 0.130 | complete | PINNED-CLEAN |
| W3-wall-r3.log | wall-r3 | 4 | 26.73 | **0.020** | **0.0748 %** | **0.070** | **0.2619 %** | 0.000 | 0.040 | 24.11 | 26.61 | 0.000 | 0.130 | `R` at `wall-r3/open` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W4-wall-r4.log | wall-r4 | 4 | 26.71 | **0.010** | **0.0374 %** | **0.130** | **0.4867 %** | 0.000 | 0.090 | 24.06 | 26.57 | 0.020 | 0.120 | `R` at `wall-r4/a2-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |
| W5-wall-r5.log | wall-r5 | 4 | 26.55 | **0.010** | **0.0377 %** | **0.090** | **0.3390 %** | 0.000 | 0.030 | 23.88 | 26.39 | 0.020 | 0.110 | `R` at `wall-r5/a4-after` with NO re-snapshot | **UNPROVEN-EVIDENCE** |

## The five busiest foreign processes per batch

**Every foreign pid of every batch, with every state observed for it, is `logs/idle-proof-pids-attrib4.md`** (settler R13). The five below are a summary of that file, not a substitute for it.

- E3-perf.log / e3: TRANSIENT pids (present in some snapshot, not in both ends): 3116480,3116528,3116543,3121392,3121408,3121410,3121414,3121418,3121420,3121424,3121425,3121428,3121430,3121433,3121436,3121438,3121440,3122197,3122249,3122296,3122310,3122336,3122341,3122373,3122379,3122382,3122383,3122385,3122401,3122423,3122425,3122427,3122429,3122432
- E3-perf.log / e3 (window 72.47 s): pid 50122 +1.95 s; pid 1097257 +0.71 s; pid 2722314 +0.45 s; pid 2721602 +0.30 s; pid 4022442 +0.23 s
- P1-pf-r1.log / pf-r1: TRANSIENT pids (present in some snapshot, not in both ends): 3116569,3116592,3116595,3116597,3116601,3116617,3116619,3116623,3116626,3116628,3116639,3116640,3116642,3116647,3117391,3117413,3117417,3117451,3117453,3117455,3117476,3117478,3117494,3117496,3117498,3117520,3117522,3117526
- P1-pf-r1.log / pf-r1 (window 56.17 s): pid 50122 +1.59 s; pid 1097257 +0.58 s; pid 2721602 +0.28 s; pid 2722314 +0.25 s; pid 4022442 +0.16 s
- P2-pf-r2.log / pf-r2: TRANSIENT pids (present in some snapshot, not in both ends): 3117391,3117413,3117417,3117451,3117453,3117455,3117476,3117478,3117494,3117496,3117498,3117520,3117522,3117526,3118985,3118991,3119046,3119048,3119050,3119071,3119073,3119090,3119092,3119094,3119096,3119100,3119107
- P2-pf-r2.log / pf-r2 (window 61.17 s): pid 50122 +1.67 s; pid 1097257 +0.58 s; pid 2721602 +0.33 s; pid 2722314 +0.27 s; pid 4022442 +0.17 s
- P2-pf-r2.log / pf-r2: BOX_PAUSE 2026-09-11T20:34:34Z tag=pf-r2/open try=1 -- a foreign process is in state R.
- P2-pf-r2.log / pf-r2: BOX_PAUSE_GIVEUP 2026-09-11T20:34:39Z tag=pf-r2/open -- still R after 1 pauses.
- P3-pf-r3.log / pf-r3: TRANSIENT pids (present in some snapshot, not in both ends): 3118985,3118991,3119046,3119048,3119050,3119071,3119073,3119090,3119092,3119096,3119100,3119107,3119710,3120535,3120557,3120561,3120578,3120584,3120586,3120587,3120589,3120605,3120627,3120629,3120631,3120634,3120641
- P3-pf-r3.log / pf-r3 (window 56.67 s): pid 50122 +1.53 s; pid 1097257 +0.50 s; pid 2721602 +0.28 s; pid 2722314 +0.25 s; pid 4022442 +0.15 s
- W1-wall-r1.log / wall-r1: TRANSIENT pids (present in some snapshot, not in both ends): 3087234,3087268,3087279,3087791,3087793,3087795,3087803,3087805,3087807,3087812,3087815,3087824,3087826,3087828,3087891,3088184,3088527,3088529,3088531,3088533,3088535,3088536,3089198,3089208,3089210,3089256,3089878,3089880,3089900,3089902,3089904,3089906,3089913,3090590,3090592,3090593,3090594,3090610,3090612,3090614
- W1-wall-r1.log / wall-r1 (window 34.09 s): pid 50122 +1.06 s; pid 1097257 +0.36 s; pid 2721602 +0.28 s; pid 2722314 +0.14 s; pid 4022442 +0.11 s
- W1-wall-r1.log / wall-r1: BOX_PAUSE 2026-09-11T20:27:36Z tag=wall-r1/a2-after try=1 -- a foreign process is in state R.
- W1-wall-r1.log / wall-r1: BOX_PAUSE_GIVEUP 2026-09-11T20:27:41Z tag=wall-r1/a2-after -- still R after 1 pauses.
- W2-wall-r2.log / wall-r2: TRANSIENT pids (present in some snapshot, not in both ends): 3108301,3110398,3110400,3111144,3111308,3112387,3112390,3112391,3112393,3112409,3112412,3112413,3112415,3112577,3113100,3113102,3113104,3113567,3113779,3113781,3113793,3114471,3114474,3114476,3114478,3115154,3115156,3115172,3115174,3115176,3115179,3115347
- W2-wall-r2.log / wall-r2 (window 29.15 s): pid 50122 +0.81 s; pid 1097257 +0.30 s; pid 2722314 +0.13 s; pid 4022442 +0.10 s; pid 1861779 +0.08 s
- W3-wall-r3.log / wall-r3: TRANSIENT pids (present in some snapshot, not in both ends): 3092614,3093293,3093295,3093983,3093985,3093987,3094664,3094665,3094682,3094686,3094688,3094690,3095994,3096030,3096679,3096681,3096682,3096684,3097361,3097364,3097366,3097378,3098056,3098058,3098059,3098075,3098077,3098079,3098130,3098739,3098741
- W3-wall-r3.log / wall-r3 (window 29.48 s): pid 50122 +0.83 s; pid 2721602 +0.34 s; pid 1097257 +0.28 s; pid 2722314 +0.12 s; pid 4022442 +0.11 s
- W4-wall-r4.log / wall-r4: TRANSIENT pids (present in some snapshot, not in both ends): 3096679,3096681,3097364,3097366,3097378,3098056,3098058,3098059,3098075,3098079,3098130,3098739,3100089,3100738,3100740,3100742,3100743,3100746,3101424,3101428,3101430,3101432,3102093,3102094,3102111,3102113,3102115,3102117,3102794,3102796
- W4-wall-r4.log / wall-r4 (window 29.44 s): pid 50122 +0.81 s; pid 1097257 +0.27 s; pid 2722314 +0.16 s; pid 4022442 +0.10 s; pid 3819500 +0.06 s
- W5-wall-r5.log / wall-r5: TRANSIENT pids (present in some snapshot, not in both ends): 3098741,3100738,3100740,3101428,3101430,3101432,3102093,3102094,3102111,3102115,3102117,3102794,3102796,3104196,3104794,3104796,3104797,3104799,3105460,3105463,3105465,3105472,3106149,3106151,3106152,3106168,3106170,3106172,3106174,3106851,3106853
- W5-wall-r5.log / wall-r5 (window 29.30 s): pid 50122 +0.94 s; pid 1097257 +0.32 s; pid 2721602 +0.26 s; pid 2722314 +0.12 s; pid 4022442 +0.10 s

## The two kinds of UNPROVEN, counted (fix3)

| batches | PINNED-CLEAN | UNPROVEN-FRACTION | UNPROVEN-EVIDENCE |
|---:|---:|---:|---:|
| 9 | 1 | 0 | 8 |

A batch failing both is counted in both unproven columns, so the three need not sum to the first.
```
