// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/switching_acceptance.h"

namespace hven::solvers {

/// (18a) constraint-violation margin gamma_theta (WB Eq. (18a); Ipopt option
/// "gamma_theta" = 1e-5).
inline constexpr double kFilterGammaTheta = 1.0e-5;
/// (18b) barrier-objective margin gamma_phi (WB Eq. (18b); Ipopt option
/// "gamma_phi" = 1e-8).
inline constexpr double kFilterGammaPhi = 1.0e-8;
/// Barrier-objective ceiling: reject a trial whose barrier objective grows by
/// more than this many orders of magnitude (log10-scaled comparison, not a
/// ratio — see the H-type verdict below); Ipopt option "obj_max_inc" = 5.0.
inline constexpr double kFilterObjMaxInc = 5.0;
/// Filter-reset heuristic: clear the filter after this many successive
/// filter-caused rejections (Ipopt option "filter_reset_trigger" = 5).
inline constexpr int kFilterResetTrigger = 5;
/// Filter-reset heuristic: cap on the number of clears per phase (Ipopt option
/// "max_filter_resets" = 5).
inline constexpr int kFilterMaxResets = 5;

/// @brief The (theta, phi)-pair set value type: a plain vector of margined
/// entries with per-coordinate dominance semantics (reference-filter shape;
/// no capacity scheme until profiling demands one). Entries store the
/// ALREADY-margined pair so acceptability is a plain dominance check.
class Filter {
  public:
    /// Acceptable w.r.t. every entry: an entry (phi_j, theta_j) blocks the
    /// trial only when phi > phi_j AND theta > theta_j (the negation of the
    /// per-coordinate <=).
    bool acceptable(double phi, double theta) const {
        for (const Entry &e : entries_) {
            if (phi > e.phi && theta > e.theta)
                return false;
        }
        return true;
    }

    /// Adds the margined pair for the CURRENT (reference) iterate's (phi,
    /// theta) and prunes every entry it dominates ((phi_j, theta_j) is
    /// dominated iff phi_add <= phi_j AND theta_add <= theta_j).
    void augment(double phi_current, double theta_current) {
        const double phi_add = phi_current - kFilterGammaPhi * theta_current;
        const double theta_add = (1.0 - kFilterGammaTheta) * theta_current;
        std::erase_if(entries_, [&](const Entry &e) {
            return phi_add <= e.phi && theta_add <= e.theta;
        });
        entries_.push_back({phi_add, theta_add});
    }

    void clear() { entries_.clear(); }

    std::size_t size() const { return entries_.size(); }

    /// Diagnostics (unit tests): the i-th stored margined (phi, theta) pair.
    std::pair<double, double> entry(std::size_t i) const {
        return {entries_[i].phi, entries_[i].theta};
    }

  private:
    struct Entry {
        double phi;
        double theta;
    };
    std::vector<Entry> entries_;
};

/// @brief Concrete Wächter–Biegler (theta, phi)-pair FILTER strategy on the
/// shared switching skeleton. Where the funnel collapses acceptance history
/// into ONE scalar width, the filter keeps a SET of dominating pairs: a trial
/// is acceptable only if not dominated (within a margin) by any stored pair.
/// This class supplies ONLY the filter-specific H-type verdict and the
/// augmentation bookkeeping through the base's hooks; the theta_min/theta_max
/// ceiling, switching condition, and F-type Armijo test live in
/// SwitchingAcceptance. Opt-in via Settings::acceptance_strategy_ == filter;
/// the default classic path stays bit-identical.
///
/// ProgressMeasures mapping (same convention as switching_acceptance.h):
/// current/trial .infeasibility = theta at z_k / z_k + alpha*d;
/// phi(pt) = pt.objective + pt.auxiliary = the barrier objective.
///
/// Verdicts:
///  - H-type progress (acceptable to current iterate), two parts:
///      (i) barrier-objective CEILING — reject when
///          log10(phi_trial - phi_ref) > kFilterObjMaxInc + log10(|phi_ref|)
///          (only evaluated when phi_trial > phi_ref): a LOG10-scaled growth
///          test, deliberately NOT a ratio.
///      (ii) two-condition margin (WB Eqs. (18a)/(18b)):
///          theta_trial <= (1-gamma_theta)*theta_ref  OR
///          phi_trial <= phi_ref - gamma_phi*theta_ref.
///          BOTH margins scale by theta_ref. Plain <= is used where the
///          reference applies a machine-epsilon-scaled comparison (affects only
///          trials within a few ULP of a margin; matches the paper's stated form).
///  - MEMBERSHIP (every trial, F-type included): acceptable w.r.t. EVERY stored
///    entry (per-coordinate <=); no margin is re-applied here — margins are
///    baked into stored pairs at augmentation time.
///
/// Augmentation: ONLY on an accepted H-TYPE step (an F-type Armijo accept never
/// augments, matching the paper/reference). The stored pair is the CURRENT
/// iterate's, pushed IN from both margins:
///     phi_add = phi_cur - gamma_phi*theta_cur ; theta_add = (1-gamma_theta)*theta_cur.
///
/// Reset heuristic (per-iteration granularity, reference-transcribed): the last
/// rejection's CAUSE is recorded via notify_trial_rejected (filter-caused only
/// when the membership test failed while the speculative T1 passed; unchanged
/// by a ceiling rejection); on each ACCEPT a filter-caused last rejection
/// advances the successive counter (clearing the filter at kFilterResetTrigger),
/// anything else zeroes it. Because the counter advances once per accept, a
/// line search backtracking through N filter-blocked trials counts ONE. The
/// clear count is capped at kFilterMaxResets per phase — this implementation
/// increments and HONORS the cap where the reference's shipped code leaves its
/// counter unincremented and the cap unenforced.
///
/// Feasibility-restoration hooks:
///  - Entry: augment the optimality filter with the ENTRY pair, STASH the whole
///    optimality working state, reinitialize FRESH working state (empty filter,
///    zeroed counters), re-arm the lazy theta_0 init, set the flag.
///  - Exit test: theta_trial <= max(kKappaResto*theta_ref, injected tolerance
///    floor) AND acceptable to the PRESERVED optimality filter AND acceptable
///    w.r.t. the preserved entry pair (margined like the live test).
///  - Exit: restore the stash and augment the restored filter with the EXIT
///    pair; clear the flag (the stash itself is left as a harmless leftover,
///    dropped by the next outside-phase reset()).
///  - mu-event invariant: while the flag is set, reset() clears WORKING state
///    only — the stashed optimality filter survives so a mid-restoration reset
///    cannot destroy what the exit test consults. Outside the phase, full clear
///    plus defensive stash drop. The injected tolerance is configuration, not
///    working state, and survives reset().
///
/// Cross-phase disclosure (why a stash at all): the proximal mode-switch
/// substitutes the PROXIMAL objective into the live objective measure during
/// feasibility, making an optimality-phase pair and a feasibility-phase pair
/// INCOMPARABLE — so the feasibility phase runs a fresh filter while the
/// optimality filter is preserved aside, consulted only by the exit test (the
/// reference implementation's own architecture, implemented inside one strategy
/// object rather than two instances).
///
/// Disclosed divergences (consequences stated):
///  - Exit-floor tolerance injection: no SolverContext is held, so the floor
///    tolerance is a member defaulting to Settings' econ_tol_ default and
///    injected from the live setting by the solver seam at every build; the
///    polymorphic interface is unchanged (concrete-class setter).
///  - Base theta_min/theta_max are NOT byte-stashed (they live privately in
///    the base): entry re-arms the lazy init; exit restores the filter directly
///    but deliberately leaves the init armed-as-initialized — re-arming there
///    would clear the just-restored filter. Consequence: after exit the ceiling
///    retains feasibility-phase values until the next phase-boundary reset —
///    LOOSER than a byte-restore (feasibility theta_0 is the high entry
///    infeasibility), affecting only the heuristic scalars, never membership or
///    dominance, and self-healing at the next mu-event/reset.
class FilterAcceptance final : public SwitchingAcceptance {
  public:
    /// Number of stored filter entries (diagnostics + unit tests). Zero before
    /// the first accepted H-type step and after any reset.
    std::size_t filter_size() const { return filter_.size(); }

    /// Diagnostics (unit tests): the i-th stored margined (phi, theta) pair.
    std::pair<double, double> filter_entry(std::size_t i) const { return filter_.entry(i); }

    /// Diagnostics (unit tests): reset-heuristic counters.
    int successive_filter_rejections() const { return successive_filter_rejections_; }
    int filter_resets() const { return n_filter_resets_; }

    /// Reports filter_size() into SolveResult::last_filter_size_ and the
    /// per-phase reset total into SolveResult::last_filter_resets_.
    void append_diagnostics(InteriorPointSolver::SolveResult &result) const override;

    /// Restoration-exit test: relative theta-reduction floor AND acceptable to
    /// the preserved (stashed) optimality filter AND acceptable w.r.t. the
    /// preserved entry pair.
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &reference,
                                               const ProgressMeasures &trial) const override;

    /// Entry hook (see the restoration description above).
    void notify_switch_to_feasibility(const ProgressMeasures &current_progress) override;

    /// Exit hook (see above).
    void notify_switch_to_optimality(const ProgressMeasures &current_progress) override;

    /// Injects the constraint-violation tolerance used as the exit floor.
    /// Concrete-class method (NOT on the polymorphic interface); the solver
    /// seam calls it with the live econ_tol_. Defaults to that field's own
    /// default until set.
    void set_restoration_constraint_tol(double tol) { restoration_constraint_tol_ = tol; }

    /// Restoration diagnostics (unit tests).
    double restoration_constraint_tol() const { return restoration_constraint_tol_; }
    bool in_feasibility_phase() const { return in_feasibility_phase_; }
    std::size_t stashed_filter_size() const { return stashed_filter_.size(); }
    std::pair<double, double> stashed_filter_entry(std::size_t i) const {
        return stashed_filter_.entry(i);
    }

  protected:
    /// Starts the phase with an empty filter (no theta_max seed).
    void initialize_bounds(double theta_0) override;

    /// Empties the filter and zeroes the reset-heuristic state.
    void reset_bounds() override;

    /// Membership: acceptable to the current filter (every trial).
    bool is_trial_acceptable_to_strategy(const ProgressMeasures &current,
                                         const ProgressMeasures &trial) override;

    /// H-type sufficient progress: acceptable to the current iterate.
    bool is_h_type_progress_acceptable(const ProgressMeasures &current,
                                       const ProgressMeasures &trial) override;

    /// Augments on an H-type accept; runs the reset heuristic on any accept.
    void register_accepted_step(const ProgressMeasures &current, const ProgressMeasures &trial,
                                bool h_type) override;

    /// Records the last rejection's cause for the reset heuristic.
    /// trial_passed_progress_test is the base's speculative T1 result,
    /// meaningful only when cause == kMembership.
    void notify_trial_rejected(RejectionCause cause, bool trial_passed_progress_test) override;

  private:
    /// Acceptable-to-current-iterate: the barrier-objective ceiling AND the
    /// two-condition margin test.
    static bool is_acceptable_to_current(double phi_trial, double theta_trial, double phi_current,
                                         double theta_current);

    Filter filter_;

    /// Reset-heuristic state; all zeroed by reset_bounds().
    int successive_filter_rejections_ = 0;   
    int n_filter_resets_ = 0;                
    bool last_rejection_was_filter_ = false; 

    // Feasibility-restoration state: the PRESERVED optimality-phase working
    // state, stashed at entry and restored at exit; the exit test consults
    // stashed_filter_.
    Filter stashed_filter_;
    int stashed_successive_filter_rejections_ = 0;
    int stashed_n_filter_resets_ = 0;
    bool stashed_last_rejection_was_filter_ = false;

    /// Set at entry, cleared at exit; makes reset() phase-aware (a mid-phase
    /// mu-event must preserve the stash and this flag).
    bool in_feasibility_phase_ = false;

    /// Injected exit-floor tolerance; derived from Settings' own default (not a
    /// duplicated literal) so this member and econ_tol_ can never silently
    /// drift apart. The seam seeds it from the live setting every solve;
    /// standalone/unit-test construction bypassing that seam observes the
    /// default.
    double restoration_constraint_tol_ = InteriorPointSolver::Settings{}.econ_tol_;
};

} // namespace hven::solvers
