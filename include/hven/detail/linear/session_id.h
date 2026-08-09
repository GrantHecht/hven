#pragma once

// The process-unique identity a backend factorization session is stamped with
// at creation.
//
// WHY IT EXISTS. A factorization used to be named by (pattern_hash, epoch)
// alone, and that pair can FORK. Re-analyzing an engine starts a fresh session
// seeded from the old session's epoch (so epochs never run backwards), but a
// handle emitted before the re-analysis keeps the OLD session alive and
// solvable. Refactorizing both branches then advances two independent epoch
// counters from the same seed: two different sets of numerics, on one sparsity
// pattern, wearing the same epoch. A consumer keying a cache on the pair would
// accept the wrong branch's numbers.
//
// The session id is the discriminator. Sessions are created in exactly one
// place -- SymmetricFactor::analyze() -- so a fork always produces a session
// with a NEW id while every co-owner of the old one keeps the old id, and the
// triple (pattern_hash, session_id, epoch) names one set of numerics for the
// life of the process.
//
// SCOPE, stated rather than left to be assumed: the id is unique within ONE
// PROCESS RUN and is not stable across runs -- it is an identity, not a
// fingerprint. Nothing persists it, and nothing should: a value written to
// disk and read back in another process would name a session that no longer
// exists. The pattern hash is the part of the triple that travels.

#include <atomic>
#include <cstdint>

namespace hven::linear::detail {

// Returns the next session id: process-unique and monotonically increasing,
// starting at 1. Zero is never returned, so it stays available to the public
// surface as "this engine has no session yet".
//
// Relaxed ordering is sufficient and deliberate: the only thing required of
// the counter is that no two calls return the same value, which
// fetch_add's atomicity gives on its own. Nothing is published through this
// value -- a session's own data is published by the shared_ptr that carries
// it -- so there is no acquire/release pairing to establish here.
inline std::uint64_t next_session_id() noexcept {
    static std::atomic<std::uint64_t> counter{0};
    return counter.fetch_add(1, std::memory_order_relaxed) + 1;
}

} // namespace hven::linear::detail
