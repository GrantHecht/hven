// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// warm_start_data.h — the warm-start currency (M5 R1) and its byte form.
//
// Value-semantic, comparable, serializable; engine-independent by
// construction. Nothing here reaches into either engine, and nothing here
// interprets an extension's bytes.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/model/structure_identity.h"

namespace hven::solvers {

/// @brief One opaque engine extension: a tag naming the producer and meaning,
///        and bytes only that producer interprets.
///
/// The pair is carried verbatim -- this component never parses a payload, and
/// a reader that does not know a tag ignores the whole extension rather than
/// guessing at its bytes. Tag bytes and payload bytes are both arbitrary: a
/// tag is compared, never displayed as text and never assumed NUL-free.
struct WarmExtension {
    std::string tag_; ///< e.g. "hven.ipm.polish.v1".
    std::vector<std::byte> payload_;

    friend bool operator==(const WarmExtension &, const WarmExtension &) = default;
};

/// @brief The warm-start currency: a neutral primal/dual core, the structural
///        stamp it was taken under, and opaque engine extensions.
///
/// SPACE CONVENTION -- binding on every producer and every consumer: every
/// block is stated over the DECLARED problem, the full primal space and the
/// declared row spaces, never over a solver's reduced space. Inside one stamp
/// the declared space is the one space both engines share.
///
/// THE STAMP IS A DECLARATION-IDENTITY KEY (owner ruling, 2026-08-25, on M5
/// W3's cross-engine evidence): a canonical digest of the DECLARED problem --
/// its dimensions, with the fixed-variable treatment's own rows subtracted,
/// and its declared bound STRUCTURE -- computed from the AggregateDeclaration
/// both engines consume (model/structure_identity.h's declaration_key). It is
/// therefore ENGINE-INDEPENDENT and TREATMENT-INDEPENDENT by construction,
/// which is what makes one value mean the same thing at both ends of a
/// hand-off.
///
/// WHAT THE STAMP DOES NOT COVER, stated here because a consumer builds safety
/// arguments on this field: the pieces' ROW STRUCTURE is not hashed (it is not
/// a cross-engine property of a declaration -- see declaration_identity_digest
/// for the measured reason), and bound VALUES are not hashed either, only
/// which sides are finite and whether they coincide. So a re-transcription
/// that splits the same rows across different pieces, or that moves a finite
/// bound without changing which sides are finite, keys the SAME and stages
/// with no refusal. That is deliberate -- moving a finite bound is the
/// continuation flow this currency exists to serve -- and the cost is a
/// possibly-poor starting point, never a wrong answer: every block a warm
/// start supplies is re-measured by the receiving solve's own first
/// convergence test.
///
/// STALENESS IS A DECLARATION CHANGE -- the caller transcribed a different
/// problem -- and nothing else. It is NOT an engine's layout state and NOT its
/// fixed-variable treatment: declared-space payloads are treatment-safe
/// because application ignores the entries an eliminating treatment holds, so
/// refusing across a treatment change would refuse a hand-off that is
/// perfectly good. `ModelStructureKey` remains the LAYOUT/EPOCH key (M4's E2
/// ruling, unchanged) and is not this stamp; the two answer different
/// questions and must never be compared against one another.
///
/// The block sizes are the declared dimensions -- `primal_` and `bound_lmults_`
/// are n, `eq_lmults_` is me, `iq_lmults_` is mi -- and nothing in this
/// component checks them against a problem: the stamp is what a consumer
/// compares, and the size check belongs to whichever engine stages the value.
///
/// `operator==` is exact and fieldwise, so it compares payload doubles
/// bitwise-through-`==`: two payloads carrying NaN in the same slot are NOT
/// equal. A round-trip check over a payload that may carry NaN compares bit
/// patterns, not values.
struct WarmStartData {
    Eigen::VectorXd primal_;    ///< n.
    Eigen::VectorXd eq_lmults_; ///< me.
    Eigen::VectorXd iq_lmults_; ///< mi.
    /// n, signed z = zL - zU (the engine's convention; >= 0 at an active
    /// lower bound).
    Eigen::VectorXd bound_lmults_;
    DeclarationKey structure_key_; ///< The stamp; `==` is the currency test.
    std::vector<WarmExtension> extensions_;

    friend bool operator==(const WarmStartData &, const WarmStartData &) = default;
};

/// @brief The serialized-format version this build writes, and the only one
///        `deserialize` accepts.
///
/// v2 is the DECLARATION-IDENTITY STAMP's format: the key slot is two u64
/// conjuncts where v1 carried three (u64, i32, u64). v1 payloads are REFUSED
/// BY VERSION, naming both versions -- there is no compatibility shim, because
/// there are no v1 payloads outside this repository's own history (the
/// currency is pre-release) and a shim would have to invent a declaration
/// digest from a claim digest, which is not a thing that can be done.
inline constexpr std::uint32_t kWarmStartFormatVersion = 2;

/// @brief Encodes a warm-start value as a self-delimiting, versioned byte
///        sequence.
/// @param data The value to encode.
/// @return The encoding: a magic-and-version tag, then the four blocks, the
///         stamp, and the extensions. Payload doubles are carried as their
///         IEEE-754 bit patterns, so the round trip is bit-exact for every
///         double a caller can store -- NaN payload and sign of zero included.
///         The encoding depends on the value alone, never on this process.
std::vector<std::byte> serialize(const WarmStartData &data);

/// @brief Decodes a byte sequence written by `serialize`.
/// @param bytes The encoding, which must be the whole of one value: trailing
///        bytes are a refusal, not slack.
/// @return The decoded value, bit-identical to the one encoded.
/// @throws std::invalid_argument if `bytes` is not a version this build reads,
///         does not open with the format magic, ends inside any field, declares
///         a length the input cannot hold, or carries trailing bytes. Every
///         refusal names the byte offset it refused at and what was expected
///         there. No length is used before it is checked against what remains,
///         so no input reads past `bytes`.
WarmStartData deserialize(std::span<const std::byte> bytes);

} // namespace hven::solvers
