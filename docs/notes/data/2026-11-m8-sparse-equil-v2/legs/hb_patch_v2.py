#!/usr/bin/env python3
# M8 sparse-equil-v2: applied to hven-bench main's lanes/hven/hven_bench_cell.cpp in a PRIVATE clone (.scratch/hb, never pushed).
# Copied from ../../2026-11-m8-sparse-equil/legs/hb_patch_se.py with its column hunk dropped: since hven-bench TR-E every
# SqpCounters field reaches the side file through hven's dump_counters, so `sparse_matched_refactors` arrives as a side column
# on the arm whose library has it. One thing moves:
#   label prefix `never-<base>`   linear.dense_factorization = kNever on both engines, set EXPLICITLY (DN-3 r4's patch j).
# usage: hb_patch_v2.py <lane cpp>   Fail closed: each anchor once.
import sys

p = sys.argv[1]
s = open(p).read()


def once(old, new, tag):
    global s
    if s.count(old) != 1:
        sys.exit(f'HB_PATCH_V2_{tag} {s.count(old)}x')
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
open(p, 'w').write(s)
print('HB_PATCH_V2_OK')
