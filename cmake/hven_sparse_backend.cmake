################################################################################
# Migrated from the origin project; flags kept identical.
#
# hven_resolve_sparse_backend()
#
# Resolves the platform sparse-linear-algebra backend: Apple Accelerate
# (AccelerateSparse) on macOS, Intel MKL everywhere else. Populates
# USE_ACCELERATE_SPARSE and the AccelerateSparse::AccelerateSparse imported
# target on Apple, or the MKL_* variables plus INCLUDE_DIRS/LINK_LIBS
# elsewhere.
#
# Implemented as a macro, not a function: find_package(AccelerateSparse) /
# find_package(MKL) create imported targets and set variables that must land
# in the caller's directory scope. A function() would confine any imported
# target it creates to a function-local scope that disappears on return,
# breaking target_link_libraries() calls elsewhere that expect
# AccelerateSparse::AccelerateSparse (or the MKL_LIBRARIES/LINK_LIBS
# variables) to still exist.
#
# Migrated from the origin project's sparse-backend module -- no behavior
# changes.
#
# Prerequisites the caller must set before invoking (all read from caller
# scope):
#
#   CMAKE_MODULE_PATH   must contain hven's cmake/ directory, which supplies
#                       FindMKL.cmake and FindAccelerateSparse.cmake.
#   Threads             find_package(Threads) must already have run; LINK_LIBS
#                       is assembled with Threads::Threads in it.
#   ENABLE_PYTHON_BINDINGS
#                       ON also resolves Python (Interpreter + Development).
#                       Leave unset in a build with no Python surface.
#   PYVERSION_EXACT     optional version argument forwarded to
#                       find_package(Python); only read when
#                       ENABLE_PYTHON_BINDINGS is ON.
#
# Sets in caller scope: INCLUDE_DIRS (MKL headers) and LINK_LIBS (the sparse
# backend plus Threads and dl), and on Apple the USE_ACCELERATE_SPARSE,
# ACCELERATE_NEW_LAPACK, and HVEN_USE_ACCELERATE_LAPACK compile definitions
# and the AccelerateSparse::AccelerateSparse target.
#
# HVEN_USE_ACCELERATE_LAPACK is a separate name from USE_ACCELERATE_SPARSE
# on purpose, even though this macro currently sets both under the same
# `if(APPLE)` branch: USE_ACCELERATE_SPARSE answers "which sparse backend",
# HVEN_USE_ACCELERATE_LAPACK answers "which dense LAPACKE header" (see
# dense_symmetric_factor.cpp, the one place that reads it). They are the
# same fact today only because both questions currently have the same
# Apple/not-Apple answer; naming them separately means a future option that
# decouples the two (e.g. forcing MKL's LAPACKE on Apple while keeping
# AccelerateSparse for the sparse solve) only has to change where this one
# is set, not every call site that reads it.
################################################################################

macro(hven_resolve_sparse_backend)

if(APPLE)
  find_package(AccelerateSparse REQUIRED)
  # ACCELERATE_NEW_LAPACK selects Apple's non-deprecated, const-correct f77
  # LAPACK declarations. Defined globally (not per-header) so
  # lapacke_shim.h and every other Accelerate.h consumer agree in every TU
  # regardless of include order -- mirrors the identical define in the
  # sibling SQP engine's own CMakeLists.txt, where lapacke_shim.h
  # originated. HVEN_USE_ACCELERATE_LAPACK is this macro's own name for
  # "use the Accelerate LAPACKE shim for dense routing" -- see the doc
  # comment above for why it is a distinct macro from USE_ACCELERATE_SPARSE
  # rather than reusing it.
  add_compile_definitions(USE_ACCELERATE_SPARSE ACCELERATE_NEW_LAPACK HVEN_USE_ACCELERATE_LAPACK)
else()
  find_package(MKL)
  if(NOT MKL_FOUND)
      message(FATAL_ERROR
          "hven requires Intel MKL. Set MKLROOT to the MKL installation directory.\n"
          "  See CLAUDE.md for installation instructions.")
  endif()
  # Validate that no individual MKL library is NOTFOUND. FindMKL.cmake does not
  # include MKL_OMP_LIBRARY in FIND_PACKAGE_HANDLE_STANDARD_ARGS, so MKL_FOUND
  # can be TRUE while MKL_LIBRARIES contains a NOTFOUND entry (libiomp5).
  foreach(_mkl_lib IN LISTS MKL_LIBRARIES)
      if("${_mkl_lib}" MATCHES "NOTFOUND")
          message(FATAL_ERROR
              "MKL library component not found: ${_mkl_lib}\n"
              "Your MKL installation may be incomplete. "
              "Ensure MKLROOT points to a complete MKL installation.")
      endif()
  endforeach()
endif()

if(ENABLE_PYTHON_BINDINGS)
    find_package(Python ${PYVERSION_EXACT} REQUIRED COMPONENTS Interpreter Development)
endif()

# Set dependency variables
set(INCLUDE_DIRS ${MKL_INCLUDE_DIRS})
# On Linux, static MKL archives have circular dependencies and must be wrapped
# in --start-group/--end-group so the linker rescans them.
if(UNIX AND NOT APPLE)
  set(LINK_LIBS -Wl,--start-group ${MKL_LIBRARIES} -Wl,--end-group Threads::Threads ${CMAKE_DL_LIBS})
else()
  set(LINK_LIBS ${MKL_LIBRARIES} Threads::Threads ${CMAKE_DL_LIBS})
endif()

endmacro()
