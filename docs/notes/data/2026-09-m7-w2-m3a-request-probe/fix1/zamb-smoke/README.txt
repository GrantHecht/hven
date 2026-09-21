M7 W2 M3a fix1 (R5) -- THE UNREGISTERED ZAMB2-8 INSTRUMENT SMOKE, COMMITTED AS IT
WAS FOUND.

WHAT THESE SIX FILES ARE. A hand-run `--ledger on` / `--ledger off` A/B on
ZAMB2-8, run once while the addendum-F probe was being built, to see whether the
new argument worked at all. Commit bb29174f's message DISCLOSED it and quoted its
numbers ("FREE PRE-CHECK, run by hand on a ZAMB2-8 smoke before this commit").
The review's finding was that the artifact ALSO said "NO LEG BEYOND THE FIVE was
ever started", that the two statements cannot both be read literally, and that
the smoke carries no lock/PGREP/foreground transcript. Both corrections are made
in README.md and PROVENANCE.txt; these are the files themselves.

THEY ARE COPIED BYTE FOR BYTE, WITH THEIR MTIMES, from
.scratch/m7w2m3a/out/{f-smoke-on,f-smoke-off}.{txt,keys,stderr}. NOTHING HERE IS
RECONSTRUCTED. The two .stderr files are empty and are committed empty, because
that is what they are; the .keys files are the sorted key lists the A/B was
compared on.

  f-smoke-on.txt     the `--ledger on` arm, 136 lines, PROBE_BEGIN..PROBE_END
                     plus the LEDGER block
  f-smoke-off.txt    the `--ledger off` arm, 67 lines, no LEDGER block
  f-smoke-*.keys     the key lists (61 lines each)
  f-smoke-*.stderr   empty, both arms

WHAT THEY ARE NOT. Not a registered leg: no RULE covers them, no pre-registered
predicate reads them, and NO NUMBER IN ANY VERDICT, TABLE OR THRESHOLD COMES FROM
THEM. They are an instrument smoke. There is no lock hold, no PGREP marker and no
FOREGROUND_START/END line for them anywhere, because none was taken -- the box
protocol's transcript is what a registered leg produces, and this was not run as
one. That is the disclosure, not an excuse.

THE ORDER, FROM WHAT EXISTS RATHER THAN FROM MEMORY. Shell history is not
evidence and is not cited. What exists is filesystem mtimes on the six files
above, bracketed by two git commit timestamps:

  229d63de  RULE-F.md committed                    2026-09-21 08:27:43 -0400
  f-smoke-on.stderr   created                      2026-09-21 08:32:10.413 -0400
  f-smoke-on.txt      written (the `on` arm ends)  2026-09-21 08:32:28.118 -0400
  f-smoke-off.stderr  created                      2026-09-21 08:32:28.120 -0400
  f-smoke-off.txt     written (the `off` arm ends) 2026-09-21 08:32:45.793 -0400
  f-smoke-{on,off}.keys written                    2026-09-21 08:32:51.91  -0400
  bb29174f  the commit that disclosed it           2026-09-21 08:33:28 -0400
  CHAINF_START, the first F leg (logs/chainF.log)  2026-09-21 08:33:44 -0400

So RULE-F.md was committed roughly four and a half minutes BEFORE the smoke ran,
and the smoke ran between that commit and the first F leg. RULE-F's thresholds
therefore cannot have been informed by these numbers -- they were fixed before
the numbers existed.

THE CAVEAT ON THAT EVIDENCE, STATED RATHER THAN GLOSSED. An mtime is filesystem
metadata: it can be rewritten, and it was read at fix1 time rather than recorded
at run time. What makes it worth quoting is that it is bracketed on both sides by
git commit timestamps that agree with it, and that the two arms' mtimes are
consistent with one sequential A/B of about forty seconds. It is the best
evidence that exists. If a reader will not accept an mtime, then the honest
statement is the weaker one: the ORDER CANNOT BE SHOWN from a transcript, because
no transcript was taken.
