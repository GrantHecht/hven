#!/usr/bin/env python3
import os
os.chdir('/home/ghecht/Projects/hven/.claude/worktrees/agent-aad152e17d08967f4/tests/install_smoke')
HDR = '''// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// One hven header, alone, no unity build, no PCH: if it does not carry its own
// transitive includes, this TU does not compile. Why, and which W5 tasks depend
// on it: this directory's CMakeLists.txt.

#include "hven/{h}"

namespace hven_install_smoke {{
int standalone_include_{n}() {{ return 1; }}
}} // namespace hven_install_smoke
'''
open('include_hven.cpp', 'w').write(HDR.format(h='hven.h', n='hven'))
open('include_ipm_iterate.cpp', 'w').write(HDR.format(h='drivers/ipm_iterate.h', n='ipm_iterate'))


def edit(p, old, new):
    s = open(p).read()
    assert s.count(old) == 1, (p, old)
    open(p, 'w').write(s.replace(old, new))


edit('CMakeLists.txt', '''    include_type_storage.cpp)''', '''    include_type_storage.cpp
    include_ipm_iterate.cpp
    include_hven.cpp)''')
edit('CMakeLists.txt', '''set(HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES''', '''#
# M8 AP-2 B8: three raw-protocol headers took the names of what they declare
# (solver_indexing_data, sizable_spec, piece_interfaces), and the umbrella
# hven/hven.h and drivers/ipm_iterate.h joined, 35 -> 37.
set(HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES''')
edit('main.cpp', 'int standalone_include_type_storage();', '''int standalone_include_type_storage();
int standalone_include_ipm_iterate();
int standalone_include_hven();''')
edit('main.cpp', '''                               hven_install_smoke::standalone_include_type_storage();
    if (standalone_tus != 35) {
        std::fprintf(stderr, "install smoke: %d standalone-include TUs linked, expected 35\\n",''',
     '''                               hven_install_smoke::standalone_include_type_storage() +
                               hven_install_smoke::standalone_include_ipm_iterate() +
                               hven_install_smoke::standalone_include_hven();
    if (standalone_tus != 37) {
        std::fprintf(stderr, "install smoke: %d standalone-include TUs linked, expected 37\\n",''')
print('ok')
