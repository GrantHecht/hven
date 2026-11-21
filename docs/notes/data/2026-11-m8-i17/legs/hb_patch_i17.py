#!/usr/bin/env python3
"""I17, PRIVATE CLONE ONLY (never pushed): hb_patch_never.py's `never-<base>` plus two hven-sqp label prefixes, so the
TRAIN lane can run the Always-forced configuration of DECLARATION-I17.md:
  always-<base>   <base> with column_scaling = kAlways
  cap<K>-<base>   <base> with column_scaling_factor_max = 2^K   (arm B only; arm A has no such option: pass --no-cap)
Prefixes nest (`cap16-always-default`). usage: hb_patch_i17.py <hven_bench_cell.cpp> [--no-cap]"""
import sys
p = sys.argv[1]
with_cap = "--no-cap" not in sys.argv[2:]
s = open(p).read()
ipm_old = "hven::IpmOptions ipm_options(const std::string &label) {\n    hven::IpmOptions o{};"
ipm_new = ("hven::IpmOptions ipm_options(const std::string &label) {\n"
           "    if (label.rfind(\"never-\", 0) == 0) {\n"
           "        auto t = ipm_options(label.substr(6));\n"
           "        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;\n"
           "        return t;\n    }\n    hven::IpmOptions o{};")
sqp_old = "hven::SqpOptions sqp_options(const std::string &label) {\n    std::string base;"
cap = ""
if with_cap:
    cap = ("    if (label.rfind(\"cap\", 0) == 0 && label.find('-') != std::string::npos) {\n"
           "        const auto dash = label.find('-');\n"
           "        auto t = sqp_options(label.substr(dash + 1));\n"
           "        t.column_scaling_factor_max = std::ldexp(1.0, std::stoi(label.substr(3, dash - 3)));\n"
           "        return t;\n    }\n")
sqp_new = ("hven::SqpOptions sqp_options(const std::string &label) {\n"
           "    if (label.rfind(\"never-\", 0) == 0) {\n"
           "        auto t = sqp_options(label.substr(6));\n"
           "        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;\n"
           "        return t;\n    }\n"
           "    if (label.rfind(\"always-\", 0) == 0) {\n"
           "        auto t = sqp_options(label.substr(7));\n"
           "        t.column_scaling = hven::SqpColumnScaling::kAlways;\n"
           "        return t;\n    }\n" + cap + "    std::string base;")
for old in (ipm_old, sqp_old):
    if s.count(old) != 1:
        sys.exit("PATCH_DOES_NOT_APPLY: " + old[:50])
s = s.replace(ipm_old, ipm_new).replace(sqp_old, sqp_new)
if "#include <cmath>" not in s:
    s = s.replace("#include <cstdio>", "#include <cmath>\n#include <cstdio>", 1)
open(p, "w").write(s)
print("PATCHED")
