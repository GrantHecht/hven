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
// machine. Field names, types, and function signatures are believed
// correct based on those two precedents' successful (real-hardware-tested)
// use of the exact same calls, but every declaration here is UNVERIFIED
// against Apple's actual header. A syntax-only compile against this stub
// proves internal self-consistency of hven's own code (types used the way
// they are declared, no typos, no missing includes) -- it does NOT prove
// this stub matches Apple's real API, and therefore does NOT prove the
// real code will compile on Apple hardware. See the Task-6 report.
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

typedef int SparseOrder_t;
enum : SparseOrder_t { SparseOrderDefault = 0, SparseOrderUser = 1, SparseOrderMetis = 2 };

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
