#!/usr/bin/env python3
# M8 sparse-equil (../DECLARATION.md): applied to hven-bench main's lanes/hven/hven_bench_cell.cpp in a PRIVATE clone
# (.scratch/hb, never pushed). Nothing else moves:
#   label prefix `never-<base>`   linear.dense_factorization = kNever on both engines, set EXPLICITLY (DN-3 r4's patch j, same
#                                 semantics: after the DN-3 flip the lane's `default` label reads the library default kAuto).
#   three SQP side columns after add_e4_counts: side.se_sparse_equilibrated_refactors, side.se_dense_shifted_solve_refusals,
#                                 side.se_dense_shifted_terminations (SqpCounters, summed over the solve).
# usage: hb_patch_se.py <lane cpp> [A]   Fail closed: each anchor once. Argument A (arm A's library has no
# sparse_equilibrated_refactors): the first column is omitted.
import sys

p = sys.argv[1]
s = open(p).read()


def once(old, new, tag):
    global s
    if s.count(old) != 1:
        sys.exit(f'HB_PATCH_SE_{tag} {s.count(old)}x')
    s = s.replace(old, new)


once('hven::IpmOptions ipm_options(const std::string &label) {\n',
     'hven::IpmOptions ipm_options(const std::string &label) {\n'
     '    if (label.rfind("never-", 0) == 0) {\n'
     '        auto t = ipm_options(label.substr(6));\n'
     '        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;\n'
     '        return t;\n'
     '    }\n', 'IPM')
once('hven::SqpOptions sqp_options(const std::string &label) {\n',
     'hven::SqpOptions sqp_options(const std::string &label) {\n'
     '    if (label.rfind("never-", 0) == 0) {\n'
     '        auto t = sqp_options(label.substr(6));\n'
     '        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;\n'
     '        return t;\n'
     '    }\n', 'SQP')
arm_a = len(sys.argv) > 2 and sys.argv[2] == 'A'
once('    add_e4_counts(report, c);\n',
     '    add_e4_counts(report, c);\n'
     + ('' if arm_a else '    add_side(report, "se_sparse_equilibrated_refactors",\n'
        '             static_cast<long long>(c.sparse_equilibrated_refactors));\n') +
     '    add_side(report, "se_dense_shifted_solve_refusals",\n'
     '             static_cast<long long>(c.dense_shifted_solve_refusals));\n'
     '    add_side(report, "se_dense_shifted_terminations",\n'
     '             static_cast<long long>(c.dense_shifted_terminations));\n', 'COLS')
open(p, 'w').write(s)
print('HB_PATCH_SE_OK')
