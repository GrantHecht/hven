#pragma once

// The golden-numerics rig's "seam under test": one abstract surface that the
// product (hven::linear) and the two seams it replaces can all be driven
// through, so a trace is written once and executed against each of them.
//
// The surface mirrors hven/linear/symmetric_factor.h clause for clause --
// analyze / factorize / solve / partial solve / evidence / handle / thread
// pin -- because that is the contract the migrations have to reproduce. It is
// deliberately NOT a superset: an operation that the unified surface does not
// have is not reachable from here, and an operation an OLD seam does not have
// is reported ABSENT through Capabilities rather than emulated. Emulating a
// missing operation would make the rig prove a property of the rig.
//
// EVIDENCE TYPES ARE HVEN'S OWN. InertiaEvidence, FactorizeOutcome and
// SolveInfo are reused verbatim as the rig's neutral currency, for two
// reasons: they are the shapes the migrations must land on, and they are
// honest by construction (absent evidence is std::nullopt, a failed query is
// its own state). An adapter over an old seam reports what that seam actually
// says -- including where the old seam fabricates a value the new surface
// would report absent. That divergence is the point of several traces; it is
// never smoothed over here.
//
// TEST-ONLY, AND TEMPORARY FOR TWO OF THE THREE. The two old-seam adapters
// (seam_psiopt.cpp, seam_sqp.cpp) exist to prove old-vs-new reproduction at
// the two engine migrations and are DELETED once those close. Nothing in this
// directory is installed or linked into the hven library.

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "hven/core/types.h"
#include "hven/linear/symmetric_factor.h"

namespace hven::rig {

// Which seam an arm drives.
enum class SeamId {
    kNative,    // hven::linear::SymmetricFactor -- always available
    kPsioptOld, // Eigen::PardisoLDLT / AccelerateLDLTTPP  (HVEN_RIG_PSIOPT_SEAM)
    kSqpOld     // tycho::sqp::KktSystem + the LAPACKE border (HVEN_RIG_SQP_SEAM)
};

const char *seam_id_name(SeamId id);

// Which sparse backend this build compiled against. One value per build --
// the rig does not select a backend at runtime, exactly as hven does not.
const char *backend_arm_name();

// How an arm pins its thread count, recorded verbatim into every expected
// table's provenance banner. Every asserted row must record the mechanism its
// arm pinned with and the value it pinned to, so an adapter reports the
// mechanism it actually possesses rather than a mechanism the rig wishes it
// had.
enum class ThreadPinMechanism {
    kPerInstance,   // the seam carries its own per-instance thread option
    kProcessGlobal, // the seam has none; the rig pins the process instead
    // NOTHING WAS PINNED on this arm. Either the backend has no thread
    // control at all, or -- the case the native arm hits on Accelerate -- the
    // surface accepts a thread count and stores it without applying it to any
    // backend call. The distinction between those two matters to a reader of
    // the source and not at all to a reader of a row: both mean the numbers
    // were produced at whatever width the backend chose for itself, which is
    // the only thing a provenance column can honestly claim. An arm reporting
    // this pairs it with a pin VALUE of 0 -- there is no count to name.
    kAbsent
};

const char *thread_pin_mechanism_name(ThreadPinMechanism m);

// What a seam can and cannot do. A trace that needs a capability an arm
// lacks SKIPS on that arm, with the missing capability named in the skip
// message -- it never silently passes and never fails as though the seam were
// broken.
struct Capabilities {
    bool partial_solve = false;            // phase-split (forward/diagonal/backward) solves
    bool partial_solve_predicate = false;  // a supports_partial_solve()-equivalent gate
    bool share_handle = false;             // a co-owning, outlive-the-engine factorization handle
    bool epoch = false;                    // a numeric-generation stamp for staleness detection
    bool adopt = false;                    // build an engine on top of a shared handle
    bool multi_rhs = false;                // a solve taking more than one right-hand side
    bool reports_refinement_iters = false; // the seam surfaces a refinement-step count at all
    bool reports_perturbed_pivots = false; // the seam surfaces a perturbed-pivot count at all
    bool reports_inertia = false;          // the seam surfaces positive/negative counts
};

// The configuration an arm is constructed with. This is the vehicle for
// per-seam OPTION PARITY: when the native seam runs as the comparison arm for
// an old seam, it is constructed with the configuration that old seam
// actually has, not with hven's own defaults. See seam_registry.cpp's arm
// table, which documents every parity setting and why it is set.
struct SeamOptions {
    int num_threads = 1;          // 1 on every asserted run
    int pivot_perturb_exp = 8;    // Pardiso static pivot perturbation 10^-k
    int max_refinement_iters = 0; // full-solve iterative-refinement cap

    // Mirrors hven::linear::SymmetricFactor::Options::Ordering, including its
    // don't-write-by-default semantics: kBackendDefault means the arm does
    // not touch the backend's ordering parameter at all.
    enum class Ordering {
        kBackendDefault,
        kMinimumDegree,
        kNestedDissection,
        kParallelNestedDissection
    };
    Ordering ordering = Ordering::kBackendDefault;

    // Same don't-write-by-default rule: false leaves the backend's own value
    // alone rather than writing a zero.
    bool weighted_matching = false;
};

// Exact call counts, the rig's asserted currency: counters are exact
// integers, per backend, always.
//
// On the native seam these are the engine's own counters. On an old seam they
// are counted BY THE ADAPTER, because neither old seam counts anything -- the
// adapter is the thing making the calls, so it can count them honestly. One
// consequence is recorded rather than hidden: the sqp old seam's factorize()
// re-runs its own symbolic analysis whenever the pattern changed, so the
// adapter attributes that implicit analysis to analyze_count (it can see it
// coming, via that seam's own public pattern_matches()). That is an
// observation of what the seam does, not an adjustment to make it look like
// the new one.
struct Counters {
    Index analyze_count = 0;
    Index factorize_count = 0;
    Index solve_count = 0;
    Index partial_solve_count = 0;
};

// A read-only, co-owning view of a factorization, for the seams that have
// one. Only the native seam does today; the old-seam adapters report
// Capabilities::share_handle == false and never produce one.
class SeamHandle {
  public:
    virtual ~SeamHandle();
    virtual hven::linear::SolveInfo solve(const Vec &rhs, Vec &x) const = 0;
    virtual hven::linear::InertiaEvidence inertia() const = 0;
    virtual std::uint64_t epoch() const = 0;
    virtual std::uint64_t pattern_hash() const = 0;
};

class SeamUnderTest {
  public:
    virtual ~SeamUnderTest();

    // --- identity and self-description ---
    virtual SeamId id() const = 0;
    virtual Capabilities capabilities() const = 0;
    virtual ThreadPinMechanism thread_pin_mechanism() const = 0;
    // The pinned value, as applied. Reported separately from the mechanism
    // because an expected-table row carries both.
    virtual int thread_pin_value() const = 0;

    // What backend configuration is ACTUALLY in force on this arm, in prose,
    // including any SeamOptions field the seam has no way to write. Recorded
    // into the report so a configuration difference between two arms is read
    // off the artifact rather than reconstructed from source. An arm whose
    // effective configuration is not the one it was asked for says so here --
    // the sqp old seam writes exactly one backend parameter and rides its
    // library's initializer for every other, which is a real difference from
    // hven's defaults and the reason the parity arms exist.
    virtual std::string configuration_note() const = 0;

    // --- lifecycle, mirroring the unified surface's own ---
    virtual void analyze(const SpMatRM &A) = 0;
    virtual hven::linear::FactorizeOutcome factorize(const SpMatRM &A) = 0;
    virtual hven::linear::SolveInfo solve(const Vec &rhs, Vec &x) = 0;

    // Multi-RHS. Only called when Capabilities::multi_rhs is true.
    virtual hven::linear::SolveInfo solve_multi(const Mat &RHS, Mat &X) = 0;

    // Phase-split solve. Only called when Capabilities::partial_solve is true.
    virtual hven::linear::SolveInfo solve_partial(hven::linear::SymmetricFactor::SolvePhase phase,
                                                  const Vec &rhs, Vec &x) = 0;

    // The composability gate. Only meaningful when
    // Capabilities::partial_solve_predicate is true.
    virtual bool supports_partial_solve() const = 0;

    virtual hven::linear::InertiaEvidence inertia() const = 0;
    virtual Counters counters() const = 0;

    // --- handles. Only called when the matching capability is true. ---
    virtual std::shared_ptr<const SeamHandle> share() = 0;
    virtual std::uint64_t epoch() const = 0;

    // Build a NEW engine of this seam's own kind on top of a shared handle,
    // inheriting the emitting engine's configuration. Only called when
    // Capabilities::adopt is true. Kept as a member rather than a free
    // factory so the adopted engine is guaranteed to be the same seam as the
    // handle it came from.
    virtual std::unique_ptr<SeamUnderTest>
    adopt(std::shared_ptr<const SeamHandle> handle) const = 0;
};

// Builds a seam. Throws std::invalid_argument for a seam that this build was
// not configured with (the old-seam adapters are compiled only when their
// CMake path option is supplied), so an arm that cannot exist never silently
// degrades into a different one.
std::unique_ptr<SeamUnderTest> make_seam(SeamId id, const SeamOptions &opts);

// True iff this build compiled the given seam's adapter in.
bool seam_available(SeamId id);

// One (seam, configuration) pair the traces are parameterized over.
struct ArmSpec {
    // The arm's name, as it appears in an expected table's `arm` column
    // (backend suffix appended by label()). Stable across runs -- expected
    // tables key on it.
    std::string name;
    SeamId seam;
    SeamOptions options;
    // Why this arm's options are what they are. For the parity arms this is
    // the documented per-seam option-parity setting.
    std::string parity_note;

    // "<name>@<backend>", the full row key in an expected table.
    std::string label() const;
};

// Prints an arm as its label. Found by argument-dependent lookup, so the test
// framework uses it instead of dumping the struct's bytes -- which is not
// cosmetic: the framework's printed form of a parameter is what ends up in the
// ctest test name, and a byte dump there makes every parameterized trace
// unnameable on the command line and unreadable in a failure report.
void PrintTo(const ArmSpec &arm, std::ostream *os);

// Every arm this build can run, in a stable order: the native arms first
// (always present), then whichever old seams were configured in.
const std::vector<ArmSpec> &arms();

// The subset of arms() that need no old-seam checkout. This is what the
// default test suite runs, so CI exercises the harness on every commit.
std::vector<ArmSpec> native_only_arms();

// The thread count the one multithreaded (unasserted, smoke) leg in this suite
// requests.
//
// It is an EXPLICIT count greater than one wherever the machine has more than
// one core, never "whatever the backend defaults to". The default is not good
// enough, for a concrete reason: the derivation invocation exports a
// single-thread setting for the whole process, so a leg asking for the default
// silently resolves to one thread, the cross-thread comparison compares a run
// against itself, and the deviation the trace exists to record comes out
// identically zero. An explicit request goes through the seam's own thread
// control, which overrides that environment setting, so the leg is genuinely
// multithreaded whatever the invocation. The value is also recorded as an
// observation, because a cross-thread deviation is not interpretable without
// knowing what it was measured against.
//
// Returns 1 only on a genuinely single-core machine, where the trace records
// that its smoke leg had nothing to vary.
int smoke_thread_count();

} // namespace hven::rig
