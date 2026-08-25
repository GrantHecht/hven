// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The consumed-surface audit's RUNTIME half: a link-level recorder that sees
// every call any seam makes into the sparse backend's phase entry point, and
// the parameter array as it stood at that moment.
//
// WHY A LINK-LEVEL INTERPOSER AND NOT AN ADAPTER-LEVEL HOOK. The audit's
// question is "which backend touchpoints actually EXECUTE when the engines run
// their fixtures", and the interesting answers are not reachable from any
// adapter. The one this recorder is pre-registered to find is the clearest
// example: the correctness rule that forces the iterative-refinement cap to
// zero around every phase-split solve and restores it afterwards. That rule is
// not an option, it is written inside a private method of one of the seams,
// and no grep over an OPTION surface would ever produce it. Interposing the
// backend's own entry point sees it happen.
//
// HOW IT IS WIRED. The rig's audit target links with the GNU/LLVM linker's
// --wrap flag on the backend entry point, so every reference to it from every
// object in the link -- hven's own session, either old seam's adapter -- is
// redirected here. The recorder forwards to the real symbol unchanged, so a
// recorded run and an unrecorded one do the same arithmetic; the recorder only
// watches. It is OFF unless enabled, so a target that links it but does not
// use it pays a branch.
//
// THE DETECTOR IS NOT TOLD WHAT TO LOOK FOR. `entries_that_varied()` reports
// which parameter-array indices changed value between calls, which is how a
// dynamically-managed entry gives itself away without the audit knowing it
// exists. That is what makes the pre-registered coverage test a real test of
// the METHOD rather than of a hardcoded expectation.

#include <vector>

namespace hven::rig::audit {

// The backend's parameter array is a fixed 64 entries.
inline constexpr int kParameterSlots = 64;

// One recorded call into the backend's phase entry point.
struct BackendCall {
    int phase = 0;
    int mtype = 0;
    int n = 0;
    int nrhs = 0;
    int error = 0;
    // The parameter array as it stood when the call was made. The whole array
    // is kept rather than a chosen few entries, because choosing which entries
    // matter is the assumption this instrument exists to avoid making.
    int parameters[kParameterSlots] = {};
};

class BackendRecorder {
  public:
    static BackendRecorder &instance();

    // Recording is off by default and is turned on around the window the
    // audit cares about, so unrelated backend traffic in the same process does
    // not contaminate the record.
    void enable();
    void disable();
    void clear();
    bool enabled() const { return enabled_; }

    void record(const BackendCall &call);

    const std::vector<BackendCall> &calls() const { return calls_; }

    // Every distinct phase seen, in ascending order.
    std::vector<int> phases() const;

    // Every parameter index whose value was not the same in all recorded
    // calls. An entry that a caller saves, overwrites and restores around one
    // operation appears here; an entry set once at configuration time does
    // not. No index is special-cased.
    std::vector<int> entries_that_varied() const;

    // The value of one parameter entry at the first recorded call whose phase
    // matches, or `fallback` when no such call was recorded.
    int parameter_at_phase(int phase, int index, int fallback) const;

  private:
    bool enabled_ = false;
    std::vector<BackendCall> calls_;
};

// RAII: records for the duration of a scope, and clears the log on entry so a
// window's record is only its own.
class RecordingWindow {
  public:
    RecordingWindow();
    ~RecordingWindow();
    RecordingWindow(const RecordingWindow &) = delete;
    RecordingWindow &operator=(const RecordingWindow &) = delete;
};

} // namespace hven::rig::audit
