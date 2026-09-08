################################################################################
# Standalone install-tree smoke consumer for hven's install/export rules.
#
# NOT added via add_subdirectory() from hven's own root CMakeLists.txt -- it
# must be configured as its own top-level project, pointed at an hven install
# prefix via -Dhven_DIR=<prefix>/lib/cmake/hven (or CMAKE_PREFIX_PATH), the
# same way any real external consumer would use it. See
# scripts/check_install_smoke.sh, which drives the full
# "install hven -> configure/build/run this" cycle from a scratch prefix.
################################################################################

cmake_minimum_required(VERSION 3.24)
project(hven_install_smoke LANGUAGES CXX)

find_package(hven REQUIRED)

# The standalone-include TUs (M6 W5 T0). Each includes exactly ONE public hven
# header and nothing else, so a header that has stopped carrying its own
# transitive includes fails HERE -- against a fresh install prefix, through
# find_package(hven), the way a real consumer sees it -- rather than passing
# inside hven's own build where some other TU in the same unity blob, or hven's
# own include order, had already pulled the missing declaration in.
#
# That is exactly the property W5 relies on: T4 MOVED the trace sink into a
# public drivers/ header and split the evidence structs out of the engine
# header, and T8 renames the four model-layer headers listed below. Both are
# public-surface changes whose failure mode is "compiles in-tree, does not
# compile for a consumer".
set(HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES
    include_trace_writer.cpp
    include_trace.cpp
    include_ipqp_evidence.cpp
    include_nlp_problem.cpp
    include_nlp_aggregate.cpp
    include_aggregate_declaration.cpp
    include_nlp_model_aggregate.cpp
    include_nlp_solver.cpp
    include_solve_status.cpp
    include_compiler.cpp)

add_executable(hven_install_smoke main.cpp ${HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES})
target_link_libraries(hven_install_smoke PRIVATE hven::hven)

# No unity build and no precompiled header, stated rather than inherited. A
# unity build would concatenate the ten TUs above into one, and the first
# header to include <vector> would then satisfy all the others -- which is
# precisely the failure this target exists to catch. CMAKE_UNITY_BUILD is a
# cache variable a caller can set from the command line (hven's own test
# targets do use unity builds), so the OFF here has to be explicit.
set_target_properties(hven_install_smoke PROPERTIES
    UNITY_BUILD OFF
    DISABLE_PRECOMPILE_HEADERS ON)
