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

# The standalone-include check. For every PUBLIC header of the installed package (everything under
# include/hven/ except detail/) one generated translation unit includes that header and nothing else,
# so a header that has stopped carrying its own transitive includes fails HERE, against a fresh
# install prefix through find_package(hven), the way a real consumer sees it -- rather than passing
# inside hven's own build where another include had already pulled the missing declaration in. The
# list is the installed tree itself, so a new public header is covered the day it is added; the
# in-tree twin for every header (detail/ included) is the HeaderSelfContained test of tests/.
# Compile-time only: the objects are empty and the smoke binary below is the link/run proof.
get_filename_component(_hven_prefix "${hven_DIR}/../../.." ABSOLUTE)
file(GLOB_RECURSE _hven_public_headers RELATIVE "${_hven_prefix}/include" CONFIGURE_DEPENDS
    "${_hven_prefix}/include/hven/*.h")
list(FILTER _hven_public_headers EXCLUDE REGEX "^hven/detail/")
list(SORT _hven_public_headers)
list(LENGTH _hven_public_headers _n_public)
if(_n_public LESS 40)
    message(FATAL_ERROR "install smoke: found only ${_n_public} public headers under "
        "${_hven_prefix}/include/hven; the install tree is wrong or the glob is.")
endif()
set(HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES "")
foreach(_h IN LISTS _hven_public_headers)
    string(REPLACE "/" "_" _stem "${_h}")
    set(_tu "${CMAKE_CURRENT_BINARY_DIR}/standalone/include_${_stem}.cpp")
    file(CONFIGURE OUTPUT "${_tu}" CONTENT "#include <${_h}>\n" @ONLY)
    list(APPEND HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES "${_tu}")
endforeach()
message(STATUS "install smoke: ${_n_public} public headers, one standalone TU each")

add_library(hven_install_smoke_standalone OBJECT ${HVEN_INSTALL_SMOKE_STANDALONE_INCLUDES})
target_link_libraries(hven_install_smoke_standalone PRIVATE hven::hven)

add_executable(hven_install_smoke main.cpp)
target_link_libraries(hven_install_smoke PRIVATE hven::hven)
add_dependencies(hven_install_smoke hven_install_smoke_standalone)

# No unity build and no precompiled header, stated rather than inherited: a unity build would
# concatenate the TUs and the first header to include <vector> would satisfy all the others --
# precisely the failure this target exists to catch. CMAKE_UNITY_BUILD is a cache variable a caller
# can set from the command line, so the OFF here has to be explicit.
set_target_properties(hven_install_smoke hven_install_smoke_standalone PROPERTIES
    UNITY_BUILD OFF
    DISABLE_PRECOMPILE_HEADERS ON)
