# DECLARATION-r3 (successor of DECLARATION-r2; DECLARATION.md and -r2 stay frozen)
RS-1d's gate surfaced a defect in G5's wording. Its `ctest -N` comparison said "identical", but 133 of the 3871 `ctest -N` names (value-parameterised
tests whose parameter has no printer, e.g. `Battery/SqpQnHs.ConvergesWithoutAHessianCall/16-byte object <03-00 ... 91-88 72-07 00-00 00-00>`) embed the raw bytes of an
object: a row ADDRESS inside the test executable. That is already true of the RS-0 baseline (its names carry `99-88 72-07`), so it is a pre-existing nondeterminism of
the names, not something the move introduced; the bytes move whenever the .rodata layout of that executable changes, and RS-1d's `HVEN_SQP_SOLVER_SOURCE`
re-pointing (a longer literal) moved them by 8. RS-1a..1c happened not to move them and were raw-identical.
G5' (replaces the `ctest -N` / gtest-list clause only): the sorted gtest list per executable is compared RAW (identical, as before); the sorted `ctest -N` names are
compared identical after replacing every `<hex-hex ...>` byte dump by `<bytes>`, and the NUMBER of names carrying a dump must be equal (133 = 133).
Implemented as tools/lists_compare.py and wired into legs/gate.sh. RS-1d's in-gate raw diff (rc=1) is kept in VERDICTS.txt; the normalised compare is rc=0.
Also recorded here: RS-1d's G2' exceptions name test_qp_mode_sites.cpp.o (reads HVEN_SQP_SOLVER_SOURCE, design 5.2 item 6) and test_message_sites.cpp.o (re-pointed again).
