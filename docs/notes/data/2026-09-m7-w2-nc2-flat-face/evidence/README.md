# NC2 evidence

This directory is a verbatim copy of the NC2 implementer's working evidence from
`.claude/worktrees/agent-a69f46e11796560b9/.scratch/nc2/` on the box. It belongs to the
implementer commit `ce3963b0`, cherry-picked onto `m7` as `71f54c50` (`qp(m7): W2 NC2 -- the
flat face: on a face with no curvature the ride takes its sign from the drop's own multiplier,
not from a measured slope the point's stationarity residual dominates`).

Copied, and nothing else:

- `corpus/` — the whole dump-replay result set (156 files, 992 KB): the per-tag
  `<tag>-<algebra>-<list>.txt` tables and their `.err` files, for every arm the implementer
  measured (`base`, `baseoff`, `face`, `faceoff`, `final`, and the `v*`/`w*`/`m2b*`/`a8*`
  experiment tags). These are RESULT tables, not dump inputs: no dump copy, no binary and no
  build tree was copied.
- `logs/bisect-*.log` (5 files) — the option-bisect legs on the final tree
  (`on`, `nopred`, `nodrift`, `nosaddle`, `alloff`), which are what isolate the landed rule's
  contribution from the rules that did not ship.
- `u0/` (7 files, 56 KB) — the U0 27-cell three-engine replay run on the final sources and its
  `comparison.txt`: **0 differences** on walk, ssn and ipm against the M6-close pin under the
  1e-5 residual gate, 7 within-gate cells on walk (the `f7_n1000_path_warm` /
  `f7_n800_path_warm` residual-class digits NC1 and E2 fix 6 both reported).
- `tools/analyze.py` — the classifier the implementer's §1 verdicts were produced with.
- `old36.list`, `new15.list` — the two dump lists the 51-dump corpus is the union of.
- `fail19.txt` — the 19 still-failing corpus cells on the final tree.

Every copied file was checked with `cmp` against its source: **172 files, 0 differences.** No
binaries, build directories, dump inputs or `*.cpp` snippets were copied. Total 1.1 MB.

The gate record for this round (`gates/`) is NOT here: the gates had not run when this directory
was placed. See `wrappers-gates/psym-derivation-nc2.txt` for why.
