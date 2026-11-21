// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// QpEngine's named friend for hven_qp_walk_seam_tests (moved here from test_qp_dense_arm.cpp when a
// second TU, test_qp_platform_retake.cpp, needed it). Reached as
// "qp/support/qp_engine_test_access.h".

#include <gtest/gtest.h>

#include <hven/detail/qp/walk/eqp_solve.h>
#include <hven/detail/qp/walk/walk_engine.h>

namespace hven::detail {
// QpEngine's named friend, DEFINED here for hven_qp_seam_tests (test_walk_engine_indefinite.cpp
// defines its own in hven_qp_tests, a different executable): every private site a seam test reads
// directly. One definition for every TU of the executable (its member functions are inline).
struct QpEngineTestAccess {
    static bool terminal_mu_free(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                 Vec &x, Vec &lambda_e, Vec &lambda_i, Vec &z, QpCounters &counters,
                                 const QpOptions &opts) {
        return eng.refine_terminal_mu_free(qp, ws, x, lambda_e, lambda_i, z, counters, opts);
    }
    static bool terminal_polish(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                Vec &x, Vec &lambda_e, Vec &lambda_i, Vec &z, QpCounters &counters,
                                const QpOptions &opts) {
        const EqpResult none; // no pass answer: stage 1 cannot adopt, stage 2 solves
        return eng.polish_terminal_point(qp, ws, none, /*row_triggered=*/false, x, lambda_e,
                                         lambda_i, z, counters, opts);
    }
    static bool terminal_polish_with(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                     const EqpResult &answer, bool row_triggered, Vec &x,
                                     Vec &lambda_e, Vec &lambda_i, Vec &z, QpCounters &counters,
                                     const QpOptions &opts) {
        return eng.polish_terminal_point(qp, ws, answer, row_triggered, x, lambda_e, lambda_i, z,
                                         counters, opts);
    }
    // The polish's adoption test alone, on a candidate the caller supplies.
    static bool polish_would_adopt(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                   const EqpResult &cand, Vec x, Vec lambda_e, Vec lambda_i, Vec z,
                                   const QpOptions &opts, bool row_triggered = false) {
        const TerminalKkt before = terminal_kkt(qp, ws, x, lambda_e, lambda_i, z);
        return eng.adopt_polished_candidate(qp, ws, cand, before, opts, row_triggered, x, lambda_e,
                                            lambda_i, z);
    }
    static bool polish_footprint_open(const QpEngine &eng, const QpProblem &qp,
                                      const WorkingSet &ws, const Vec &x, const Vec &lambda_e,
                                      const Vec &lambda_i, const Vec &z, const QpOptions &opts,
                                      bool &row_triggered) {
        Vec ai_norm1 = Vec::Zero(qp.mi());
        Vec ae_norm1 = Vec::Zero(qp.me());
        for (Index j = 0; j < qp.mi(); ++j) {
            ai_norm1(j) = qp.Ai.row(j).cwiseAbs().sum();
        }
        for (Index j = 0; j < qp.me(); ++j) {
            ae_norm1(j) = qp.Ae.row(j).cwiseAbs().sum();
        }
        return eng.polish_footprint_open(qp, ws, x, lambda_e, lambda_i, z, ai_norm1, ae_norm1, opts,
                                         row_triggered);
    }
    // K0 for `ws` and its borders, as border_candidate builds them.
    static void build_border(const QpEngine &eng, const QpProblem &qp, const WorkingSet &k0_ws,
                             const WorkingSet &ws, BorderState &border, QpCounters &counters,
                             const QpOptions &opts) {
        eng.rebuild_k0(qp, k0_ws, border, counters, opts);
        eng.sync_borders(qp, ws, border, counters, opts);
    }
    // The two verdict-site face refinements, at an iterate with zero multipliers.
    static bool border_verdict_refine(const QpEngine &eng, const QpProblem &qp,
                                      const WorkingSet &ws, Vec &x, BorderState &border,
                                      QpCounters &counters, const QpOptions &opts) {
        const VerdictInputs in(qp, x);
        return eng.refine_face_for_verdict(qp, ws, x, in.aix, in.ai_norm, in.lambda_i, in.ae_norm,
                                           in.lambda_e, border, counters, opts);
    }
    static bool eliminated_verdict_refine(const QpEngine &eng, const QpProblem &qp,
                                          const WorkingSet &ws, Vec &x, QpCounters &counters,
                                          const QpOptions &opts) {
        const VerdictInputs in(qp, x);
        const KktFactor kkt(0, eng.dense_policy());
        EliminatedFace face; // armed but not holding `ws`: the twin factorizes its own
        face.factorized = true;
        return eng.refine_eliminated_face_for_verdict(qp, ws, x, in.aix, in.ai_norm, in.lambda_i,
                                                      in.ae_norm, in.lambda_e, kkt, face, counters,
                                                      opts);
    }

    // The verdict refinements' acceptance measure, at zero multipliers.
    static double face_measure(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                               const Vec &x, const QpOptions &opts) {
        const VerdictInputs in(qp, x);
        return eng.working_face_measure(qp, ws, x, in.aix, in.ai_norm, in.lambda_i, in.ae_norm,
                                        in.lambda_e, opts);
    }

    // THE PLATFORM-SOLVE RESPONSE's border path (kkt_factor.h): one pass's border solve, as
    // border_candidate hands it over (no rebuild spent yet this pass), and the latch's no-op test.
    static EqpResult border_solve(const QpEngine &eng, const QpProblem &qp, const WorkingSet &ws,
                                  const Vec &x_k, KktFactor &kkt, BorderState &border,
                                  QpCounters &counters, const QpOptions &opts) {
        EliminatedFace face;
        InertiaVerdict verdict = InertiaVerdict::kOk;
        return eng.border_solve_or_fall_back(qp, ws, x_k, kkt, face, border, counters, verdict,
                                             /*rebuilt=*/false, opts);
    }
    static bool rebuild_would_be_noop(const WorkingSet &ws, const BorderState &border) {
        return QpEngine::rebuild_would_be_noop(ws, border);
    }

  private:
    struct VerdictInputs {
        VerdictInputs(const QpProblem &qp, const Vec &x)
            : aix(qp.mi() > 0 ? Vec(qp.Ai * x) : Vec(0)), ai_norm(Vec::Zero(qp.mi())),
              lambda_i(Vec::Zero(qp.mi())), ae_norm(Vec::Zero(qp.me())),
              lambda_e(Vec::Zero(qp.me())) {
            for (Index j = 0; j < qp.mi(); ++j) {
                ai_norm(j) = qp.Ai.row(j).cwiseAbs().sum();
            }
            for (Index j = 0; j < qp.me(); ++j) {
                ae_norm(j) = qp.Ae.row(j).cwiseAbs().sum();
            }
        }
        Vec aix, ai_norm, lambda_i, ae_norm, lambda_e;
    };
};
} // namespace hven::detail
