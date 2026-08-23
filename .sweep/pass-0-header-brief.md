# Pass 0 — file header normalisation (hven)

You are working in the hven repository on branch `sweep/base`. This pass rewrites ONLY the
provenance/license header block at the top of source files. It is mechanical; do not
change anything else. No builds, no tests.

## Inputs (all in `.sweep/`)
- `header-classification.csv` — every file in scope with its class: `derived`,
  `original`, or `third-party-boost`. The classification is authoritative; do not
  reclassify. A file not in the CSV is out of scope — leave it alone.
- `header-derived.txt` — the exact header for `derived` files.
- `header-original.txt` — the exact header for `original` files.

## What to do, per file
1. Identify the existing header block: the contiguous run of `//` comment lines (and
   blank `//` lines) at the very top of the file that talks about provenance or license —
   "Originally from ASSET", "New file in Tycho", "Modified in Tycho", "carried into hven",
   `// ====` rulers, "Copyright", "License", "Source:", "Original Developer", and any
   change-history bullets attached to that block. Stop at the first line that is not part
   of that block (a blank line, `#pragma once`, `#include`, or a comment that describes the
   code rather than its provenance).
2. Replace that block with the template for the file's class, byte-exact, followed by one
   blank line. If the file has NO provenance block (most `original` files), insert the
   template + one blank line at the very top, before `#pragma once` / the first line.
3. `third-party-boost` (one file: `src/interior/utils/get_core_count.cpp`): keep the
   existing Boost copyright/license block verbatim, then a `//` blank line, then the
   `original` template's two lines, then the existing blank line.
4. A descriptive comment that follows the provenance block (e.g. a paragraph explaining
   what the header contains) is NOT part of the header — keep it untouched; the comment
   pass deals with it later.

## Hard constraints
- Every line outside the header block stays byte-identical. The reviewer runs a
  comment-stripped diff and a "first-N-lines-only" diff filter; any hunk outside the header
  fails the pass.
- Do not "improve" anything you notice. Write it in the report instead.
- Do not touch `dep/`, `notices/`, `LICENSE`, `docs/`, `cmake/`, or any file not in the CSV.

## Output
- One commit on `sweep/base`: subject `docs: normalise source file headers`, body listing
  counts per class.
- Write `.sweep/pass-0-header-report.md`: counts per class actually rewritten; every file
  where the header-block boundary was ambiguous (path + the line you stopped at + why);
  anything you noticed but did not change. Commit the report in the same commit.
