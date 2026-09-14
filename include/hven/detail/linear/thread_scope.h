// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The per-call thread scope both backend paths bracket their calls with.
//
// WHY IT LIVES HERE AND NOT WHERE IT WAS WRITTEN (M6 W5 T8.8). This class was
// written in `src/linear/pardiso_session.cpp`, which is MPL-2.0 *and* Intel
// BSD-3-Clause derived from Eigen's PardisoSupport module. It is hven's own
// code: it appears in no upstream module, and it is named in no line of that
// file's derived-material list in `notices/eigen-mpl2.txt` (which is the Pardiso
// CALL DISCIPLINE -- the argument block, the phase sequence, the parameter-array
// value set, and the error-code handling). Lifting it out therefore SHRINKS the
// derived file's diff against its upstream, which is the direction CLAUDE.md
// section 6 prefers, and it needs no deviation record: nothing is added to a
// derived file, and no test seam is involved.
//
// The second reason is functional: the dense border factor
// (`linear/dense_symmetric_factor.h`) needs exactly the same bracket around its
// two LAPACK calls, and one implementation of "apply for this call, put back
// what was there" is better than two.
//
// WHAT IT IS NOT. It is not a process-wide setting: the override it applies is
// THREAD-LOCAL, which is what lets N solver instances on N threads run without
// seeing each other's count -- `docs/concurrency.md` is that contract. This
// comment used to name a second such scope, `jet.h`'s `MklLocalPinGuard`; that
// header was DELETED with the owner's jet ruling (2026-09-11) and is gone.

#if !defined(HVEN_USE_ACCELERATE_LAPACK)
#include <mkl_service.h>
#endif

namespace hven::linear::detail {

// Applies a per-instance thread count for the duration of one backend call.
//
// mkl_set_num_threads_local sets a THREAD-LOCAL override: an hven instance
// configured for two threads must not reach out and change the thread count of
// every other MKL user in the process.
//
// SAVE AND RESTORE, not restore-to-zero. The setter RETURNS the override that
// was in force on this thread (0 when there was none, meaning "follow MKL's
// global setting"), and that returned value is what the destructor puts back.
// Restoring a hardcoded 0 instead would silently lose a caller's own
// pre-existing local override -- surfacing far away, as somebody else's solve
// running at the wrong width. Undoing exactly what was done is the contract
// this scope exists to keep.
//
// RESTORATION ON AN EXCEPTIONAL EXIT IS RAII, i.e. by construction. The
// backend calls this brackets are C entry points and cannot throw, and no
// statement sits between a scope's construction and the call it guards -- so no
// SOLVE in this tree can force a throw INSIDE the scope. The destructor's
// restore on an unwind is measured anyway, AT THIS CLASS, since M6 W6 T4:
// `tests/linear/test_fault_injection.cpp`'s
// `ThreadScope.TheCallersOverrideIsRestoredWhenAThrowUnwindsTheScope` throws
// inside a scope and catches outside it. The session throw seam once registered
// for that job was declined and RETIRED -- `docs/testing.md`'s dated W6 note.
//
// ON APPLE THIS IS A NO-OP -- UNOBSERVED. Accelerate exposes no thread-local
// equivalent that is restorable, and the process-wide
// `accelerate_set_num_threads()` the interior-point solver calls at driver
// level cannot keep the promise above (it is never restored). A count is
// therefore STORED by every hven factor on Apple and applied to nothing; no
// Apple value is estimated anywhere.
class MklThreadScope {
  public:
#if !defined(HVEN_USE_ACCELERATE_LAPACK)
    explicit MklThreadScope(int num_threads) : engaged_(num_threads > 0) {
        if (engaged_) {
            previous_ = mkl_set_num_threads_local(num_threads);
        }
    }

    ~MklThreadScope() {
        if (engaged_) {
            mkl_set_num_threads_local(previous_);
        }
    }
#else
    explicit MklThreadScope(int num_threads) : engaged_(num_threads > 0) {}
    ~MklThreadScope() = default;
#endif

    MklThreadScope(const MklThreadScope &) = delete;
    MklThreadScope &operator=(const MklThreadScope &) = delete;
    MklThreadScope(MklThreadScope &&) = delete;
    MklThreadScope &operator=(MklThreadScope &&) = delete;

  private:
    bool engaged_;

    // The thread-local override in force before this scope engaged. Only
    // meaningful while engaged_ is true; 0 is both the "there was none"
    // answer MKL reports and the value that restores global control, so the
    // unengaged case needs no separate sentinel.
    int previous_ = 0;
};

} // namespace hven::linear::detail
