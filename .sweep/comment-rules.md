# Comment rules (apply to every comment-sweep chunk)

A comment survives only if it is one of:
1. **A docstring** on a declaration (class, function, member, enum, template, namespace-level
   constant) in tycho-style Doxygen: `/// @brief …` then `@tparam`, `@param`, `@return`,
   `@throws` as applicable. Short; states what, contract, and preconditions. Existing prose
   that describes a declaration is rewritten into this form — do not delete the information,
   reshape it. Add a docstring where a public or non-obvious declaration has none.
2. **A concise descriptor** (one or two lines) of a bit of code that is obscure or confusing
   without it — an index trick, a non-obvious ordering, a numerical guard.
3. **An important-fact callout**: an invariant, an ownership/lifetime rule, thread-safety,
   a numerical caveat, a deliberate deviation a reader would otherwise "fix".

Everything else goes: history ("extracted from …", "formerly …", "used to be …"), design
essays and rationale narratives, alternative-considered lists, comparisons to other
codebases, plan/task/review/milestone labels (E2, G8, Task 6, fb3, spec §, dossier, M4 …),
commented-out code, rulers (`// ====`), restated obvious code ("increment i"). Rationale that
is genuinely worth keeping is NOT rewritten into the file — list it in the report under
"rationale candidates for docs/notes" with file:line and a one-line summary; the controller
moves it.

Hard constraints
- Zero code-token change. The gate is a comment-stripped diff of before vs after: it must
  be empty. Whitespace inside code lines, includes, macros, string literals — untouched.
  Removing a whole-line comment may remove its line; do not otherwise reflow code.
- Provenance headers (set by pass 0) are untouched.
- No builds, no tests. One commit per chunk, subject `docs: comment sweep — <chunk dir>`.
- Report file `.sweep/chunk-N-report.md` in the same commit: files touched with
  before/after comment-line counts; rationale candidates; anything that looked like a
  defect in the code (report, do not fix); any docstring you wrote where you were unsure
  of the behaviour (say so — the reviewer checks those first).
