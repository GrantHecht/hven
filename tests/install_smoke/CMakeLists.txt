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

add_executable(hven_install_smoke main.cpp)
target_link_libraries(hven_install_smoke PRIVATE hven::hven)
