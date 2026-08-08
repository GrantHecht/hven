// Minimal STUB, not part of hven's own Accelerate surface: some `__APPLE__`
// code this repository does NOT own (googletest's gtest-port.h) includes
// this real macOS SDK header unconditionally whenever `__APPLE__` is
// defined, purely to detect the host OS -- nothing in hven's own code or in
// the gtest paths this repository actually exercises reads a macro this
// header would define. An empty stub is therefore sufficient to let
// `-D__APPLE__` syntax-checks of the two Apple-gated test files
// (tests/linear/test_fault_injection.cpp,
// tests/linear/test_symmetric_factor_evidence_invariants.cpp;
// scripts/check_accelerate_syntax_linux.sh) get past googletest's own
// platform-detection headers on Linux. See that script's header comment for
// the exact claim ceiling this supports.
#pragma once
