#!/usr/bin/env python3
# B8 (C226 F8): max_restoration_entries -- one name, one floor on both engines (>= 0; 0 refuses
# restoration). The SQP refused 0; for every value it accepted the entry rule is unchanged.
import os
os.chdir('/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4')


def edit(p, old, new):
    s = open(p).read()
    assert s.count(old) == 1, (p, s.count(old), old[:70])
    open(p, 'w').write(s.replace(old, new))


edit('src/drivers/sqp_solver.cpp',
     '''    const bool entry_admissible =
        st.resto.entries == 0 || (st.resto.entries < opts_.advanced.max_restoration_entries &&
                                  st.out.counters.steps_accepted > st.resto.accepted_at_resume);''',
     '''    const bool entry_admissible =
        st.resto.entries < opts_.advanced.max_restoration_entries &&
        (st.resto.entries == 0 || st.out.counters.steps_accepted > st.resto.accepted_at_resume);''')
edit('src/drivers/sqp_options.cpp',
     '''    if (!(opts.advanced.max_restoration_entries >= 1)) {
        throw std::invalid_argument(fmt::format("SqpSolver: advanced.max_restoration_entries ({}) "
                                                "must be >= 1 (1 = one restoration phase "
                                                "per solve)",
                                                opts.advanced.max_restoration_entries));
    }''',
     '''    if (!(opts.advanced.max_restoration_entries >= 0)) {
        throw std::invalid_argument(fmt::format("SqpSolver: advanced.max_restoration_entries ({}) "
                                                "must be >= 0 (0 refuses restoration)",
                                                opts.advanced.max_restoration_entries));
    }''')
edit('include/hven/drivers/sqp_options.h',
     '''    /// The number of restoration phases one solve may run. Default 10; must be
    /// >= 1.
    ///
    /// 1 is the once-per-solve rule''',
     '''    /// The number of restoration phases one solve may run. Default 10; must be
    /// >= 0; 0 refuses restoration: the first request ends the solve
    /// kNumericalError, as a refused later request does.
    ///
    /// 1 is the once-per-solve rule''')
edit('include/hven/drivers/sqp_options.h', 'a `max_restoration_entries` below 1, a',
     'a `max_restoration_entries` below 0, a')
edit('tests/sqp/test_sqp_restoration.cpp',
     '''TEST(RestorationEntriesOption, RefusesFewerThanOnePhaseByNameAndValue) {
    SqpOptions o;
    for (const Index bad : {Index{0}, Index{-1}}) {''',
     '''TEST(RestorationEntriesOption, RefusesANegativeCountByNameAndValue) {
    SqpOptions o;
    for (const Index bad : {Index{-1}, Index{-7}}) {''')
edit('tests/sqp/test_sqp_restoration.cpp',
     '''            EXPECT_NE(std::string(e.what()).find("restoration_max_entries"), std::string::npos)
                << e.what();
            EXPECT_NE(std::string(e.what()).find(fmt::format("({})", bad)), std::string::npos)
                << e.what();
        }
    }
    o.advanced.max_restoration_entries = 1;
    EXPECT_NO_THROW(SqpSolver{o});''',
     '''            EXPECT_NE(std::string(e.what()).find("max_restoration_entries"), std::string::npos)
                << e.what();
            EXPECT_NE(std::string(e.what()).find(fmt::format("({})", bad)), std::string::npos)
                << e.what();
        }
    }
    o.advanced.max_restoration_entries = 0; // the IPM's floor: 0 refuses restoration
    EXPECT_NO_THROW(SqpSolver{o});
    o.advanced.max_restoration_entries = 1;
    EXPECT_NO_THROW(SqpSolver{o});''')
# the pin: a cap of 0 refuses the FIRST request, by the same exit a refused later request takes
edit('tests/sqp/test_sqp_restoration.cpp',
     '''TEST(RestorationEntriesOption, BothFieldsCarryADumpLineInDeclarationPosition) {''',
     '''TEST(SqpDriverRestorationEntries, ACapOfZeroRefusesTheFirstRequest) {
    RunawayValleyModel model;
    SqpOptions opts;
    opts.trust_region_initial = 8.0;
    opts.common.max_iterations = 200;
    opts.advanced.max_restoration_entries = 0;
    SqpSolver driver(opts);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
    EXPECT_EQ(sol.counters.restoration_entries, 0);
    EXPECT_EQ(sol.counters.restoration_iters, 0);
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_EQ(driver.last_stop_reason(), SqpStopReason::kRestorationRefused);
}

TEST(RestorationEntriesOption, BothFieldsCarryADumpLineInDeclarationPosition) {''')
print('ok')
