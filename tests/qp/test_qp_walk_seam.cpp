// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_walk_seam.cpp — the primal active-set walk's READ-ONLY per-minor
// observer (hven/detail/qp/qp_walk_trace.h), on the test-seam convention
// docs/testing.md describes.
//
// WHAT THIS FILE HAS TO PROVE, and it is not "the observer records something":
//
//   (1) NEUTRALITY. With the observer attached, the engine's answer and its
//       counters are BIT-IDENTICAL to the unattached run. That is the whole
//       licence for using this instrument to take a measurement whose identity
//       gate is a committed counter row, so it is asserted per fixture, field
//       by field AND over the whole `QpCounters` object, and over the returned
//       x / multipliers / bound states / status as exact bit patterns.
//   (2) FAITHFULNESS of each recorded quantity against the loop's own public
//       counters: one record per minor, `degenerate` summing to
//       `degenerate_steps`, additions summing to `ws_adds`, drops to
//       `ws_drops`, bound additions to `ws_adds_bound`, `ratio_tied > 1`
//       summing to `ratio_ties`, `drop_tied > 1` to `drop_ties`, and the
//       working-set signature's transitions agreeing with the actions.
//   (3) The SIGNATURE is what it claims: order-independent, side-sensitive and
//       identity-sensitive.
//
// This target defines HVEN_TESTING target-wide and does NOT link hven::hven —
// see tests/CMakeLists.txt.

#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Action = hven::solvers::detail::testing::QpWalkAction;
using Block = hven::solvers::detail::testing::QpWalkBlock;
using Route = hven::solvers::detail::testing::QpWalkDropRoute;
using Record = hven::solvers::detail::testing::QpWalkMinorRecord;
using EqpPath = hven::solvers::detail::testing::QpWalkEqpPath;
using InertiaState = hven::solvers::detail::testing::QpWalkInertiaState;
using RideOutcome = hven::solvers::detail::testing::QpWalkRideOutcome;

namespace {

// --- The three fixtures -------------------------------------------------
//
// Transcribed from tests/sqp/test_qp_engine.cpp's battery (that file is not
// included: a test TU never includes another test TU) so that the degenerate
// one is the SAME degenerate problem the ordinary QP suite pins.

/// min 1/2(x0^2+x1^2) - x0 - 2x1 s.t. x0+x1 <= 1, 0 <= x <= 10. The optimum
/// (0,1) is a DEGENERATE vertex: the row is active with lambda = 1 and the
/// bound x0 >= 0 is active with a ZERO multiplier, which is exactly what §4b's
/// zero-multiplier probe is written for.
QpProblem simple_box_qp() {
    QpProblem qp;
    const Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

/// simple_box_qp with the inequality row DUPLICATED: at the optimum the two
/// identical rows are both active, so the working set is linearly DEPENDENT —
/// the case `qp_engine.h`'s "Degeneracy" note is about.
QpProblem degenerate_qp() {
    QpProblem qp = simple_box_qp();
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, 1.0);
    return qp;
}

/// A larger box-and-rows QP whose walk takes many minors: a deterministic
/// tridiagonal Hessian, a linear term that pulls the solution outside the box,
/// and two general rows. It exists so neutrality is not proved only on
/// two-variable problems.
///
/// PARAMETERIZED IN `kN` (fix round 2) and in nothing else: every coefficient
/// is the same function of `i` it always was, and the two row right-hand sides
/// are the same constants, so `many_minor_qp_n(24)` is the 24-variable problem
/// this file has always built, coefficient for coefficient. The width is a
/// parameter because the concurrency fixtures below need solves LONG enough for
/// two threads released together to actually overlap inside `QpEngine::run`,
/// and a 24-variable walk is not reliably that.
QpProblem many_minor_qp_n(Index kN) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(kN, kN);
    for (Index i = 0; i < kN; ++i) {
        Hd(i, i) = 2.0 + 0.01 * static_cast<double>(i);
        if (i + 1 < kN) {
            Hd(i, i + 1) = -0.5;
            Hd(i + 1, i) = -0.5;
        }
    }
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(kN);
    for (Index i = 0; i < kN; ++i) {
        qp.g(i) = -3.0 - 0.25 * static_cast<double>((i * 7) % 11);
    }
    qp.Ae.resize(0, kN);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(2, kN);
    for (Index i = 0; i < kN; ++i) {
        Aid(0, i) = 1.0;
        Aid(1, i) = (i % 2 == 0) ? 1.0 : -1.0;
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 6.0, 2.0;
    qp.lower = Vec::Zero(kN);
    qp.upper = Vec::Constant(kN, 4.0);
    return qp;
}

QpProblem many_minor_qp() { return many_minor_qp_n(24); }

/// The two problems the CONCURRENCY fixtures run, wide enough that a solve is
/// milliseconds rather than microseconds and the overlap window the start gate
/// opens is a real one. Their widths differ, which is what makes "this thread's
/// own trace" checkable by constraint index alone.
QpProblem concurrent_wide_qp() { return many_minor_qp_n(96); }
QpProblem concurrent_narrow_qp() { return many_minor_qp_n(48); }

/// A working set that is WRONG on purpose: every variable pinned at its lower
/// bound, no row active. On these fixtures the optimum has neither property,
/// so the walk has to DROP its way out — which is how the drop paths of this
/// seam are reached at all. Without it every assertion about a drop would pass
/// vacuously, because a cold walk on these problems only ever adds.
QpSolution all_at_lower_seed(const QpProblem &qp) {
    QpSolution seed;
    seed.x = qp.lower;
    seed.bound_state.assign(static_cast<std::size_t>(qp.n()), BoundState::kAtLower);
    seed.ineq_active.assign(static_cast<std::size_t>(qp.mi()), false);
    return seed;
}

struct Case {
    const char *name;
    QpProblem (*build)();
    bool seeded;
};

const Case kCases[] = {
    {"simple_box", &simple_box_qp, false},
    {"degenerate", &degenerate_qp, false},
    {"many_minor", &many_minor_qp, false},
    {"simple_box@wrong-seed", &simple_box_qp, true},
    {"degenerate@wrong-seed", &degenerate_qp, true},
    {"many_minor@wrong-seed", &many_minor_qp, true},
};

/// The CONCURRENCY fixtures' own two cases, kept OUT of `kCases` so that the
/// neutrality, faithfulness and signature fixtures keep running exactly the six
/// cases they have always run. Both are wrong-seeded, so both walks DROP.
const Case kConcurrentWide{"concurrent_wide@wrong-seed", &concurrent_wide_qp, true};
const Case kConcurrentNarrow{"concurrent_narrow@wrong-seed", &concurrent_narrow_qp, true};

QpSolution run_case(const Case &c, const QpProblem &qp, QpEngine &engine) {
    if (!c.seeded) {
        return engine.solve(qp);
    }
    const QpSolution seed = all_at_lower_seed(qp);
    return engine.solve(qp, seed);
}

/// Bit equality, so a NaN compares equal to itself and a signed zero does not
/// compare equal to its negative. `EXPECT_DOUBLE_EQ` would tolerate a
/// difference this gate exists to refuse.
::testing::AssertionResult same_bits(const Vec &a, const Vec &b, const char *what) {
    if (a.size() != b.size()) {
        return ::testing::AssertionFailure()
               << what << ": sizes " << a.size() << " vs " << b.size();
    }
    for (Index i = 0; i < a.size(); ++i) {
        const double x = a(i);
        const double y = b(i);
        if (std::memcmp(&x, &y, sizeof(double)) != 0) {
            return ::testing::AssertionFailure()
                   << what << ": element " << i << " differs (" << x << " vs " << y << ")";
        }
    }
    return ::testing::AssertionSuccess();
}

/// WHICH BYTES OF `QpCounters` NO FIELD OWNS, determined at runtime rather than
/// assumed. Two storage buffers are poisoned with different bytes and a
/// DEFAULT-initialized (not value-initialized) object is constructed in each,
/// which writes every field's own initializer and leaves the padding holding
/// the poison. The offsets that still differ are exactly the padding.
///
/// WHY IT IS NEEDED. The whole-object `memcmp` arm below is the one that would
/// catch a counter the named list forgets, and it is worth keeping — but
/// padding bytes are not part of a counter's VALUE, are not required to be
/// carried by memberwise assignment, and are not required to agree between two
/// objects built on different stacks. The concurrency fixtures compare a
/// thread's result against a main-thread baseline and so compare exactly such a
/// pair; without this mask the arm compares stack residue, which is a flake
/// rather than a gate (it fires at the seven bytes after `k0_reused`).
///
/// IF THE MASK COMES OUT EMPTY — a toolchain that zero-fills a
/// default-initialized object — `memcmp_ignoring_padding` is arithmetically the
/// strict byte-for-byte comparison, so nothing is weakened. But the suite does
/// NOT then quietly run in that mode: the non-vacuity fixture requires
/// `padded > 0` and FAILS, because on such a toolchain the mask's derivation has
/// stopped being a measurement of padding and the whole-object arm's behaviour
/// would no longer be the one argued for here. Refusing is deliberate; an
/// earlier wording of this comment said "degrades", which contradicted that
/// fixture (fix round 2).
const std::vector<bool> &counters_padding_mask() {
    static const std::vector<bool> mask = [] {
        alignas(QpCounters) unsigned char lo[sizeof(QpCounters)];
        alignas(QpCounters) unsigned char hi[sizeof(QpCounters)];
        std::memset(lo, 0x00, sizeof(lo));
        std::memset(hi, 0xFF, sizeof(hi));
        new (static_cast<void *>(lo)) QpCounters;
        new (static_cast<void *>(hi)) QpCounters;
        std::vector<bool> m(sizeof(QpCounters), false);
        for (std::size_t i = 0; i < sizeof(QpCounters); ++i) {
            m[i] = lo[i] != hi[i];
        }
        return m;
    }();
    return mask;
}

/// `std::memcmp` over everything the mask above does not call padding.
int memcmp_ignoring_padding(const QpCounters &a, const QpCounters &b) {
    const std::vector<bool> &pad = counters_padding_mask();
    const auto *pa = reinterpret_cast<const unsigned char *>(&a);
    const auto *pb = reinterpret_cast<const unsigned char *>(&b);
    for (std::size_t i = 0; i < sizeof(QpCounters); ++i) {
        if (!pad[i] && pa[i] != pb[i]) {
            return 1;
        }
    }
    return 0;
}

void expect_same_counters(const QpCounters &a, const QpCounters &b) {
    // Named first, so a failure says WHICH quantity moved.
    EXPECT_EQ(a.factorizations, b.factorizations);
    EXPECT_EQ(a.schur_updates, b.schur_updates);
    EXPECT_EQ(a.minor_iters, b.minor_iters);
    EXPECT_EQ(a.eqp_refine_steps, b.eqp_refine_steps);
    EXPECT_EQ(a.border_refine_steps, b.border_refine_steps);
    EXPECT_EQ(a.verdict_refine_steps, b.verdict_refine_steps);
    EXPECT_EQ(a.suspect_escalations, b.suspect_escalations);
    EXPECT_EQ(a.k0_reused, b.k0_reused);
    EXPECT_EQ(a.symbolic_analyses, b.symbolic_analyses);
    EXPECT_EQ(a.ws_adds, b.ws_adds);
    EXPECT_EQ(a.ws_drops, b.ws_drops);
    EXPECT_EQ(a.shift_adds, b.shift_adds);
    EXPECT_EQ(a.degenerate_steps, b.degenerate_steps);
    EXPECT_EQ(a.degenerate_run_max, b.degenerate_run_max);
    EXPECT_EQ(a.ws_adds_bound, b.ws_adds_bound);
    EXPECT_EQ(a.ws_drops_bound, b.ws_drops_bound);
    EXPECT_EQ(a.distinct_ineq_added, b.distinct_ineq_added);
    EXPECT_EQ(a.distinct_bound_added, b.distinct_bound_added);
    EXPECT_EQ(a.drop_ties, b.drop_ties);
    EXPECT_EQ(a.ratio_ties, b.ratio_ties);
    // Then the WHOLE object, so a field this list forgets cannot slip through.
    // QpCounters is trivially copyable and both objects are copies of
    // value-initialized locals, so their padding is equal too.
    static_assert(std::is_trivially_copyable_v<QpCounters>);
    if (memcmp_ignoring_padding(a, b) != 0) {
        // Say WHERE. A whole-object failure with every named field equal is
        // either a field the list forgot or a padding byte, and the two are
        // diagnosed very differently — the byte offsets are what separates them.
        const auto *pa = reinterpret_cast<const unsigned char *>(&a);
        const auto *pb = reinterpret_cast<const unsigned char *>(&b);
        const std::vector<bool> &pad = counters_padding_mask();
        std::string offsets;
        for (std::size_t i = 0; i < sizeof(QpCounters); ++i) {
            if (!pad[i] && pa[i] != pb[i]) {
                offsets += " " + std::to_string(i);
            }
        }
        ADD_FAILURE() << "QpCounters differ at byte offsets:" << offsets;
    }
}

/// A scope guard, so a fixture that fails an assertion cannot leave the
/// observer armed for the next one.
struct ObserverGuard {
    ObserverGuard() { Obs::reset(); }
    ~ObserverGuard() { Obs::reset(); }
};

/// Everything one thread's solve produced, COPIED OUT of the thread-local
/// observer before that thread exits. This copy is the whole point: with
/// `thread_local` state, `Obs::records` belongs to the thread that filled it
/// and is gone when the thread ends, so a fixture that means to read another
/// thread's capture has to be handed it, not reach for it.
struct ThreadResult {
    QpSolution sol{};
    /// THE COUNTERS, `memcpy`-ed rather than assigned. `expect_same_counters`
    /// closes with a whole-object `memcmp`, which is what would catch a field
    /// the named list forgets — and that arm compares PADDING bytes too. An
    /// assignment through `sol` is memberwise and does not reliably carry
    /// padding (it demonstrably does not, in Release, at the seven bytes after
    /// `k0_reused`), so the object the engine returned is copied byte for byte
    /// instead. That keeps the memcmp arm comparing what it exists to compare
    /// rather than weakening it to dodge the problem.
    QpCounters counters{};
    std::vector<Record> records;
    Index solves = 0;
    Index solve_index = -1;
    bool truncated = false;
    bool allocation_failed = false;
    bool active_seen_at_entry = false;
};

/// A TWO-PARTY START GATE. Hand-rolled from a mutex and a condition variable
/// rather than taken from `<barrier>`, so the fixture depends on nothing beyond
/// the threading primitives this file already uses.
///
/// WHY IT IS HERE (the review's Important 2). Without it the two workers are
/// merely CREATED close together, and a valid scheduler may run them one after
/// the other: each pre-fix worker could then reset, solve, copy out and reset
/// again before the other started, and every assertion below would pass on the
/// broken observer. The gate makes both threads block until both have built
/// their problem and constructed their engine, and releases them together, so
/// the thing under test — two `QpEngine::run` calls IN FLIGHT AT ONCE — is what
/// actually runs.
///
/// WHAT IT STILL CANNOT PROMISE, said plainly rather than left to be inferred.
/// The defect this fixture is written against is a DATA RACE on process-global
/// observer state. A data race is undefined behaviour, and no ordinary
/// assertion can promise a particular symptom from one: on the PRE-fix observer
/// these fixtures are probabilistically red, not deterministically red. The
/// gate plus solves sized in milliseconds makes overlap the overwhelmingly
/// likely case rather than an accident of scheduling; a sanitizer verdict is
/// what would make it a certainty, and THERE IS NO ThreadSanitizer PRESET IN
/// THIS TREE (`CMakePresets.json` carries linux-clang-{release,debug,coverage},
/// macos-clang-release and windows-clang-release, and nothing under `cmake/`
/// adds `-fsanitize=thread`). Adding one, and running this suite and the
/// concurrent-solve suites under it, is REGISTERED for the milestone close in
/// `docs/testing.md`; it is deliberately not done in this round.
/// `QpWalkSeam.ArmingOnOneThreadIsInvisibleOnAnother` below is the
/// DETERMINISTIC arm of the same claim and does not depend on any of this.
class StartGate {
  public:
    void arrive_and_wait() {
        std::unique_lock<std::mutex> lk(m_);
        if (++arrived_ == 2) {
            open_ = true;
            cv_.notify_all();
            return;
        }
        cv_.wait(lk, [this] { return open_; });
    }

  private:
    std::mutex m_;
    std::condition_variable cv_;
    int arrived_ = 0;
    bool open_ = false;
};

/// The body each thread of the concurrency fixtures runs: arm (or do not arm)
/// THIS thread's observer, build its own problem, construct its engine, wait at
/// the start gate for the other thread, and only then solve and copy out.
void run_one_thread(const Case &c, bool arm, ThreadResult &out, StartGate &gate) {
    // What this thread saw BEFORE it armed anything. With process-global state
    // the other thread's arming would be visible here.
    out.active_seen_at_entry = Obs::active;
    Obs::reset();
    Obs::active = arm;
    const QpProblem qp = c.build();
    QpEngine engine{QpOptions{}};
    // EVERYTHING EXPENSIVE THAT IS NOT THE SOLVE HAPPENS ABOVE THIS LINE, so
    // what the gate releases into is `QpEngine::run` and not problem assembly.
    gate.arrive_and_wait();
    const QpSolution sol = run_case(c, qp, engine);
    out.sol = sol;
    std::memcpy(&out.counters, &sol.counters, sizeof(QpCounters));
    out.records = Obs::records;
    out.solves = Obs::solves;
    out.solve_index = Obs::solve_index;
    out.truncated = Obs::truncated;
    out.allocation_failed = Obs::allocation_failed;
    Obs::reset();
}

/// The unobserved, single-threaded answer for one case: the baseline each
/// thread's own output is compared against bit for bit.
QpSolution baseline_for(const Case &c) {
    Obs::reset();
    const QpProblem qp = c.build();
    QpEngine engine{QpOptions{}};
    return run_case(c, qp, engine);
}

/// A capture is COMPLETE when it was neither truncated nor short of memory:
/// the "one record per pass" correspondence is asserted only under those two
/// qualifications (see qp_walk_trace.h's contract).
void expect_complete_own_capture(const ThreadResult &r, Index n_of_its_own_problem) {
    EXPECT_FALSE(r.truncated);
    EXPECT_FALSE(r.allocation_failed);
    ASSERT_FALSE(r.records.empty());
    EXPECT_EQ(static_cast<std::size_t>(r.counters.minor_iters), r.records.size())
        << "a complete capture holds exactly one record per pass entered";
    EXPECT_EQ(0, r.solve_index);
    for (const Record &rec : r.records) {
        EXPECT_EQ(0, rec.solve) << "a record from another thread's solve leaked in";
        EXPECT_NE(Action::kUnset, rec.action);
        // The discriminator that makes "its OWN trace" checkable rather than
        // merely plausible: the two threads run problems of DIFFERENT widths,
        // so a record carrying the other thread's constraint index is out of
        // range here.
        EXPECT_LT(rec.block_index, n_of_its_own_problem);
        EXPECT_LE(rec.ws_size_in, n_of_its_own_problem + 4);
    }
}

} // namespace

// --- (1) NEUTRALITY -----------------------------------------------------

TEST(QpWalkSeam, AttachingTheObserverChangesNoCounterAndNoAnswer) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();

        Obs::reset();
        QpEngine unobserved{QpOptions{}};
        const QpSolution without = run_case(c, qp, unobserved);
        ASSERT_FALSE(Obs::active);
        EXPECT_TRUE(Obs::records.empty()) << "an inactive observer recorded something";

        ObserverGuard guard;
        Obs::active = true;
        QpEngine observed{QpOptions{}};
        const QpSolution with = run_case(c, qp, observed);

        EXPECT_EQ(static_cast<int>(without.status), static_cast<int>(with.status));
        expect_same_counters(without.counters, with.counters);
        EXPECT_TRUE(same_bits(without.x, with.x, "x"));
        EXPECT_TRUE(same_bits(without.lambda_e, with.lambda_e, "lambda_e"));
        EXPECT_TRUE(same_bits(without.lambda_i, with.lambda_i, "lambda_i"));
        EXPECT_TRUE(same_bits(without.z, with.z, "z"));
        EXPECT_EQ(without.bound_state, with.bound_state);
        EXPECT_EQ(without.ineq_active, with.ineq_active);
        EXPECT_EQ(without.tr_active, with.tr_active);
        EXPECT_FALSE(Obs::records.empty()) << "an ACTIVE observer recorded nothing";
    }
}

TEST(QpWalkSeam, TheObserverIsNeutralOnAWarmRESOLVETOO) {
    // The neutrality that matters for a driver measurement is neutrality on
    // the SECOND solve of the same engine as well as the first: a warm
    // re-solve reuses border state, and an observer that perturbed that state
    // would show up here and nowhere else.
    const QpProblem qp = many_minor_qp();

    Obs::reset();
    QpEngine a{QpOptions{}};
    const QpSolution first_without = a.solve(qp);
    const QpSolution second_without = a.solve(qp, first_without);

    ObserverGuard guard;
    Obs::active = true;
    QpEngine b{QpOptions{}};
    const QpSolution first_with = b.solve(qp);
    const QpSolution second_with = b.solve(qp, first_with);

    expect_same_counters(first_without.counters, first_with.counters);
    expect_same_counters(second_without.counters, second_with.counters);
    // EVERY field of the second solve, not only x: a border state the observer
    // perturbed could move a multiplier or a membership without moving x.
    EXPECT_EQ(static_cast<int>(second_without.status), static_cast<int>(second_with.status));
    EXPECT_TRUE(same_bits(second_without.x, second_with.x, "x"));
    EXPECT_TRUE(same_bits(second_without.lambda_e, second_with.lambda_e, "lambda_e"));
    EXPECT_TRUE(same_bits(second_without.lambda_i, second_with.lambda_i, "lambda_i"));
    EXPECT_TRUE(same_bits(second_without.z, second_with.z, "z"));
    EXPECT_EQ(second_without.bound_state, second_with.bound_state);
    EXPECT_EQ(second_without.ineq_active, second_with.ineq_active);
    EXPECT_EQ(second_without.tr_active, second_with.tr_active);
    EXPECT_EQ(2, Obs::solves);
}

// --- (1b) NEUTRALITY UNDER CONCURRENCY ----------------------------------
//
// The review's Critical finding: with process-global observer state, two
// overlapping instrumented solves race on the arming flag, the solve counter,
// the record buffer and the current-record pointer. A data race is undefined
// behaviour, so the answer to "can a hook alter a CONCURRENT solve?" was yes —
// not because any hook writes a solver quantity, but because the observer's own
// state was shared. These two fixtures run the case the finding describes.
//
// THERE IS NO ThreadSanitizer PRESET IN THIS TREE (`CMakePresets.json` carries
// linux-clang-{release,debug,coverage}, macos-clang-release and
// windows-clang-release, and nothing in cmake/ adds `-fsanitize=thread`), so
// these run under the ordinary Debug and Release configurations and assert the
// OBSERVABLE consequences of separation rather than a sanitizer verdict: each
// thread's answer is bit-identical to its own single-threaded unobserved run,
// each thread's solve COUNTER is its own (a shared one would read 2), and each
// armed thread's trace is complete and contains only its own records.
//
// FIX ROUND 2 forces the overlap they are about. Both threads now block at a
// `StartGate` until both have built their problem and their engine, and they
// run problems of 96 and 48 variables rather than 24 and 2, so the solve the
// gate releases them into lasts long enough for the two `QpEngine::run` calls
// to be in flight together. The honest limit, restated because it is easy to
// read past: the PRE-fix defect is a data race, redness under a data race is
// probabilistic without a sanitizer, and this tree has no TSan preset. These
// two fixtures make the racing case HAPPEN; the third one below is the
// deterministic arm and needs no scheduling luck at all.

TEST(QpWalkSeam, TwoEnginesOnTwoThreadsOneArmedOneNot) {
    // Deliberately different widths (96 variables against 48) so a record from
    // the wrong thread is detectable by its constraint index alone, and both
    // seeded wrong so both walks actually DROP.
    const Case &wide = kConcurrentWide;
    const Case &narrow = kConcurrentNarrow;

    ObserverGuard guard;
    const QpSolution wide_base = baseline_for(wide);
    const QpSolution narrow_base = baseline_for(narrow);

    ThreadResult wide_out;
    ThreadResult narrow_out;
    StartGate gate;
    std::thread a([&] { run_one_thread(wide, /*arm=*/true, wide_out, gate); });
    std::thread b([&] { run_one_thread(narrow, /*arm=*/false, narrow_out, gate); });
    a.join();
    b.join();

    // Neither thread saw the other's arming.
    EXPECT_FALSE(wide_out.active_seen_at_entry);
    EXPECT_FALSE(narrow_out.active_seen_at_entry);

    // Each thread's solve counter is ITS OWN. Shared state would make one of
    // these 2 — this is the assertion that fails on the pre-fix observer.
    EXPECT_EQ(1, wide_out.solves);
    EXPECT_EQ(1, narrow_out.solves);

    // Each thread's answer equals its own unobserved, single-threaded run.
    EXPECT_EQ(static_cast<int>(wide_base.status), static_cast<int>(wide_out.sol.status));
    expect_same_counters(wide_base.counters, wide_out.counters);
    EXPECT_TRUE(same_bits(wide_base.x, wide_out.sol.x, "wide x"));
    EXPECT_TRUE(same_bits(wide_base.lambda_i, wide_out.sol.lambda_i, "wide lambda_i"));
    EXPECT_TRUE(same_bits(wide_base.z, wide_out.sol.z, "wide z"));
    EXPECT_EQ(wide_base.bound_state, wide_out.sol.bound_state);
    EXPECT_EQ(wide_base.ineq_active, wide_out.sol.ineq_active);

    EXPECT_EQ(static_cast<int>(narrow_base.status), static_cast<int>(narrow_out.sol.status));
    expect_same_counters(narrow_base.counters, narrow_out.counters);
    EXPECT_TRUE(same_bits(narrow_base.x, narrow_out.sol.x, "narrow x"));
    EXPECT_TRUE(same_bits(narrow_base.lambda_i, narrow_out.sol.lambda_i, "narrow lambda_i"));
    EXPECT_TRUE(same_bits(narrow_base.z, narrow_out.sol.z, "narrow z"));
    EXPECT_EQ(narrow_base.bound_state, narrow_out.sol.bound_state);
    EXPECT_EQ(narrow_base.ineq_active, narrow_out.sol.ineq_active);

    // The armed thread's trace is complete and entirely its own; the unarmed
    // thread recorded nothing at all.
    expect_complete_own_capture(wide_out, 96);
    EXPECT_TRUE(narrow_out.records.empty()) << "an unarmed thread recorded something";
}

TEST(QpWalkSeam, TwoEnginesOnTwoThreadsBothArmed) {
    const Case &wide = kConcurrentWide;
    const Case &narrow = kConcurrentNarrow;

    ObserverGuard guard;
    const QpSolution wide_base = baseline_for(wide);
    const QpSolution narrow_base = baseline_for(narrow);

    ThreadResult wide_out;
    ThreadResult narrow_out;
    StartGate gate;
    std::thread a([&] { run_one_thread(wide, /*arm=*/true, wide_out, gate); });
    std::thread b([&] { run_one_thread(narrow, /*arm=*/true, narrow_out, gate); });
    a.join();
    b.join();

    EXPECT_EQ(1, wide_out.solves);
    EXPECT_EQ(1, narrow_out.solves);

    expect_same_counters(wide_base.counters, wide_out.counters);
    EXPECT_TRUE(same_bits(wide_base.x, wide_out.sol.x, "wide x"));
    EXPECT_TRUE(same_bits(wide_base.z, wide_out.sol.z, "wide z"));
    EXPECT_EQ(wide_base.bound_state, wide_out.sol.bound_state);

    expect_same_counters(narrow_base.counters, narrow_out.counters);
    EXPECT_TRUE(same_bits(narrow_base.x, narrow_out.sol.x, "narrow x"));
    EXPECT_TRUE(same_bits(narrow_base.z, narrow_out.sol.z, "narrow z"));
    EXPECT_EQ(narrow_base.bound_state, narrow_out.sol.bound_state);

    // BOTH traces are complete and neither holds a record of the other's walk.
    expect_complete_own_capture(wide_out, 96);
    expect_complete_own_capture(narrow_out, 48);
    EXPECT_NE(wide_out.records.size(), narrow_out.records.size())
        << "the two walks were chosen to differ in length; equal sizes would "
           "make the separation claim untestable";
}

TEST(QpWalkSeam, ArmingOnOneThreadIsInvisibleOnAnother) {
    // The property under the two fixtures above, asserted directly and without
    // a solve: every piece of observer state has THREAD storage duration.
    ObserverGuard guard;
    Obs::active = true;
    Obs::capture_solve = 7;
    Obs::max_records = 11;

    bool other_active = true;
    Index other_capture = -99;
    std::size_t other_max = 0;
    std::size_t other_records = 99;
    std::thread t([&] {
        other_active = Obs::active;
        other_capture = Obs::capture_solve;
        other_max = Obs::max_records;
        other_records = Obs::records.size();
    });
    t.join();

    EXPECT_FALSE(other_active);
    EXPECT_EQ(-1, other_capture);
    EXPECT_EQ(1000000u, other_max);
    EXPECT_EQ(0u, other_records);

    // And this thread's own state is exactly as it was left.
    EXPECT_TRUE(Obs::active);
    EXPECT_EQ(7, Obs::capture_solve);
    EXPECT_EQ(11u, Obs::max_records);
}

TEST(QpWalkSeam, CaptureSolveSelectsOneSolveAndTheRestAreNotRecorded) {
    const QpProblem qp = many_minor_qp();
    ObserverGuard guard;
    Obs::active = true;
    Obs::capture_solve = 1; // the SECOND solve only

    QpEngine engine{QpOptions{}};
    const QpSolution first = engine.solve(qp);
    engine.solve(qp, first);

    EXPECT_EQ(2, Obs::solves);
    ASSERT_FALSE(Obs::records.empty());
    for (const Record &r : Obs::records) {
        EXPECT_EQ(1, r.solve);
    }
    EXPECT_EQ(1, Obs::solve_index);
}

// --- (2) FAITHFULNESS against the loop's own counters --------------------

TEST(QpWalkSeam, TheTraceAgreesWithTheCountersItMustNotHaveTouched) {
    for (const Case &cs : kCases) {
        SCOPED_TRACE(cs.name);
        const QpProblem qp = cs.build();
        ObserverGuard guard;
        Obs::active = true;
        QpEngine engine{QpOptions{}};
        const QpSolution sol = run_case(cs, qp, engine);
        const QpCounters &c = sol.counters;

        ASSERT_FALSE(Obs::truncated);
        // ONE RECORD PER MINOR, in order, numbered from zero.
        ASSERT_EQ(static_cast<std::size_t>(c.minor_iters), Obs::records.size());
        for (std::size_t i = 0; i < Obs::records.size(); ++i) {
            EXPECT_EQ(static_cast<Index>(i), Obs::records[i].minor);
            EXPECT_NE(Action::kUnset, Obs::records[i].action)
                << "minor " << i << " took a branch the seam does not enumerate";
        }

        Index degenerate = 0;
        Index adds = 0;
        Index adds_bound = 0;
        Index drops = 0;
        Index drops_bound = 0;
        Index ratio_ties = 0;
        Index drop_ties = 0;
        for (const Record &r : Obs::records) {
            if (r.degenerate) {
                ++degenerate;
            }
            if (r.action == Action::kAdd || r.action == Action::kRideStepped) {
                ++adds;
                if (r.block == Block::kBound) {
                    ++adds_bound;
                }
            }
            if (r.drop_route != Route::kNone) {
                ++drops;
                if (r.block == Block::kBound) {
                    ++drops_bound;
                }
            }
            if (r.ratio_tied > 1) {
                ++ratio_ties;
            }
            if (r.drop_tied > 1) {
                ++drop_ties;
            }
        }
        EXPECT_EQ(c.degenerate_steps, degenerate);
        EXPECT_EQ(c.ws_adds, adds);
        EXPECT_EQ(c.ws_adds_bound, adds_bound);
        EXPECT_EQ(c.ws_drops, drops);
        EXPECT_EQ(c.ws_drops_bound, drops_bound);
        EXPECT_EQ(c.ratio_ties, ratio_ties);
        EXPECT_EQ(c.drop_ties, drop_ties);
        EXPECT_EQ(c.suspect_escalations, [] {
            Index n = 0;
            for (const Record &r : Obs::records) {
                if (r.action == Action::kSuspectEscalation) {
                    ++n;
                }
            }
            return n;
        }());
    }
}

TEST(QpWalkSeam, TheWorkingSetSignatureTracksTheActionsThatChangeIt) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();
        ObserverGuard guard;
        Obs::active = true;
        QpEngine engine{QpOptions{}};
        run_case(c, qp, engine);
        ASSERT_FALSE(Obs::records.empty());

        // The first minor starts from the INCOMING working set.
        EXPECT_EQ(Obs::incoming_ws_signature, Obs::records.front().ws_sig_in);
        EXPECT_EQ(Obs::incoming_ws_size, Obs::records.front().ws_size_in);

        for (std::size_t i = 0; i + 1 < Obs::records.size(); ++i) {
            // The set one minor ends with is the set the next one begins with:
            // nothing between two passes of the loop may move it.
            EXPECT_EQ(Obs::records[i].ws_sig_out, Obs::records[i + 1].ws_sig_in)
                << "between minors " << i << " and " << i + 1;
            EXPECT_EQ(Obs::records[i].ws_size_out, Obs::records[i + 1].ws_size_in);
        }
        // THE SIZE ARITHMETIC OF A PASS, exactly. The working set grows in two
        // unrelated ways — the action, and `refresh_shifts` admitting rows the
        // homotopy is driving to feasibility — so the identity is stated with
        // the homotopy's own term in it rather than as an inequality.
        for (const Record &r : Obs::records) {
            const Index base = r.ws_size_in + r.shift_adds;
            SCOPED_TRACE(::testing::Message()
                         << "minor " << r.minor << " action " << static_cast<int>(r.action));
            EXPECT_GE(r.shift_adds, 0);
            switch (r.action) {
            case Action::kAdd:
            case Action::kRideStepped:
                EXPECT_EQ(base + 1, r.ws_size_out) << "an addition grows the set by one";
                EXPECT_NE(r.ws_sig_in, r.ws_sig_out);
                EXPECT_NE(Block::kNone, r.block);
                EXPECT_GE(r.block_index, 0);
                break;
            case Action::kDropDantzig:
            case Action::kDropZeroProbe:
                EXPECT_EQ(base - 1, r.ws_size_out) << "a drop shrinks the set by one";
                EXPECT_NE(Route::kNone, r.drop_route);
                break;
            case Action::kMoveOnly:
            case Action::kSuspectEscalation:
            case Action::kEliminatedRetreat:
            case Action::kTerminal:
            case Action::kRideUnbounded:
                EXPECT_EQ(base, r.ws_size_out);
                break;
            case Action::kStartRepair:
            case Action::kPostProbeRestart:
                // The repair pins as many variables as it needs to reach a
                // second-order consistent vertex, so no size identity holds
                // here and none is asserted. What is asserted of it is that it
                // is recorded as the repair rather than as a step.
                EXPECT_EQ(Block::kNone, r.block);
                break;
            case Action::kUnset:
                ADD_FAILURE() << "a minor recorded no action";
                break;
            }
        }
    }
}

TEST(QpWalkSeam, ADropRecordsItsRouteItsMultiplierAndTheToleranceItWasComparedAgainst) {
    // A drop has to be PROVOKED: on these fixtures the cold walk only ever
    // adds. Seeding a working set that pins BOTH variables at their lower
    // bounds, when the optimum of simple_box_qp is (0, 1), makes the drop rule
    // release x1 — the constraint whose multiplier has the wrong sign.
    const QpProblem qp = simple_box_qp();
    QpSolution seed;
    seed.x = Vec::Zero(2);
    seed.bound_state = {BoundState::kAtLower, BoundState::kAtLower};
    seed.ineq_active = {false};

    ObserverGuard guard;
    Obs::active = true;
    QpEngine engine{QpOptions{}};
    const QpSolution sol = engine.solve(qp, seed);
    ASSERT_GE(sol.counters.ws_drops, 1) << "the seeded walk dropped nothing";

    bool saw_drop = false;
    for (const Record &r : Obs::records) {
        if (r.drop_route == Route::kNone) {
            continue;
        }
        saw_drop = true;
        EXPECT_NE(Block::kNone, r.block);
        EXPECT_GE(r.block_index, 0);
        EXPECT_FALSE(std::isnan(r.decision_multiplier));
        EXPECT_FALSE(std::isnan(r.decision_tol));
        EXPECT_GT(r.decision_tol, 0.0);
        if (r.drop_route == Route::kDantzig) {
            // The Dantzig rule drops on a violation that CLEARED the tolerance;
            // the probe drops on one that did not.
            EXPECT_GT(r.decision_multiplier, r.decision_tol);
            EXPECT_GE(r.best_multiplier, r.decision_multiplier);
            EXPECT_GE(r.drop_candidates, 1);
        } else {
            EXPECT_LE(r.decision_multiplier, r.decision_tol);
        }
    }
    EXPECT_TRUE(saw_drop) << "the degenerate fixture dropped nothing; the assertions above "
                             "never ran and this test would pass vacuously";
}

TEST(QpWalkSeam, ADegenerateStepIsAnAdditionThatMovedNoFurtherThanTheStepTolerance) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        const QpProblem qp = c.build();
        ObserverGuard guard;
        Obs::active = true;
        QpEngine engine{QpOptions{}};
        run_case(c, qp, engine);
        for (const Record &r : Obs::records) {
            if (!r.degenerate) {
                continue;
            }
            ASSERT_NE(Block::kNone, r.block);
            ASSERT_FALSE(std::isnan(r.dx_inf));
            ASSERT_FALSE(std::isnan(r.step_tol));
            EXPECT_LE(r.dx_inf, r.step_tol);
        }
    }
}

// --- (2b) THE FIX-ROUND 1 DISCRIMINATING FIELDS --------------------------
//
// These are the fields added so the CAUSE of the zero-motion period-2 tail can
// be classified rather than guessed. Each test below asserts them against a
// fact the loop independently establishes — the verdict against its own
// evidence, the per-pass counter deltas against the solve's totals, the
// candidate list against the drop rule's own `best_multiplier` and tolerance —
// so none of them is a self-consistency check of the observer against itself.

TEST(QpWalkSeam, EveryPassRecordsTheMainVerdictAndThePathThatProducedIt) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        ObserverGuard guard;
        Obs::active = true;
        const QpProblem qp = c.build();
        QpEngine engine{QpOptions{}};
        const QpSolution sol = run_case(c, qp, engine);
        (void)sol;
        ASSERT_FALSE(Obs::records.empty());

        bool saw_evidence = false;
        for (const Record &r : Obs::records) {
            SCOPED_TRACE(r.minor);
            // A verdict site runs on EVERY pass: the walk's own
            // `eqp_candidate` call is unconditional at the top of the body.
            EXPECT_GE(r.inertia_verdict, 0);
            EXPECT_LE(r.inertia_verdict, 2);
            EXPECT_NE(EqpPath::kNone, r.eqp_path);

            if (r.eqp_path == EqpPath::kEmptySystem) {
                // A 0x0 system is factorized by nobody, so there is no backend
                // evidence to report — ABSENT, not a fabricated (0, 0, 0).
                EXPECT_EQ(InertiaState::kAbsent, r.inertia_state);
                EXPECT_EQ(-1, r.inertia_pos);
                continue;
            }
            EXPECT_GE(r.eqp_call_refine_steps, 0);

            if (r.inertia_state != InertiaState::kObserved) {
                EXPECT_EQ(-1, r.inertia_pos);
                EXPECT_EQ(-1, r.inertia_neg);
                continue;
            }
            saw_evidence = true;
            EXPECT_GE(r.inertia_pos, 0);
            EXPECT_GE(r.inertia_neg, 0);
            EXPECT_GE(r.inertia_expected_pos, 0);
            EXPECT_GE(r.inertia_expected_neg, 0);

            // THE ORACLE: the recorded evidence has to imply the recorded
            // verdict, by `detail::inertia_verdict`'s own rule. A triple that
            // matched the expectation and a kWrong verdict beside it would mean
            // the trace's evidence is not the evidence the gate read.
            const bool matches =
                r.inertia_pos == r.inertia_expected_pos && r.inertia_neg == r.inertia_expected_neg;
            const bool sums =
                r.inertia_pos + r.inertia_neg == r.inertia_expected_pos + r.inertia_expected_neg;
            const bool perturbed = r.inertia_perturbed > 0;
            if (!perturbed && sums) {
                EXPECT_EQ(matches ? 0 : 2, r.inertia_verdict)
                    << "evidence (" << r.inertia_pos << ", " << r.inertia_neg << ") vs expected ("
                    << r.inertia_expected_pos << ", " << r.inertia_expected_neg << ")";
            } else {
                EXPECT_EQ(1, r.inertia_verdict)
                    << "perturbed or short-summing evidence is kSuspect";
            }
        }
        EXPECT_TRUE(saw_evidence) << "no pass reported backend inertia evidence at all";
    }
}

// THE SINGLE-READ PROPERTY (fix round 2), and what can and cannot be asserted
// about it here.
//
// The hooks at the two verdict sites in `qp_engine.cpp` now read
// `factor.inertia()` ONCE, into a local, and hand that same object to
// `detail::inertia_verdict` and to `qp_walk_note_inertia`. Before this round
// each site called the accessor twice, once per consumer. That matters because
// `inertia()` is not the same kind of call on both backends: on MKL it rereads
// fields the session already cached, while on Accelerate it makes a genuine,
// independently fallible `SparseGetInertia` call
// (`src/linear/symmetric_factor_accelerate.cpp`). A RECORD-ONLY seam may not
// add a backend query, and evidence read a second time is not provably the
// evidence that decided the verdict.
//
// THE COUNT OF BACKEND INERTIA QUERIES IS NOT OBSERVABLE FROM ANY EXISTING SEAM
// IN THIS TREE, so there is no fixture here that compares it attached against
// unattached. Said plainly rather than papered over: the seam's inertia-query
// hook is `InertiaQueryFaultInjector`
// (`hven/detail/linear/fault_injection.h`), which SUBSTITUTES a return code and
// carries no counter, and it is Accelerate-only; nothing else on the linear
// seam counts calls. Inventing a counting seam is out of scope for a fix round
// and is not what the convention asks for. The property is therefore asserted
// BY CONSTRUCTION, and the construction is checkable by reading eleven lines:
// each of the two sites contains exactly one textual `inertia()` call inside
// its `#ifdef HVEN_TESTING` arm, the `#else` arm is the production statement
// verbatim, and neither arm is reachable from the other.
//
// WHAT THIS FIXTURE DOES ADD is the premise the "no re-trace" ruling rests on:
// on the backend this build links, the accessor answers the SAME THING TWICE,
// so the inertia columns of the traces taken under the old two-call form are
// equal to the evidence those walks' own verdicts were computed from. That is a
// measurement on this platform and is explicitly not a claim about Accelerate,
// where the second call is a real query that can fail on its own.
TEST(QpWalkSeam, TheBackendsInertiaAccessorAnswersTheSameThingTwiceOnThisPlatform) {
    // A symmetric INDEFINITE 4x4, so the triple under test is not the trivial
    // all-positive one: tridiag(-1, 2, -1) with the last two diagonal entries
    // negated has both signs represented.
    hven::Mat dense = hven::Mat::Zero(4, 4);
    for (Index i = 0; i < 4; ++i) {
        dense(i, i) = (i < 2) ? 2.0 : -2.0;
        if (i + 1 < 4) {
            dense(i, i + 1) = -1.0;
            dense(i + 1, i) = -1.0;
        }
    }
    std::vector<Eigen::Triplet<double>> triplets;
    for (Index i = 0; i < 4; ++i) {
        for (Index j = i; j < 4; ++j) {
            if (i == j || dense(i, j) != 0.0) {
                triplets.emplace_back(static_cast<int>(i), static_cast<int>(j), dense(i, j));
            }
        }
    }
    hven::SpMatRM A(4, 4);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();

    hven::linear::SymmetricFactor factor{hven::linear::SymmetricFactor::Options{}};
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);

    const hven::linear::InertiaEvidence first = factor.inertia();
    const hven::linear::InertiaEvidence second = factor.inertia();
    ASSERT_EQ(static_cast<int>(hven::linear::InertiaEvidence::State::kObserved),
              static_cast<int>(first.state))
        << "this platform's backend did not report inertia at all: the premise below is untested";
    EXPECT_EQ(static_cast<int>(first.state), static_cast<int>(second.state));
    EXPECT_EQ(first.n_pos, second.n_pos);
    EXPECT_EQ(first.n_neg, second.n_neg);
    EXPECT_EQ(first.n_zero, second.n_zero);
    EXPECT_EQ(first.zero_is_derived, second.zero_is_derived);
    EXPECT_EQ(first.perturbed_pivots.has_value(), second.perturbed_pivots.has_value());
    if (first.perturbed_pivots.has_value() && second.perturbed_pivots.has_value()) {
        EXPECT_EQ(*first.perturbed_pivots, *second.perturbed_pivots);
    }
    // NON-VACUITY: the matrix really is indefinite, so a triple that agreed
    // only because it was all zeros would not pass here.
    EXPECT_GT(first.n_pos, 0);
    EXPECT_GT(first.n_neg, 0);
    // And the accessor agrees with the factorization's own returned evidence,
    // which is the other object the engine could have read.
    EXPECT_EQ(static_cast<int>(outcome.inertia.state), static_cast<int>(first.state));
    EXPECT_EQ(outcome.inertia.n_pos, first.n_pos);
    EXPECT_EQ(outcome.inertia.n_neg, first.n_neg);
}

TEST(QpWalkSeam, ThePerPassCounterDeltasAreNonNegativeAndBoundedByTheSolvesTotals) {
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        ObserverGuard guard;
        Obs::active = true;
        const QpProblem qp = c.build();
        QpEngine engine{QpOptions{}};
        const QpSolution sol = run_case(c, qp, engine);
        ASSERT_FALSE(Obs::records.empty());

        Index facts = 0;
        Index eqp_ref = 0;
        Index bor_ref = 0;
        Index ver_ref = 0;
        Index schur = 0;
        for (const Record &r : Obs::records) {
            // Differences of monotone counters cannot be negative; one that is
            // would mean the scope's base was taken after the counter moved.
            EXPECT_GE(r.pass_factorizations, 0);
            EXPECT_GE(r.pass_eqp_refine_steps, 0);
            EXPECT_GE(r.pass_border_refine_steps, 0);
            EXPECT_GE(r.pass_verdict_refine_steps, 0);
            EXPECT_GE(r.pass_schur_updates, 0);
            facts += r.pass_factorizations;
            eqp_ref += r.pass_eqp_refine_steps;
            bor_ref += r.pass_border_refine_steps;
            ver_ref += r.pass_verdict_refine_steps;
            schur += r.pass_schur_updates;
        }
        // Bounded by, not equal to: the seed ingest and the hot-start gate do
        // work OUTSIDE the walk loop, which no pass can claim.
        EXPECT_LE(facts, sol.counters.factorizations);
        EXPECT_LE(eqp_ref, sol.counters.eqp_refine_steps);
        EXPECT_LE(bor_ref, sol.counters.border_refine_steps);
        EXPECT_LE(ver_ref, sol.counters.verdict_refine_steps);
        EXPECT_LE(schur, sol.counters.schur_updates);
        EXPECT_GT(facts, 0) << "no pass recorded a factorization";
    }
}

TEST(QpWalkSeam, APostDropPassRecordsWhatWasReleasedAndWhyTheRideDidOrDidNotRun) {
    // The wrong-seed cases are the ones that DROP; without one of them every
    // assertion below would pass vacuously.
    bool saw_post_drop = false;
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        ObserverGuard guard;
        Obs::active = true;
        const QpProblem qp = c.build();
        QpEngine engine{QpOptions{}};
        (void)run_case(c, qp, engine);
        const std::vector<Record> recs = Obs::records;
        ASSERT_FALSE(recs.empty());

        for (std::size_t k = 0; k < recs.size(); ++k) {
            const Record &r = recs[k];
            SCOPED_TRACE(r.minor);
            if (!r.post_drop) {
                // Nothing was released, so §4c was never reached and every
                // field about it stays ABSENT.
                EXPECT_EQ(RideOutcome::kNotAttempted, r.ride_outcome);
                EXPECT_EQ(-1, r.released_index);
                EXPECT_EQ(Block::kNone, r.released_block);
                EXPECT_FALSE(r.curvature_fired);
                EXPECT_TRUE(std::isnan(r.p_released));
                continue;
            }
            saw_post_drop = true;
            EXPECT_NE(Block::kNone, r.released_block);
            EXPECT_GE(r.released_index, 0);
            EXPECT_FALSE(std::isnan(r.p_released));
            EXPECT_FALSE(std::isnan(r.released_directional));
            EXPECT_FALSE(std::isnan(r.ride_slope)) << "g^T p is recorded on every post-drop pass";

            if (r.curvature_fired) {
                // The gate fired, so `ride_negative_curvature` RAN, so one of
                // its branches must be named. "The ride did not step" standing
                // in for a reason is exactly the gap this field closes.
                EXPECT_NE(RideOutcome::kNotAttempted, r.ride_outcome);
                EXPECT_FALSE(std::isnan(r.curvature));
                EXPECT_FALSE(std::isnan(r.p_hp));
                EXPECT_FALSE(std::isnan(r.p_sq));
                EXPECT_GT(r.p_sq, 0.0);
                EXPECT_LE(r.curvature, r.curvature_tol);
                if (r.ride_outcome == RideOutcome::kStepped) {
                    EXPECT_TRUE(r.ride);
                    EXPECT_NE(0, r.ride_sign_chosen);
                    EXPECT_EQ(Action::kRideStepped, r.action);
                } else if (r.ride_outcome == RideOutcome::kSlopeFlatVeto ||
                           r.ride_outcome == RideOutcome::kSignDisagreementVeto) {
                    EXPECT_EQ(0, r.ride_sign_chosen);
                    EXPECT_FALSE(std::isnan(r.ride_slope_tol));
                    EXPECT_FALSE(std::isnan(r.ride_leaves));
                }
            } else {
                // §4c was armed but the curvature quotient did not clear the
                // threshold, so the ride was never offered the direction.
                EXPECT_EQ(RideOutcome::kNotAttempted, r.ride_outcome);
                EXPECT_FALSE(std::isnan(r.curvature));
                EXPECT_GT(r.curvature, r.curvature_tol);
            }

            // THE LINK TO THE LOOP'S OWN RECORD OF THE DROP: the pass before a
            // post-drop pass is the one whose drop rule released something.
            ASSERT_GT(k, 0u);
            const Record &prev = recs[k - 1];
            EXPECT_NE(Route::kNone, prev.drop_route)
                << "a post-drop pass whose predecessor recorded no drop";
            EXPECT_EQ(prev.block_index, r.released_index);
        }
    }
    EXPECT_TRUE(saw_post_drop) << "no case exercised a post-drop pass";
}

TEST(QpWalkSeam, TheDropCandidateSetIsRecordedLargestViolationFirst) {
    bool saw_candidates = false;
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        ObserverGuard guard;
        Obs::active = true;
        const QpProblem qp = c.build();
        QpEngine engine{QpOptions{}};
        (void)run_case(c, qp, engine);
        ASSERT_FALSE(Obs::records.empty());

        for (const Record &r : Obs::records) {
            if (r.drop_route != Route::kDantzig) {
                EXPECT_EQ(0, r.candidates_recorded);
                continue;
            }
            SCOPED_TRACE(r.minor);
            saw_candidates = true;
            ASSERT_GT(r.drop_candidates, 0);
            const Index expected = std::min<Index>(
                r.drop_candidates, static_cast<Index>(Record::kMaxRecordedCandidates));
            ASSERT_EQ(expected, r.candidates_recorded);

            // Largest violation first, and the largest one IS the drop rule's
            // own `best_multiplier` — a number the observer did not compute.
            EXPECT_EQ(r.best_multiplier, r.candidates[0].violation);
            for (Index k = 0; k + 1 < r.candidates_recorded; ++k) {
                EXPECT_GE(r.candidates[static_cast<std::size_t>(k)].violation,
                          r.candidates[static_cast<std::size_t>(k + 1)].violation);
            }
            // Every candidate cleared the rule's own admission test.
            for (Index k = 0; k < r.candidates_recorded; ++k) {
                const auto &cand = r.candidates[static_cast<std::size_t>(k)];
                EXPECT_GT(cand.violation, r.decision_tol);
                EXPECT_NE(Block::kNone, cand.block);
                EXPECT_GE(cand.index, 0);
            }
            // When the whole set fits, the constraint actually released is in
            // it — the property that makes "the rule could have chosen
            // otherwise" readable off the trace.
            if (r.drop_candidates <= static_cast<Index>(Record::kMaxRecordedCandidates)) {
                bool found = false;
                for (Index k = 0; k < r.candidates_recorded; ++k) {
                    const auto &cand = r.candidates[static_cast<std::size_t>(k)];
                    found = found || (cand.block == r.block && cand.index == r.block_index);
                }
                EXPECT_TRUE(found) << "the released constraint is not among its own candidates";
            }
        }
    }
    EXPECT_TRUE(saw_candidates) << "no case exercised the Dantzig drop rule";
}

TEST(QpWalkSeam, TheRegularizationAndResidualsAreRecordedOnEveryPass) {
    const QpOptions defaults{};
    for (const Case &c : kCases) {
        SCOPED_TRACE(c.name);
        ObserverGuard guard;
        Obs::active = true;
        const QpProblem qp = c.build();
        QpEngine engine{QpOptions{}};
        (void)run_case(c, qp, engine);
        ASSERT_FALSE(Obs::records.empty());

        for (const Record &r : Obs::records) {
            SCOPED_TRACE(r.minor);
            // The EFFECTIVE pair, which §4b's suspect ladder can only raise.
            EXPECT_GE(r.primal_delta, defaults.primal_delta);
            EXPECT_EQ(defaults.dual_mu, r.dual_mu);
            // Residual norms: finite, non-negative, and a scale of at least 1
            // by free_block_stationarity's own construction.
            EXPECT_FALSE(std::isnan(r.stat_free_inf));
            EXPECT_GE(r.stat_free_inf, 0.0);
            EXPECT_GE(r.stat_scale, 1.0);
            EXPECT_FALSE(std::isnan(r.active_res_inf));
            EXPECT_GE(r.active_res_inf, 0.0);
            EXPECT_GE(r.p_sq, 0.0);
        }
    }
}

// --- (3) THE SIGNATURE ---------------------------------------------------

TEST(QpWalkSeam, TheSignatureIsOrderIndependentSideSensitiveAndIdentitySensitive) {
    using hven::solvers::detail::testing::qp_walk_ws_signature;
    using hven::solvers::detail::testing::qp_walk_ws_size;

    WorkingSet a(4, 3);
    a.add_ineq(2);
    a.add_ineq(0);
    a.bound_state()[1] = BoundState::kAtLower;
    a.bound_state()[3] = BoundState::kAtUpper;

    // The same MEMBERS, built in a different order.
    WorkingSet b(4, 3);
    b.bound_state()[3] = BoundState::kAtUpper;
    b.add_ineq(0);
    b.bound_state()[1] = BoundState::kAtLower;
    b.add_ineq(2);
    EXPECT_EQ(qp_walk_ws_signature(a), qp_walk_ws_signature(b));
    EXPECT_EQ(4, qp_walk_ws_size(a));
    EXPECT_EQ(qp_walk_ws_size(a), qp_walk_ws_size(b));

    // The SIDE matters: the same variable at the other bound is a different
    // working set, which is precisely what a lower/upper ping-pong looks like.
    WorkingSet c = b;
    c.bound_state()[1] = BoundState::kAtUpper;
    EXPECT_NE(qp_walk_ws_signature(a), qp_walk_ws_signature(c));
    EXPECT_EQ(qp_walk_ws_size(a), qp_walk_ws_size(c));

    // The IDENTITY matters, and a row never collides with a variable of the
    // same index.
    WorkingSet d(4, 3);
    d.add_ineq(1);
    WorkingSet e(4, 3);
    e.bound_state()[1] = BoundState::kAtLower;
    EXPECT_NE(qp_walk_ws_signature(d), qp_walk_ws_signature(e));

    // An empty set is zero, so "nothing pinned" is not a hash of nothing.
    const WorkingSet empty(4, 3);
    EXPECT_EQ(0u, qp_walk_ws_signature(empty));
    EXPECT_EQ(0, qp_walk_ws_size(empty));
}

TEST(QpWalkSeam, ResetClearsEverythingTheObserverHolds) {
    ObserverGuard guard;
    Obs::active = true;
    Obs::capture_solve = 3;
    Obs::max_records = 7;
    QpEngine engine{QpOptions{}};
    engine.solve(simple_box_qp());
    Obs::reset();
    EXPECT_FALSE(Obs::active);
    EXPECT_EQ(0, Obs::solves);
    EXPECT_EQ(-1, Obs::capture_solve);
    EXPECT_FALSE(Obs::truncated);
    EXPECT_TRUE(Obs::records.empty());
    EXPECT_FALSE(Obs::capturing);
    EXPECT_EQ(-1, Obs::exit_status);
    EXPECT_EQ(nullptr, Obs::current);
    EXPECT_FALSE(Obs::allocation_failed);
    EXPECT_EQ(1000000u, Obs::max_records);
    EXPECT_EQ(EqpPath::kNone, Obs::eqp.path);
    EXPECT_EQ(InertiaState::kAbsent, Obs::eqp.state);
    EXPECT_EQ(-1, Obs::eqp.refine_steps);
    EXPECT_FALSE(Obs::eqp.k0_rebuilt);
}

TEST(QpWalkSeam, TheRecordCapIsHonouredAndReported) {
    ObserverGuard guard;
    Obs::active = true;
    Obs::max_records = 1;
    QpEngine engine{QpOptions{}};
    engine.solve(many_minor_qp());
    EXPECT_EQ(1u, Obs::records.size());
    EXPECT_TRUE(Obs::truncated);
}

// --- (4) THE NEUTRALITY CHECK'S OWN NON-VACUITY --------------------------

TEST(QpWalkSeam, TheNeutralityComparisonWouldCatchAMoveIfThereWereOne) {
    // A gate that cannot fail is not a gate. `expect_same_counters` and
    // `same_bits` are what the neutrality test rests on, and both are asserted
    // here to REJECT a difference -- the field-by-field arm, the whole-object
    // `memcmp` arm (which is the one that would carry a field the named list
    // forgets), and the bit comparison on the returned vectors.
    ObserverGuard guard;
    Obs::active = true;
    QpEngine engine{QpOptions{}};
    const QpSolution sol = engine.solve(many_minor_qp());
    ASSERT_GT(sol.counters.minor_iters, 0);

    QpCounters bumped = sol.counters;
    ++bumped.minor_iters;
    EXPECT_NE(0, memcmp_ignoring_padding(sol.counters, bumped))
        << "the whole-object arm cannot see a one-count difference";
    EXPECT_NE(sol.counters.minor_iters, bumped.minor_iters);

    // A field the NAMED list does carry, moved by the smallest possible
    // amount, and a field it carries as a bool.
    QpCounters flipped = sol.counters;
    flipped.k0_reused = !flipped.k0_reused;
    EXPECT_NE(0, memcmp_ignoring_padding(sol.counters, flipped));

    // And the PADDING MASK itself is not vacuous: it found padding, and it did
    // not swallow the whole object. A mask that covered everything would make
    // both assertions above pass for the wrong reason.
    const std::vector<bool> &pad = counters_padding_mask();
    ASSERT_EQ(sizeof(QpCounters), pad.size());
    std::size_t padded = 0;
    for (std::size_t i = 0; i < pad.size(); ++i) {
        padded += pad[i] ? 1u : 0u;
    }
    EXPECT_GT(padded, 0u) << "no padding found: the whole-object arm is comparing stack residue";
    EXPECT_LT(padded, sizeof(QpCounters) / 2)
        << "the mask covers most of the object: it is not a padding mask";

    // And the float comparison: one ULP, and a sign flip on a zero.
    Vec a = sol.x;
    ASSERT_GT(a.size(), 0);
    Vec b = a;
    b(0) = std::nextafter(b(0), std::numeric_limits<double>::infinity());
    EXPECT_FALSE(same_bits(a, b, "one ulp"));
    Vec zero_pos = Vec::Zero(1);
    Vec zero_neg = Vec::Constant(1, -0.0);
    EXPECT_FALSE(same_bits(zero_pos, zero_neg, "signed zero"));
    EXPECT_TRUE(same_bits(a, a, "self"));
}
