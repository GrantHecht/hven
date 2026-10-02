# M7 W2 NC3 (the saddle exit) — the task record

> Base `a522f115` (its sources equal P1's gated head `1f46e34d`). **Run 1** head **`054e4c78`**
> (fixtures `1b644a62` + mechanism `054e4c78`, cherry-picks of worktree `bd3155e7` / `170ce2c9`),
> declaration **`a4390c4e`**, gates **GREEN** (`gates/`). This is the first measurement of P1 and NC3
> built together. The Fable review (`REVIEW-fable.md`) returned **FIX-ROUND 1**, one blocking item:
> the anti-return rule must also cover probe-exempt and point-memory-released members, because a
> degenerate saddle vertex could otherwise be certified. Fix round 1 gets a SUCCESSOR declaration
> (`-r2`) on `a522f115..<fix1>`, recorded beside this one. Settler rulings C5–C6 are at the end of
> `docs/notes/data/2026-10-m7-w2-p1-schur-cost/REPORT-c10.md` and in the round's `.superpowers` copy.

`REPORT.md` is the implementer's report and `REVIEW-fable.md` the review, both placed by the
settler from hand-back text.

## Gates, run 1 (chain pid 465951, 2026-10-02, `gates/chain-nc3.txt`)

`GATES_SUMMARY G1=0 G2=0 G3=0 G4=0 G5=0 G6=0`, fold proofs **46 / 0**.

| gate | result |
|---|---|
| G1 Debug suite | 2903 / 2903 (= P1's 2893 + 10 by name; skipped set = P1's by name) |
| G2 long test | 1 / 1 |
| G3 Release suite | 2904 / 2904 |
| G4 U0 replay vs the M6 pin | the declared **43**-difference set; exact-identical to P1's gate replay (0 differences, 75 columns × 3 engines) |
| G5 P-SYM | DIFFERING **39** = class A 4 + class B 35 both configs, UNEXPLAINED **0** |
| G6 | install smoke, export contract, clang-format, `git diff --check` |
