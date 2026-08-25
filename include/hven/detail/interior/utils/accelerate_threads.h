// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once

// Apple Accelerate thread control and startup initialization for the
// interior-point engine.
//
// This is deliberately NOT part of hven's linear-algebra surface, and it is
// what the engine keeps for itself when that surface owns everything else
// about the factorization. hven::linear's own num_threads option is applied at
// backend-call scope on MKL and stored-but-not-applied on Accelerate, because
// Accelerate's control is version-split, per-calling-thread, and binary rather
// than a count -- a shape no per-instance option can honestly promise. Until
// some hven surface adopts that contract explicitly, the engine keeps driving
// it here: the startup initialization, the per-solve repair of a lingering
// single-thread pin, and Jet's per-worker pin.
//
// Whether these BLAS controls govern Accelerate's Sparse routines at all
// remains unobserved; the mechanism is retained because dropping it would be a
// new, unexplained behavior change, not because its reach has been measured.

#include <Accelerate/Accelerate.h>
#include <cstdlib>
#include <string>

// BLASSetThreading (macOS 15+) provides per-thread dynamic control via
// thread-local storage. It supports a binary toggle: single-threaded vs
// multi-threaded. "Multi" returns to the thread count cached from
// VECLIB_MAXIMUM_THREADS at the first BLAS call.
//
// VECLIB_MAXIMUM_THREADS is read exactly once — at the first BLAS call.
// setenv() after that point is a no-op. BLASSetThreading is the only
// working dynamic control (macOS 15+).
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
#define HVEN_HAS_BLAS_SET_THREADING 1
#endif

// Warm up the Accelerate sparse solver subsystem by performing a trivial
// LDLT factorization. On macOS 26+, this triggers MT-METIS thread pool
// initialization so the first real solve doesn't pay the cold-start penalty.
inline void warmup_sparse_solver() {
    // Tiny 2x2 SPD matrix (upper triangle of [[2,-1],[-1,2]])
    long columnStarts[] = {0, 1, 3};
    int rowIndices[] = {0, 0, 1};
    double values[] = {2.0, -1.0, 2.0};

    SparseMatrixStructure structure{};
    structure.rowCount = 2;
    structure.columnCount = 2;
    structure.columnStarts = columnStarts;
    structure.rowIndices = rowIndices;
    structure.attributes.kind = SparseSymmetric;
    structure.attributes.triangle = SparseUpperTriangle;
    structure.blockSize = 1;

    SparseMatrix_Double A{};
    A.structure = structure;
    A.data = values;

    SparseSymbolicFactorOptions fopts{};
    fopts.control = SparseDefaultControl;
    // Serial METIS, deliberately, on every OS version. This warmup used to
    // request SparseOrderMTMetis where available, but (a) libSparse's
    // MT-METIS path deadlocks inside dispatch_apply on constrained
    // virtualized hosts (observed and stack-sampled on a 3-core macOS 26.5
    // CI VM, at default QoS as well as degraded), hanging every first solve;
    // and (b) the solver's own default ordering on this platform is serial
    // METIS, so MT-METIS pool warm-up only ever benefited a caller who
    // explicitly selects the parallel ordering -- that caller now pays the
    // pool's cold start on first use instead of at startup. Numerics are
    // unaffected either way: this factors a throwaway 2x2.
    fopts.orderMethod = SparseOrderMetis;
    fopts.malloc = malloc;
    fopts.free = free;
    // Without a reportError callback, an Accelerate parameter-check failure
    // takes its null-callback branch: os_log_error followed by _SparseTrap(),
    // i.e. an abort on the startup path.
    fopts.reportError = [](const char *) {};

    auto sym = SparseFactor(SparseFactorizationLDLTTPP, structure, fopts);
    if (sym.status == SparseStatusOK) {
        auto num = SparseFactor(sym, A);
        // Apple requires SparseCleanup even for a FAILED factorization.
        SparseCleanup(num);
    }
    SparseCleanup(sym);
}

// Called once at startup (before any BLAS call) to set the Accelerate thread
// pool size and warm up the runtime (thread pool, CPU dispatch, memory
// allocators). VECLIB_MAXIMUM_THREADS must be set before the first BLAS call;
// it is cached at that point and setenv() is a no-op afterwards.
inline void ensure_accelerate_initialized(int num_threads) {
    setenv("VECLIB_MAXIMUM_THREADS", std::to_string(num_threads).c_str(), 1);
    // Trigger BLAS/vDSP runtime initialization.
    double x = 1.0, result = 0.0;
    vDSP_dotprD(&x, 1, &x, 1, &result, 1);
    // Warm up the sparse solver subsystem (including MT-METIS thread pool).
    warmup_sparse_solver();
}

// Dynamic thread control for Accelerate BLAS/LAPACK operations.
// On macOS 15+, uses BLASSetThreading (per-thread, thread-local).
// On older systems, falls back to VECLIB_MAXIMUM_THREADS env var
// (global, only effective before first BLAS call).
inline void accelerate_set_num_threads(int num_threads) {
#ifdef HVEN_HAS_BLAS_SET_THREADING
    // The #ifdef only guarantees the DECLARATION exists in this SDK.
    // BLASSetThreading is API_AVAILABLE(macos(15.0)) and therefore weak-linked,
    // so a binary built against a newer SDK with an older deployment target
    // would null-call it. The runtime check is the actual guard.
    if (__builtin_available(macOS 15.0, *)) {
        if (num_threads <= 1)
            BLASSetThreading(BLAS_THREADING_SINGLE_THREADED);
        else
            BLASSetThreading(BLAS_THREADING_MULTI_THREADED);
        return;
    }
#endif
    setenv("VECLIB_MAXIMUM_THREADS", std::to_string(num_threads).c_str(), 1);
}
