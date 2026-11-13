#!/usr/bin/env python3
# M8 DN-3 r4 (../../DECLARATION-r4.md): applied AFTER ../../sl/hb_patch_g.py, hb_patch_h.py and hb_patch_i.py (DN-3's
# lane, local clone branch dn3-legs) to lanes/hven/hven_bench_cell.cpp. Nothing else moves:
#   label prefix `never-<base>`   linear.dense_factorization = kNever on both engines, set EXPLICITLY (peeled beside
#                                 `auto-`); `never-default` is r4's kNever-explicit arm, since after the flip the lane's
#                                 `default` label (which sets no dense policy) reads the library default kAuto.
# usage: hb_patch_j.py <lane cpp>   Fail closed: each anchor once.
import sys

p = sys.argv[1]
s = open(p).read()


def once(old, new, tag):
    global s
    if s.count(old) != 1:
        sys.exit(f'HB_PATCH_J_{tag} {s.count(old)}x')
    s = s.replace(old, new)


once('constexpr const char *kDn3AutoPrefix = "auto-";\n',
     'constexpr const char *kDn3AutoPrefix = "auto-";\n'
     '/// M8 DN-3 r4, LOCAL EVIDENCE CLONE ONLY: `never-<base>` sets linear.dense_factorization = kNever.\n'
     'constexpr const char *kDn3NeverPrefix = "never-";\n', 'PREFIX')
for eng in ('ipm', 'sqp'):
    once(f'''    if (label.rfind(kDn3AutoPrefix, 0) == 0) {{
        auto t = {eng}_options(label.substr(std::strlen(kDn3AutoPrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAuto;
        return t;
    }}
''', f'''    if (label.rfind(kDn3AutoPrefix, 0) == 0) {{
        auto t = {eng}_options(label.substr(std::strlen(kDn3AutoPrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kAuto;
        return t;
    }}
    if (label.rfind(kDn3NeverPrefix, 0) == 0) {{
        auto t = {eng}_options(label.substr(std::strlen(kDn3NeverPrefix)));
        t.linear.dense_factorization = hven::linear::DensePolicy::kNever;
        return t;
    }}
''', f'{eng.upper()}_LABEL')
open(p, 'w').write(s)
print('HB_PATCH_J_OK')
