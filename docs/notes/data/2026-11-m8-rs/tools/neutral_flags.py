#!/usr/bin/env python3
"""Print the compiler flags that neutralise `__FILE__` for the RS-2 P-SYM arms.

Why.  Every gtest TEST registers through a dynamic initialiser that copies `__FILE__` into a std::string; the copy is expanded inline, so moving a test source to a
directory whose name has a different length changes displacements -- and sometimes the NUMBER of instructions -- in that function, in every moved test object (run 1:
70 objects).  P-SYM would then report real-looking differences that are only the length of a path.  So both arms are compiled with the SAME flag list that maps every test
directory prefix (the union of the directories of both trees) to the empty string, making `__FILE__` the bare file name in both arms, plus one exact-file map for each
of the three S6 renames so the renamed file's `__FILE__` is the same token in both arms (clang takes the longest matching prefix).  The flags are identical in the two
arms by construction (same list, computed once), so they cannot themselves create a difference; they only remove the one path-length dependence.
usage: neutral_flags.py ROOT REV_BEFORE REV_AFTER     -> one line of flags on stdout"""
import subprocess, sys
root, rb, ra = sys.argv[1].rstrip('/'), sys.argv[2], sys.argv[3]
dirs = set()
for rev in (rb, ra):
    out = subprocess.run(['git', '-C', root, 'ls-tree', '-r', '-d', '--name-only', rev, 'tests'], capture_output=True, text=True, check=True).stdout
    dirs |= {d for d in out.split('\n') if d}
dirs.add('tests')
flags = ['-fmacro-prefix-map=%s/%s/=' % (root, d) for d in sorted(dirs)]
renames = [('tests/drivers/test_solve_status.cpp', 'tests/ipm/test_ipm_phase_status.cpp'),
           ('tests/sqp/test_types.cpp', 'tests/qp/test_qp_types.cpp'),
           ('tests/sqp/test_scaffold.cpp', 'tests/linear/test_backend_scaffold.cpp')]
for i, (o, n) in enumerate(renames):
    flags.append('-fmacro-prefix-map=%s/%s=RS2_RENAMED_%d' % (root, o, i))
    flags.append('-fmacro-prefix-map=%s/%s=RS2_RENAMED_%d' % (root, n, i))
print(' '.join(flags))
