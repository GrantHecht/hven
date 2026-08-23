# M3 phase-C T1-T8 addenda -- TU-carve measurement provenance

Moved verbatim (CMake `#` comment markers removed, text otherwise unchanged)
from `src/CMakeLists.txt`, where the PCH-measurement table still points here.

M3 PHASE-C T1/T2 ADDENDUM (2026-08-17, task T9's obligation on a commit that
adds a source). Re-measured on the post-U0 unified flag set, same method,
same box (AMD Ryzen 7 5800X3D, clang 22.1.8, Release, CCACHE_DISABLE=1,
minimum of three compiles per arm, the two arms being a normal configure and
one with -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON). The absolute seconds are
NOT comparable with the table above -- that sweep predates the flag
unification -- so two TUs already on the opt-in list were re-measured in the
same pass as controls, and they reproduce their verdict:

  drivers/interior_point_solver.cpp             6.95 -> 4.32  -2.63  identical
  model/nlp_solver.cpp                          4.07 -> 1.84  -2.23  identical
    ------ the two sources this commit adds: faster, NOT byte-identical ----
  drivers/sqp_print.cpp                         4.82 -> 2.97  -1.85  differs
  core/ledger.cpp                               2.98 -> 1.53  -1.46  differs
    ------ pre-existing gap, measured in the same pass -------------------
  kkt/kkt_calls.cpp                             3.10 -> 1.55  -1.55  differs

So NEITHER new source joins the opt-in list: both clear bar 1 comfortably
and both fail bar 2, which puts them in the same "faster but not
byte-identical" group as most of the library. The phase-C plan's written
prediction was that sqp_print.cpp would qualify and that ledger.cpp might be
SLOWER with the PCH; both halves are falsified here by measurement, which is
what the plan asked for rather than the prediction being carried forward.

kkt/kkt_calls.cpp is not a source this commit adds -- it was added earlier
and never appeared in the sweep above, so the "all 18 Linux TUs" claim had
quietly become 18 of 19. Measuring it here closes that gap without changing
its (already correct, default) opted-out status. With this addendum the
Linux table covers all 21 sources.

M3 PHASE-C T3 ADDENDUM (2026-08-17). Same method, same box, same session
shape as the T1/T2 addendum: two configures differing only in
-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON, CCACHE_DISABLE=1, the TU's own compile
command replayed three times per arm, minimum reported, objects byte-compared.
The two opt-in controls were re-measured again and reproduce (6.96 -> 4.32 and
4.07 -> 1.83, both identical), so the arms are comparable with the T1/T2 rows.

    ------ the source this commit adds: faster, NOT byte-identical ---------
  drivers/sqp_options.cpp                       3.10 -> 1.60  -1.49  differs

It does NOT join the opt-in list: bar 1 is cleared comfortably, bar 2 is not.
The plan predicted this TU would resemble drivers/interior_point_solver_
settings.cpp (opt-in, identical); measured, it lands in the large
"faster but not byte-identical" group instead, exactly as T1's two did. The
measurement is recorded rather than the prediction. Linux table: 22 sources.

NOTE ON MEASURING AN OPTED-OUT TU. An opted-out source carries no PCH flags
in the normal configure, so a "with-PCH" arm read straight out of that build's
compile_commands.json would not consume the PCH and would report a delta of
zero. The number above comes from a THROWAWAY configure with this source
temporarily added to _hven_pch_sources, and the list here is unchanged.

T3 FOLLOW-UP (same session, same method and controls). The commit that adds
core/enum_names.cpp also SHRINKS drivers/sqp_print.cpp by two switches, so its
existing row above is re-measured rather than assumed to still hold:

    ------ the source this commit adds: faster, NOT byte-identical ---------
  core/enum_names.cpp                           1.41 -> 0.14  -1.27  differs
    ------ re-measured because its content shrank -------------------------
  drivers/sqp_print.cpp                         4.80 -> 2.95  -1.85  differs

enum_names.cpp does not join the opt-in list either -- same verdict, same
reason. Its row is also the clearest statement of what this PCH actually
buys: a 70-line TU whose own code is two switches costs 1.41 s to compile
without the PCH and 0.14 s with it, i.e. essentially ALL of its cost is the
shared-header parse floor. And sqp_print.cpp's row is unmoved to within the
measurement's own spread (4.82 -> 2.97 before, 4.80 -> 2.95 after) although it
lost two functions, which is the same fact from the other side. Linux table:
23 sources.

M3 PHASE-C T4 ADDENDUM (2026-08-18). Same method, same box, same two controls,
and the same throwaway-configure technique the note above describes (this
source is opted out, so the with-PCH arm has to come from a configure that
temporarily lists it). The controls reproduce their T3 readings to within a
hundredth of a second (6.98 -> 4.32 and 4.09 -> 1.84, both identical), so the
arms are comparable with every row above:

    ------ the source this commit adds: faster, NOT byte-identical ---------
  globalization/sqp/funnel.cpp                  3.00 -> 1.45  -1.55  differs

It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. That is the
FOURTH consecutive TU carved out of the SQP tree to land in the "faster but
not byte-identical" group (sqp_print.cpp, ledger.cpp, sqp_options.cpp,
enum_names.cpp before it), and the plan's per-TU predictions have now been
falsified every time they were made. The measurement is what is recorded.
Linux table: 24 sources.

M3 PHASE-C T5 ADDENDUM (2026-08-19). Same method, same box, same two controls,
same throwaway-configure technique. The controls reproduce their T4 readings
to within 0.02 s (6.96 -> 4.31 and 4.07 -> 1.84, both identical), so this row
is comparable with every row above:

    ------ the source this commit adds: faster, NOT byte-identical ---------
  drivers/sqp_driver.cpp                       10.46 -> 8.56  -1.90  differs

It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. Fifth
consecutive SQP-side TU in the "faster but not byte-identical" group.

THE ROW'S SHAPE IS NEW AND IS THE POINT. Every other source in this table
costs 1.3-7 s without the PCH, of which the ~3 s shared-header parse floor is
most or all -- which is why their with-PCH times collapse toward zero and why
their savings cluster at -1.5 to -2.7 s. sqp_driver.cpp costs 10.46 s, the
most expensive TU in the library by a factor of 1.5 over the next
(interior_point_solver.cpp at 6.96), and its saving is SMALLER in absolute
terms than either control's. The reason is arithmetic: ~2 330 lines of driver
body is real work no precompiled header can amortize, so only the floor part
of its cost is removable. Reading the two facts together -- the largest TU in
the tree, and the smallest PCH leverage per second of compile -- is the
honest summary of what carving the major loop out costs the build, and
T5's P-BUILD (task-c-t5-report.md) measures the whole-tree consequence.
Linux table: 25 sources.

M3 PHASE-C T6 ADDENDUM (2026-08-19). Same method, same box, same two
controls, same throwaway-configure technique. The controls reproduce their T5
readings to within 0.07 s (7.03 -> 4.36 and 4.10 -> 1.84, both identical), so
this row is comparable with every row above:

    ------ the source this commit adds: faster, NOT byte-identical ---------
  globalization/sqp/soc_elastic_restoration.cpp 5.58 ->  3.70  -1.87  differs

It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. SIXTH
consecutive SQP-side TU in the "faster but not byte-identical" group. Six for
six is no longer a run of coincidences, and it is worth naming the mechanism
the earlier notes already identified: these TUs include a different header
set in a different order from src/hven_pch.h's, so prefixing the shared block
changes template instantiation and therefore emission order. Every SQP-side
carve inherits that property from the headers it must include, so the
expectation for the remaining carves is the same -- stated as an expectation
to be measured, not a prediction to be assumed.

The row's shape is unremarkable next to T5's: 5.58 s puts it third in the
table behind sqp_driver.cpp (10.46) and interior_point_solver.cpp (7.03), and
its -1.87 s saving sits in the same -1.5 to -2.7 s band as everything else.
That is what a TU costs when most of its cost IS the shared-header parse
floor: ~370 moved lines is little enough work that the floor dominates, which
is exactly the contrast T5's note draws.
Linux table: 26 sources.

M3 PHASE-C T7 ADDENDUM (2026-08-19). Same method, same box, same two
controls, same throwaway-configure technique. The controls reproduce their T6
readings to within 0.08 s (6.95 -> 4.32 and 4.08 -> 1.84, both identical), so
this row is comparable with every row above:

    ------ the source this commit adds: faster, NOT byte-identical ---------
  warmstart/warm_start.cpp                      3.25 ->  1.75  -1.50  differs

It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. Seventh
consecutive SQP-side TU in that group, and the expectation the T6 addendum
stated is now measured rather than assumed for one more row. At 3.25 s this
is the SMALLEST of the six SQP-side carves -- about the shared-header parse
floor plus a hundred lines of ingest -- which is why its absolute saving is
the smallest and why its RATIO (-46 %) is nevertheless the largest of them.
Linux table: 27 sources.

M3 PHASE-C T8 ADDENDUM (2026-08-19), closing the T-series. Same method, same
box, same two controls, same throwaway-configure technique. The controls
reproduce their T7 readings to within 0.01 s (6.96 -> 4.33 and 4.08 -> 1.84,
both identical), so this row is comparable with every row above:

    ------ the source this commit adds: faster, NOT byte-identical ---------
  warmstart/continuation.cpp                    6.23 ->  4.35  -1.87  differs

It does NOT join the opt-in list: bar 1 cleared, bar 2 failed. EIGHTH
consecutive SQP-side TU in that group, and the T-series is now closed with a
clean sweep: every one of the seven TUs T1-T8 added to this target compiles
faster with the PCH and none produces a byte-identical object. The mechanism
was named in the T6 addendum and has not needed revising -- these TUs include
a different header set in a different order from src/hven_pch.h's, so
prefixing the shared block changes template instantiation and therefore
emission order. Anyone proposing to admit them is proposing to relax bar 2,
which docs/build.md is explicit is a numerics-governance decision and not a
build one. Eight measurements is now the evidence for that conversation.
Linux table: 28 sources.
