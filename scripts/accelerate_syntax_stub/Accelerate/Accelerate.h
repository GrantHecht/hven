// Minimal STUB of <Accelerate/Accelerate.h>'s Sparse Solvers surface,
// declaring ONLY the names hven/detail/linear/accelerate_session.h and
// src/linear/symmetric_factor_accelerate.cpp actually reference. Built for
// scripts/check_accelerate_syntax_linux.sh -- see that script and
// docs/testing.md for exactly what compiling against this stub does and
// does not prove.
//
// THIS IS NOT AN ACCELERATE HEADER. It is a hand-written approximation of
// the subset of Apple's public Sparse Solvers API surface this repository
// calls, built entirely from reading the two Mac-hardware-verified
// downstream ports this backend follows (tycho's accelerate_interface.h /
// accelerate_utils.h and tycho_sqp's kkt_system_accelerate.h) -- NOT from
// Apple's own header, which is unavailable on this Linux development
// machine. Field names, types, and function signatures were based on those
// two precedents' successful real-hardware use of the same calls. macOS CI
// now compiles hven's call sites against Apple's actual header, confirming
// that this stub is compatible with the current SDK surface hven consumes.
// A syntax-only compile against the stub still proves only internal
// self-consistency; it does not prove runtime behavior or future-SDK parity.
// See docs/testing.md's claim-ceiling section.
//
// No SNOPT, no Apple-copyrighted material, and no decompiled/reconstructed
// header content is present here -- every declaration is a minimal
// forward-facing shape written from the call-site usage in the two
// precedent files and the code this stub supports.

#pragma once

#include <cstddef>

// ---- status / control enums --------------------------------------------

typedef int SparseStatus_t;
enum : SparseStatus_t {
    SparseStatusOK = 0,
    SparseStatusReleased = -1,
    SparseFactorizationFailed = -2,
    SparseMatrixIsSingular = -3,
    SparseInternalError = -4,
    SparseParameterError = -5,
};

typedef int SparseControl_t;
enum : SparseControl_t { SparseDefaultControl = 0 };

// SparseOrderAMD is declared unconditionally here (not gated behind an SDK
// version macro): it is a long-standing Accelerate order method. SparseOrder-
// MTMetis (macOS 26+) is declared conditionally, mirroring how the real SDK
// header only exposes it once the deployment target is new enough --
// gated on the exact same predicate
// src/linear/symmetric_factor_accelerate.cpp's HVEN_HAS_MTMETIS guard uses
// (`defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&
// __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000`).
//
// The DEFAULT syntax-only checks in check_accelerate_syntax_linux.sh define
// neither macro, so SparseOrderMTMetis stays undeclared and only the
// mapper's `#else` (pre-macOS-26 / non-Apple) fallback compiles -- the same
// "SDK too old to declare the symbol" state a real pre-macOS-26 SDK leaves
// this stub in. A SEPARATE check in that script passes
// `-D__APPLE__ -D__MAC_OS_X_VERSION_MAX_ALLOWED=260000` to activate this
// branch and exercise the availability-guarded code path itself (see that
// script's "SDK-26 MT-METIS activated branch" section).
typedef int SparseOrder_t;
enum : SparseOrder_t {
    SparseOrderDefault = 0,
    SparseOrderUser = 1,
    SparseOrderMetis = 2,
    SparseOrderAMD = 3,
#if defined(__APPLE__) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) &&                               \
    __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
    SparseOrderMTMetis = 4,
#endif
};

typedef int SparseScaling_t;
enum : SparseScaling_t { SparseScalingDefault = 0 };

typedef int SparseKind_t;
enum : SparseKind_t { SparseOrdinary = 0, SparseSymmetric = 1, SparseTriangular = 2 };

typedef int SparseTriangle_t;
enum : SparseTriangle_t { SparseLowerTriangle = 0, SparseUpperTriangle = 1 };

typedef int SparseFactorization_t;
enum : SparseFactorization_t {
    SparseFactorizationCholesky = 0,
    SparseFactorizationLDLT = 1,
    SparseFactorizationLDLTUnpivoted = 2,
    SparseFactorizationLDLTSBK = 3,
    SparseFactorizationLDLTTPP = 4,
    SparseFactorizationQR = 5,
    SparseFactorizationCholeskyAtA = 6,
};

typedef int SparseSubfactor_t;
enum : SparseSubfactor_t { SparseSubfactorL = 0, SparseSubfactorD = 1 };

// ---- structures ----------------------------------------------------------

struct SparseAttributes_t {
    SparseKind_t kind = SparseOrdinary;
    SparseTriangle_t triangle = SparseLowerTriangle;
    bool transpose = false;
};

struct SparseMatrixStructure {
    int rowCount = 0;
    int columnCount = 0;
    long *columnStarts = nullptr;
    int *rowIndices = nullptr;
    SparseAttributes_t attributes{};
    unsigned char blockSize = 1;
};

struct SparseMatrix_Double {
    SparseMatrixStructure structure{};
    double *data = nullptr;
};

struct SparseSymbolicFactorOptions {
    SparseControl_t control = SparseDefaultControl;
    SparseOrder_t orderMethod = SparseOrderDefault;
    int *order = nullptr;
    int *ignoreRowsAndColumns = nullptr;
    void *(*malloc)(std::size_t) = nullptr;
    void (*free)(void *) = nullptr;
    void (*reportError)(const char *) = nullptr;
};

struct SparseNumericFactorOptions {
    SparseControl_t control = SparseDefaultControl;
    SparseScaling_t scalingMethod = SparseScalingDefault;
    double *scaling = nullptr;
    double pivotTolerance = 0.01;
    double zeroTolerance = 0.0;
};

struct SparseOpaqueSymbolicFactorization {
    SparseStatus_t status = SparseStatusReleased;

    // Real Accelerate fields (confirmed by the two Mac-hardware-verified
    // downstream ports symmetric_factor_accelerate.cpp's own header comment
    // names -- tycho's accelerate_interface.h reads
    // m_symbolicFactorization->factorSize_Double/factorSize_Float on real
    // hardware). Reported by Apple in BYTES. Only *_Double is read by this
    // repository (hven's SpMatRM is always double-valued); *_Float is
    // declared alongside it for shape-fidelity with the real header, not
    // because anything here reads it.
    size_t factorSize_Double = 0;
    size_t factorSize_Float = 0;
};

struct SparseOpaqueFactorization_Double {
    SparseStatus_t status = SparseStatusReleased;
};

struct SparseOpaqueSubfactor_Double {
    SparseStatus_t status = SparseStatusReleased;
    SparseAttributes_t attributes{};
};

struct DenseVector_Double {
    int count = 0;
    double *data = nullptr;
};

// ---- entry points ----------------------------------------------------------

inline SparseOpaqueSymbolicFactorization SparseFactor(SparseFactorization_t, SparseMatrixStructure,
                                                      SparseSymbolicFactorOptions) {
    return SparseOpaqueSymbolicFactorization{SparseStatusOK};
}

inline SparseOpaqueFactorization_Double
SparseFactor(SparseOpaqueSymbolicFactorization, SparseMatrix_Double, SparseNumericFactorOptions) {
    return SparseOpaqueFactorization_Double{SparseStatusOK};
}

inline void SparseCleanup(SparseOpaqueSymbolicFactorization) {}
inline void SparseCleanup(SparseOpaqueFactorization_Double) {}
inline void SparseCleanup(SparseOpaqueSubfactor_Double) {}

inline int SparseGetInertia(SparseOpaqueFactorization_Double, int *num_positive, int *num_zero,
                            int *num_negative) {
    if (num_positive)
        *num_positive = 0;
    if (num_zero)
        *num_zero = 0;
    if (num_negative)
        *num_negative = 0;
    return 0;
}

inline void SparseSolve(SparseOpaqueFactorization_Double, DenseVector_Double, DenseVector_Double) {}
inline void SparseSolve(SparseOpaqueSubfactor_Double, DenseVector_Double) {}

inline SparseOpaqueSubfactor_Double SparseCreateSubfactor(SparseSubfactor_t,
                                                          SparseOpaqueFactorization_Double) {
    return SparseOpaqueSubfactor_Double{};
}
