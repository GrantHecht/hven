#!/usr/bin/env bash
# M8 DN-0 IPM r1: CMAKE_CXX_COMPILER_LAUNCHER for the MEASUREMENT-ONLY hven build.
# Runs the compile command unchanged and appends -fasynchronous-unwind-tables LAST, overriding the
# library's -fno-asynchronous-unwind-tables (cmake/hven_compile_options.cmake). The flag adds CFI
# (.eh_frame) metadata only; code_identity.sh proves every executable section byte-identical.
exec "$@" -fasynchronous-unwind-tables
