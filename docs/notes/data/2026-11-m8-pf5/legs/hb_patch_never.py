#!/usr/bin/env python3
# PF-5 (../DECLARATION-PF-5.md): copy of ../../2026-11-m8-pf4/r2/legs/hb_patch_never.py re-pointed to the pf5b worktree (paths only); older lines are historical.
# PF-4 r2 (../../DECLARATION-PF-4-r2.md): copy of ../../legs/hb_patch_never.py re-pointed to the pf4-r2 worktree and r2/ (paths only); older lines are historical.
"""kopt-1x r2, PRIVATE CLONE ONLY (never pushed): adds the `never-<base>` label prefix to lanes/hven/hven_bench_cell.cpp
(DN-3 r4's hb_patch_j idea on the current lane): `never-<base>` is `<base>` with linear.dense_factorization = kNever.
usage: hb_patch_never.py <path to hven_bench_cell.cpp>"""
import sys
p = sys.argv[1]
s = open(p).read()
ipm_old = "hven::IpmOptions ipm_options(const std::string &label) {\n    hven::IpmOptions o{};"
ipm_new = ("hven::IpmOptions ipm_options(const std::string &label) {\n"
           "    if (label.rfind(\"never-\", 0) == 0) {\n"
           "        auto t = ipm_options(label.substr(6));\n"
           "        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;\n"
           "        return t;\n    }\n    hven::IpmOptions o{};")
sqp_old = "hven::SqpOptions sqp_options(const std::string &label) {\n    std::string base;"
sqp_new = ("hven::SqpOptions sqp_options(const std::string &label) {\n"
           "    if (label.rfind(\"never-\", 0) == 0) {\n"
           "        auto t = sqp_options(label.substr(6));\n"
           "        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;\n"
           "        return t;\n    }\n    std::string base;")
for old in (ipm_old, sqp_old):
    if s.count(old) != 1:
        sys.exit("PATCH_DOES_NOT_APPLY: " + old[:50])
s = s.replace(ipm_old, ipm_new).replace(sqp_old, sqp_new)
open(p, "w").write(s)
print("PATCHED")
