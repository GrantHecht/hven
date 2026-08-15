# Included via TEST_INCLUDE_FILES, appended after gtest_discover_tests()'s own
# generated include for hven_sqp_tests (see tests/sqp/CMakeLists.txt), so by
# the time ctest processes this file the discovered tests already exist.
#
# Continuation.ProbeBudgetBoundsAFailingProposal takes ~780 s in Debug (R6
# report, .superpowers/sdd/2026-08-14-hven-m3-plan-revB/task-c-r6-report.md
# §4); the default ctest timeout is too small and produces a spurious
# Timeout. Raise just this one test's TIMEOUT rather than the suite default.
set_tests_properties(Continuation.ProbeBudgetBoundsAFailingProposal
    PROPERTIES TIMEOUT 1200)
