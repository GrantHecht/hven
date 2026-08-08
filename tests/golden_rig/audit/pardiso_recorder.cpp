// See pardiso_recorder.h. This TU also defines the wrapper the linker
// redirects the backend's phase entry point to.

#include <algorithm>
#include <cstring>

#include <mkl_pardiso.h>
#include <mkl_types.h>

#include "pardiso_recorder.h"

namespace hven::rig::audit {

BackendRecorder &BackendRecorder::instance() {
    static BackendRecorder r;
    return r;
}

void BackendRecorder::enable() { enabled_ = true; }
void BackendRecorder::disable() { enabled_ = false; }
void BackendRecorder::clear() { calls_.clear(); }

void BackendRecorder::record(const BackendCall &call) { calls_.push_back(call); }

std::vector<int> BackendRecorder::phases() const {
    std::vector<int> out;
    for (const BackendCall &c : calls_) {
        if (std::find(out.begin(), out.end(), c.phase) == out.end()) {
            out.push_back(c.phase);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<int> BackendRecorder::entries_that_varied() const {
    std::vector<int> out;
    if (calls_.size() < 2) {
        return out;
    }
    for (int i = 0; i < kParameterSlots; ++i) {
        const int first = calls_.front().parameters[i];
        for (const BackendCall &c : calls_) {
            if (c.parameters[i] != first) {
                out.push_back(i);
                break;
            }
        }
    }
    return out;
}

int BackendRecorder::parameter_at_phase(int phase, int index, int fallback) const {
    if (index < 0 || index >= kParameterSlots) {
        return fallback;
    }
    for (const BackendCall &c : calls_) {
        if (c.phase == phase) {
            return c.parameters[index];
        }
    }
    return fallback;
}

RecordingWindow::RecordingWindow() {
    BackendRecorder::instance().clear();
    BackendRecorder::instance().enable();
}

RecordingWindow::~RecordingWindow() { BackendRecorder::instance().disable(); }

} // namespace hven::rig::audit

// --- the interposer ---------------------------------------------------------
//
// The linker's --wrap flag redirects every reference to `pardiso` to
// `__wrap_pardiso` and makes the original reachable as `__real_pardiso`. The
// signature below is the backend header's own, copied exactly: a mismatch here
// would be undefined behaviour rather than a compile error, since the linker
// only matches names.
//
// ONE ENTRY POINT IS WRAPPED, NOT TWO, and that is a claim rather than an
// omission. The backend also exposes a 64-bit-index entry point, and the
// static half of the audit does match it -- but no seam in scope can reach it.
// hven's own session static_asserts that the backend's integer width matches
// Eigen's sparse index width, which pins this build to the 32-bit interface;
// the SQP old seam calls the 32-bit name directly; and the interior-point old
// seam selects between the two by its matrix's storage-index type, which is
// Eigen's default `int` for every matrix this rig builds. If a future build
// ever links the 64-bit interface, hven's own static_assert fires first and
// this comment becomes the place to add the second wrapper.

extern "C" {

void __real_pardiso(_MKL_DSS_HANDLE_t pt, const MKL_INT *maxfct, const MKL_INT *mnum,
                    const MKL_INT *mtype, const MKL_INT *phase, const MKL_INT *n, const void *a,
                    const MKL_INT *ia, const MKL_INT *ja, MKL_INT *perm, const MKL_INT *nrhs,
                    MKL_INT *iparm, const MKL_INT *msglvl, void *b, void *x, MKL_INT *error);

void __wrap_pardiso(_MKL_DSS_HANDLE_t pt, const MKL_INT *maxfct, const MKL_INT *mnum,
                    const MKL_INT *mtype, const MKL_INT *phase, const MKL_INT *n, const void *a,
                    const MKL_INT *ia, const MKL_INT *ja, MKL_INT *perm, const MKL_INT *nrhs,
                    MKL_INT *iparm, const MKL_INT *msglvl, void *b, void *x, MKL_INT *error) {
    namespace audit = hven::rig::audit;
    audit::BackendRecorder &recorder = audit::BackendRecorder::instance();

    // The parameter array is captured BEFORE the call: this instrument's
    // subject is what the caller configured, and the backend writes its own
    // output entries during the call.
    audit::BackendCall entry;
    const bool recording = recorder.enabled();
    if (recording) {
        entry.phase = phase != nullptr ? static_cast<int>(*phase) : 0;
        entry.mtype = mtype != nullptr ? static_cast<int>(*mtype) : 0;
        entry.n = n != nullptr ? static_cast<int>(*n) : 0;
        entry.nrhs = nrhs != nullptr ? static_cast<int>(*nrhs) : 0;
        if (iparm != nullptr) {
            for (int i = 0; i < audit::kParameterSlots; ++i) {
                entry.parameters[i] = static_cast<int>(iparm[i]);
            }
        }
    }

    __real_pardiso(pt, maxfct, mnum, mtype, phase, n, a, ia, ja, perm, nrhs, iparm, msglvl, b, x,
                   error);

    if (recording) {
        entry.error = error != nullptr ? static_cast<int>(*error) : 0;
        recorder.record(entry);
    }
}

} // extern "C"
