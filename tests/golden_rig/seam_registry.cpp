// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The arm table: every (seam, configuration) pair this build can run, and the
// per-seam OPTION PARITY that makes a cross-seam comparison mean something.
//
// WHY PARITY ARMS EXIST. Two seams configured differently will disagree on
// float values for reasons that have nothing to do with either being wrong.
// The sqp old seam writes exactly one backend parameter and rides its
// library's initializer for the rest -- including the iterative-refinement
// cap, which the initializer sets nonzero and hven's frozen default writes as
// zero. Comparing that seam's refined full solves against hven's unrefined
// ones would record a configuration difference as a numeric one. So for each
// old seam the table carries a NATIVE arm configured to match that seam's
// actual configuration, and every parity setting is documented in the arm's
// own parity_note (which the report prints alongside every observation).
//
// PARITY VALUES ARE PROBED, NOT ASSUMED, wherever the thing being matched is
// a backend default rather than a specification constant. The refinement cap
// and the pivot-perturbation exponent the sqp arm rides come from a live call
// to the backend's own initializer here, for the same reason the repository's
// don't-write-by-default coverage test compares against a fresh initializer
// call instead of a literal: those are library defaults, not contract
// constants, and a version bump is allowed to move them.

#include <algorithm>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if !defined(__APPLE__)
#include <mkl_pardiso.h>
#include <mkl_types.h>
#endif

#include "seam.h"

namespace hven::rig {

SeamHandle::~SeamHandle() = default;
SeamUnderTest::~SeamUnderTest() = default;

// Defined in the per-seam adapter TUs. Declared here rather than in seam.h so
// the header carries only the surface the traces use.
std::unique_ptr<SeamUnderTest> make_native_seam(const SeamOptions &opts);
#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM)
std::unique_ptr<SeamUnderTest> make_psiopt_seam(const SeamOptions &opts);
#endif
#if defined(HVEN_RIG_HAVE_SQP_SEAM)
std::unique_ptr<SeamUnderTest> make_sqp_seam(const SeamOptions &opts);
#endif

const char *seam_id_name(SeamId id) {
    switch (id) {
    case SeamId::kNative:
        return "native";
    case SeamId::kPsioptOld:
        return "psiopt-old";
    case SeamId::kSqpOld:
        return "sqp-old";
    }
    return "unknown";
}

const char *backend_arm_name() {
#if defined(__APPLE__)
    return "accelerate";
#else
    return "mkl";
#endif
}

const char *thread_pin_mechanism_name(ThreadPinMechanism m) {
    switch (m) {
    case ThreadPinMechanism::kPerInstance:
        return "per-instance";
    case ThreadPinMechanism::kProcessGlobal:
        return "process-global";
    case ThreadPinMechanism::kAbsent:
        return "absent";
    case ThreadPinMechanism::kSeamThreadLocalBinary:
        return "seam-thread-local-binary";
    }
    return "unknown";
}

std::string ArmSpec::label() const { return name + "@" + backend_arm_name(); }

void PrintTo(const ArmSpec &arm, std::ostream *os) { *os << arm.label(); }

bool seam_available(SeamId id) {
    switch (id) {
    case SeamId::kNative:
        return true;
    case SeamId::kPsioptOld:
#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM)
        return true;
#else
        return false;
#endif
    case SeamId::kSqpOld:
#if defined(HVEN_RIG_HAVE_SQP_SEAM)
        return true;
#else
        return false;
#endif
    }
    return false;
}

std::unique_ptr<SeamUnderTest> make_seam(SeamId id, const SeamOptions &opts) {
    switch (id) {
    case SeamId::kNative:
        return make_native_seam(opts);
    case SeamId::kPsioptOld:
#if defined(HVEN_RIG_HAVE_PSIOPT_SEAM)
        return make_psiopt_seam(opts);
#else
        break;
#endif
    case SeamId::kSqpOld:
#if defined(HVEN_RIG_HAVE_SQP_SEAM)
        return make_sqp_seam(opts);
#else
        break;
#endif
    }
    throw std::invalid_argument(
        std::string("hven::rig::make_seam: seam '") + seam_id_name(id) +
        "' was not compiled into this build -- configure with its HVEN_RIG_*_SEAM path option");
}

namespace {

#if !defined(__APPLE__)
// One entry of the backend initializer's own parameter array, for the
// symmetric-indefinite matrix type the whole rig runs. Probed live so a parity
// arm mirrors what the old seam actually rides on THIS machine's library
// version rather than what a report once measured on another one.
struct BackendDefaults {
    int refinement_cap = 0;
    int pivot_perturb_exp = 0;
};

BackendDefaults probe_backend_defaults() {
    void *pt[64] = {};
    MKL_INT iparm[64] = {};
    MKL_INT mtype = -2;
    ::pardisoinit(pt, &mtype, iparm);
    return BackendDefaults{static_cast<int>(iparm[7]), static_cast<int>(iparm[9])};
}
#endif

std::vector<ArmSpec> build_arms() {
    std::vector<ArmSpec> out;

    {
        ArmSpec a;
        a.name = "native";
        a.seam = SeamId::kNative;
        a.options = SeamOptions{}; // hven's own frozen defaults, threads pinned to 1
        a.parity_note = "no parity adjustment: hven's frozen defaults, which is the "
                        "configuration the migrated engines will actually run.";
        out.push_back(std::move(a));
    }

#if !defined(__APPLE__)
    const BackendDefaults defaults = probe_backend_defaults();

    {
        // Parity with the sqp old seam. That seam writes ONLY zero-based CSR
        // indexing and rides the initializer for everything else, so the
        // matching native configuration is: don't write the ordering entry,
        // don't write the weighted-matching entry, and set the two entries
        // hven always writes to the values the initializer would have left.
        ArmSpec a;
        a.name = "native-sqp-parity";
        a.seam = SeamId::kNative;
        a.options.num_threads = 1;
        a.options.max_refinement_iters = defaults.refinement_cap;
        a.options.pivot_perturb_exp = defaults.pivot_perturb_exp;
        a.options.ordering = SeamOptions::Ordering::kBackendDefault;
        a.options.weighted_matching = false;
        a.parity_note =
            "mirrors the sqp old seam's ACTUAL configuration. refinement cap = " +
            std::to_string(defaults.refinement_cap) +
            " and pivot-perturbation exponent = " + std::to_string(defaults.pivot_perturb_exp) +
            ", both read from a live probe of the backend initializer this build links (they are "
            "library defaults, not contract constants). Ordering and weighted matching are left "
            "UNWRITTEN, which is exactly what that seam does -- writing the same value explicitly "
            "would be a different act with the same effect, and the point of the don't-write "
            "semantics is that it is not written. Closes the refinement-default gap: hven's own "
            "default writes a zero cap where this seam refines.";
        out.push_back(std::move(a));
    }

    {
        // Parity with the psiopt old seam: the interior-point engine's own
        // settings. Note the consequence, which is not a side effect but a
        // property of the configuration: weighted matching ON forces
        // supports_partial_solve() to false for this arm's whole lifetime,
        // because composition under active matching is unexercised at scale.
        ArmSpec a;
        a.name = "native-psiopt-parity";
        a.seam = SeamId::kNative;
        a.options.num_threads = 1;
        a.options.max_refinement_iters = 0;
        a.options.pivot_perturb_exp = 8;
        a.options.ordering = SeamOptions::Ordering::kNestedDissection;
        a.options.weighted_matching = true;
        a.parity_note =
            "mirrors the interior-point engine's own settings: nested-dissection ordering, "
            "static pivot perturbation 10^-8, maximum weighted matching ON, refinement cap 0 "
            "(that engine's shipped default is zero refinement steps, so hven's frozen default "
            "already matches and no adjustment is needed there). CONSEQUENCE, stated rather than "
            "discovered: weighted matching on makes supports_partial_solve() false for this arm "
            "unconditionally, so partial-solve traces report the gate closed on it -- that is the "
            "configuration's real behaviour, not a rig limitation. The old seam has no "
            "don't-write state at all: it writes about twenty-five backend parameters "
            "unconditionally, so an ordering request of backend-default is not representable "
            "there and lands on that engine's own value.";
        out.push_back(std::move(a));
    }
#endif

    if (seam_available(SeamId::kPsioptOld)) {
        ArmSpec a;
        a.name = "psiopt-old";
        a.seam = SeamId::kPsioptOld;
        a.options.num_threads = 1;
        a.options.max_refinement_iters = 0;
        a.options.pivot_perturb_exp = 8;
        a.options.ordering = SeamOptions::Ordering::kNestedDissection;
        a.options.weighted_matching = true;
        a.parity_note = "the interior-point engine's own settings, driven through its own seam. "
                        "Its native comparison arm is native-psiopt-parity.";
        out.push_back(std::move(a));
    }

    if (seam_available(SeamId::kSqpOld)) {
        ArmSpec a;
        a.name = "sqp-old";
        a.seam = SeamId::kSqpOld;
        a.options.num_threads = 1;
        a.parity_note = "this seam applies none of the rig's backend-parameter options -- it "
                        "writes one backend parameter and rides its library initializer for the "
                        "rest, so the options above are recorded for the row and not applied. "
                        "Its native comparison arm is native-sqp-parity, whose values come from "
                        "a live probe of that same initializer.";
        out.push_back(std::move(a));
    }

    return out;
}

} // namespace

const std::vector<ArmSpec> &arms() {
    static const std::vector<ArmSpec> table = build_arms();
    return table;
}

int smoke_thread_count() {
    // Capped low on purpose: the point is that MORE THAN ONE thread is in
    // play, not that the machine is saturated. A big count would make the leg
    // slow and, on a shared box, make its recorded deviation a measurement of
    // whoever else is running.
    constexpr unsigned kCap = 4;
    const unsigned cores = std::thread::hardware_concurrency();
    if (cores <= 1) {
        return 1; // single core, or unknowable -- nothing to vary
    }
    return static_cast<int>(std::min(cores, kCap));
}

std::vector<ArmSpec> native_only_arms() {
    std::vector<ArmSpec> out;
    for (const ArmSpec &a : arms()) {
        if (a.seam == SeamId::kNative) {
            out.push_back(a);
        }
    }
    return out;
}

} // namespace hven::rig
