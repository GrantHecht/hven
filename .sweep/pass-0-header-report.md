# Pass 0 — file header normalisation report

Branch `sweep/base` at `d2a732a` ("chore: sweep workspace — header pass inputs").
Executor: O12 Mac-session agent, 2026-08-22. No builds, no tests (per brief).

## Counts rewritten

| class              | in CSV | rewritten |
|--------------------|--------|-----------|
| `derived`          | 35     | 35        |
| `original`         | 239    | 234       |
| `third-party-boost`| 1      | 1         |
| **total**          | 275    | **270**   |

Verification: for every rewritten file the post-header bytes were compared
against `HEAD:<path>` programmatically — 270/270 identical outside the header
region (insert case: template + one blank line prepended; replace case: old
block replaced by class template + exactly one blank line, immediately
following blank line(s) consumed so exactly one remains).

## NOT rewritten — classification conflict (4 files, needs owner adjudication)

The CSV classifies these as `original`, but their top-of-file blocks are
**third-party license notices**, not hven provenance banners:

- `include/hven/detail/linear/pardiso_session.h`
- `include/hven/detail/linear/pardiso_session.cpp`
  — MPL-2.0 notice (Eigen PardisoSupport) plus Intel BSD-3-Clause notice,
  reproduced in full by upstream requirement.
- `include/hven/detail/linear/accelerate_session.h`
- `src/linear/accelerate_session.cpp`
  — MPL-2.0 notice (Eigen AccelerateSupport), same structure.

Replacing those blocks with the Apache-2.0 `original` template would strip
MPL-2.0/BSD-3-Clause attribution from derived files. Repository governance
(CLAUDE.md §6: a derived file stays a clean diff against its upstream;
`notices/` protection embodies the same principle) binds the executor
verbatim, so the brief's "classification is authoritative" instruction cannot
lawfully extend this far without an owner ruling. Headers left byte-identical;
no other byte of these four files was touched. Recommend either reclassifying
them out of scope permanently or ruling a license-noticing amendment to the
template set.

## Missing file (1)

- `tests/sqp/support/claim_stream_double.h` — listed in the CSV, absent at
  `d2a732a`. Not created (out of scope).

## Boundary interpretations used (declared, mechanical, applied uniformly)

1. A wrapped continuation line (`//` + ≥2 spaces + text) directly following a
   change-history bullet joins the block even though it carries no keyword
   (e.g. `interior_point_solver_fwd.h`'s `//   psiopt.h`,
   `barrier_math.h`'s wrapped bullets).
2. After a replaced block, immediately-following blank lines were consumed so
   the template is followed by exactly ONE blank line (brief: "followed by
   one blank line"); all other bytes untouched.
3. A provenance block lacking a closing ruler stops at the first descriptive
   comment (e.g. `super_scalar_traits.h`: two banner lines then description).

## Ambiguous boundaries

None remaining after rule 1–3; every boundary was decided mechanically and
the full-tree audit above confirms nothing outside header regions moved.

## Noticed but not changed

- `super_scalar_traits.h`'s old block had no closing ruler (see 3 above).
- `tests/golden_rig/seam_psiopt.cpp` / `seam_sqp.cpp` carry descriptive
  (not provenance) opening comments naming the origin projects — sanctioned
  by CLAUDE.md §1's test-seam exception; they were preserved verbatim under
  the inserted template.
- `src/linear/symmetric_factor_accelerate.cpp`'s descriptive opening mentions
  its MPL-derived siblings in prose; it sits below the inserted header and is
  unchanged.
