#!/usr/bin/env python3
# B8: AlgorithmMode::kInit leaves the public surface. The public kSolve-phase option is SolvePhaseMode;
# the engine's internal AlgorithmMode (with kInit) moves to the implementation header.
import os
os.chdir('/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4')


def edit(p, old, new):
    s = open(p).read()
    assert old in s, (p, old[:60])
    open(p, 'w').write(s.replace(old, new, 1))


edit('include/hven/drivers/ipm_enums.h',
     '''/// @brief The algorithm mode a phase runs in.
enum class AlgorithmMode { kOpt, kOptNo, kSoe, kInit };''',
     '''/// @brief What an `IpmPhase::kSolve` phase iterates on.
enum class SolvePhaseMode {
    /// The optimality iteration, objective included.
    kOptimize,
    /// The optimality iteration with the objective dropped (scaled by 0).
    kOptimizeWithoutObjective,
    /// The feasibility system ("solve the equations"). The default.
    kSolveEquations,
};''')
edit('include/hven/drivers/ipm_options.h',
     '''    /// The algorithm mode of an `IpmPhase::kSolve` phase. Default `kSoe`.
    AlgorithmMode solve_phase_mode = AlgorithmMode::kSoe;''',
     '''    /// What an `IpmPhase::kSolve` phase iterates on. Default `kSolveEquations`.
    SolvePhaseMode solve_phase_mode = SolvePhaseMode::kSolveEquations;''')
edit('src/drivers/ipm_options.cpp',
     'enum_in_range(adv.solve_phase_mode, AlgorithmMode::kInit, "advanced.solve_phase_mode");',
     'enum_in_range(adv.solve_phase_mode, SolvePhaseMode::kSolveEquations, "advanced.solve_phase_mode");')
p = 'src/drivers/ipm_options.cpp'
s = open(p).read()
a = s.index('const char *enum_name(hven::AlgorithmMode v) {')
b = s.index('const char *enum_name(hven::BarrierMode v) {')
s = s[:a] + '''const char *enum_name(hven::SolvePhaseMode v) {
    using hven::SolvePhaseMode;
    switch (v) {
    case SolvePhaseMode::kOptimize:
        return "kOptimize";
    case SolvePhaseMode::kOptimizeWithoutObjective:
        return "kOptimizeWithoutObjective";
    case SolvePhaseMode::kSolveEquations:
        return "kSolveEquations";
    }
    return "unknown";
}
''' + s[b:]
open(p, 'w').write(s)
edit('include/hven/detail/drivers/ipm_solver_impl.h', 'namespace hven {\n', '''namespace hven {

/// The interior-point engine's internal iteration mode. A phase runs one of the
/// first three (`SolvePhaseMode` names them for a kSolve phase); kInit is the
/// multiplier initializer's one-shot evaluation, never a phase's mode.
enum class AlgorithmMode { kOpt, kOptNo, kSoe, kInit };
''')
edit('src/drivers/ipm_solver.cpp',
     '                opts_.advanced.solve_phase_mode, opts_.advanced.solve_barrier_mode,',
     '                to_algorithm_mode(opts_.advanced.solve_phase_mode), opts_.advanced.solve_barrier_mode,')
old = 'std::vector<hven::IpmSolverImpl::PhaseStep>\nhven::IpmSolverImpl::phase_steps() const {'
edit('src/drivers/ipm_solver.cpp', old, '''namespace {
// The public kSolve-phase mode as the engine's internal iteration mode.
hven::AlgorithmMode to_algorithm_mode(hven::SolvePhaseMode m) {
    switch (m) {
    case hven::SolvePhaseMode::kOptimize:
        return hven::AlgorithmMode::kOpt;
    case hven::SolvePhaseMode::kOptimizeWithoutObjective:
        return hven::AlgorithmMode::kOptNo;
    case hven::SolvePhaseMode::kSolveEquations:
        return hven::AlgorithmMode::kSoe;
    }
    return hven::AlgorithmMode::kSoe; // unreachable: validate() refuses an out-of-range value
}
} // namespace

''' + old)
edit('tests/drivers/test_options_dump.cpp', 'advanced.solve_phase_mode = kSoe', 'advanced.solve_phase_mode = kSolveEquations')
print('ok')
