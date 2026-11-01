# NC2 fix round 1 — evidence

Copied from the fix-round implementer's working evidence at
`.claude/worktrees/agent-a69f46e11796560b9/.scratch/nc2-fix1/` on the box. It belongs to the
implementer commit `7b36a01e`, cherry-picked onto `m7` as `0fa59cdc` (`qp(m7): W2 NC2 fix round 1
-- a ride never ascends the slope the arithmetic can read`). The round's report is `../REPORT-fix1.md`
and the review that opened it is `../REVIEW-fable.md`.

Copied, and nothing else:

- `zero-length-rides.md` — the M17 measurement the round turned on: which capped Cs solves are
  dominated by zero-length rides and which are not, and the finding that in Release qp1/qp5 and
  Debug qp3 **half the rides ASCEND** (1064/2154, 1072/2167, 1061/2156), which is what reversed
  NC2's precedence.
- `probes/` (83 files) — the walk-seam probe output, six arms: `dbg-base`, `rel-base` (NC2's
  behaviour), `fix1-dbg`, `fix1-rel` (the landed rule), `rho`, `rho-rel` (the ρ decomposition).
  The 42 `.out` files are the gtest transcripts, kept plain. The 41 per-pass `.csv` traces are
  kept **gzipped** (`*.csv.gz`): they are 17.1 MB raw and 720 KB compressed, they are read by
  tooling rather than by eye, and `zcat`/`zgrep` read them in place with no unpacking. Every one
  was verified byte-identical to its source through `zcat | cmp` (41/41, 0 differences). The 16
  `.seq` raw sequence files (7.1 MB) were NOT copied — nothing in the report or the declaration
  cites them.
- `logs/` (25 files, 1.7 MB) — every leg log of the round, including `ctest-dbg.log` and
  `ctest-rel.log` (the two suite transcripts this declaration's §3 derives its accounting check
  from), `ctest-dbg-long.log` (the > 600 s test alone: Passed, 562.04 s), `rho6-rel.log` (the
  descent invariant on the whole Cs family: `DESCENT_INVARIANT ascending_rides=0`, 12 cells),
  `u0.log`, and the build/configure/install logs.
- `corpus/` (22 files) — the dump-replay result tables for the `base`, `baseoff`, `fix1`,
  `fix1off` and `final` arms, both algebras, both dump lists. Result tables only: no dump inputs,
  no binaries, no build trees.
- `tools/rowcmp.py` — the row comparator the corpus movers were established with.

Fidelity: **132 files, 0 differences** — 91 plain files by `cmp`, 41 gzipped by `zcat | cmp`.
Total 2.6 MB. No binaries, build directories, dump inputs, install trees or `*.cpp` snippets.

The NC2 (pre-fix) evidence is `../evidence/`, copied from the same worktree's `.scratch/nc2/`.
