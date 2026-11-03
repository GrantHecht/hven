// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// warm_start_data.h — the warm-start currency (M5 R1), its byte form, and the
// row-space seed.
//
// Value-semantic, comparable, serializable; engine-independent by
// construction. Nothing here interprets an extension's bytes. The SQP
// column-scaling decision (carried by SqpWarmStart) is not part of this
// currency: a payload route re-reads the rule.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/model/structure_identity.h"

namespace hven::solvers {

class Problem;

namespace detail {
struct WarmStartAccess;
} // namespace detail

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
///        stamp it was taken under, and opaque engine extensions. A
///        default-built value is EMPTY and means "no warm start".
///
/// SPACE CONVENTION -- binding on every producer and every consumer: every
/// block is stated over the DECLARED problem, the full primal space and the
/// declared row spaces, never over a solver's reduced space.
///
/// THE STAMP IS A DECLARATION-IDENTITY KEY: a canonical digest of the DECLARED
/// problem -- its dimensions, with the fixed-variable treatment's own rows
/// subtracted, and its declared bound STRUCTURE -- computed from the
/// AssemblyDeclaration both engines consume (model/structure_identity.h's
/// declaration_key). It is therefore ENGINE-INDEPENDENT and
/// TREATMENT-INDEPENDENT, which is what makes one value mean the same thing at
/// both ends of a hand-off.
///
/// WHAT THE STAMP DOES NOT COVER, stated here because a consumer builds safety
/// arguments on this field: the pieces' ROW STRUCTURE is not hashed, and bound
/// VALUES are not hashed either, only which sides are finite and whether they
/// coincide (declaration_identity_digest holds the exclusion list and its
/// reasons). So a re-transcription that splits the same rows across different
/// pieces, or that moves a finite bound without changing which sides are
/// finite, keys the SAME and stages with no refusal. The cost is a
/// possibly-poor starting point, never a wrong answer: every block a warm start
/// supplies is re-measured by the receiving solve's own first convergence test.
///
/// STALENESS IS A DECLARATION CHANGE -- the caller transcribed a different
/// problem -- and nothing else: not an engine's layout state, and not its
/// fixed-variable treatment. `ModelStructureKey` remains the LAYOUT/EPOCH key;
/// the two answer different questions and must never be compared against one
/// another.
///
/// The stored blocks are in the ENGINE SPLIT over the declared problem -- the
/// primal block and the bound-multiplier block are n, the equality block is me,
/// the inequality block is mi -- and nothing in this component checks them
/// against a problem on its own: the stamp is what a consumer compares, and the
/// size check belongs to whichever engine stages the value. A consumer reads and
/// builds the multipliers in the problem's ROW SPACE instead (@ref lambda,
/// @ref seed), the same space `SolveResult::lambda` reports; the engine split
/// stays behind `detail::WarmStartAccess`.
///
class WarmStart {
  public:
    /// @brief The empty value: no blocks, no stamp. Passed to a solve it means
    ///        "no warm start" and the solve is the cold one, counter for counter.
    WarmStart() = default;

    /// @brief True when all four blocks are empty (the value of a solve that
    ///        produced no usable point, and of a default-built value).
    bool empty() const noexcept {
        return primal_.size() == 0 && eq_lmults_.size() == 0 && iq_lmults_.size() == 0 &&
               bound_lmults_.size() == 0;
    }

    /// @brief The primal block, declared width (empty for a multipliers-only
    ///        seed and for an empty value).
    const Vec &x() const noexcept { return primal_; }

    /// @brief The bound multipliers z = z_lower - z_upper, declared width (empty
    ///        for a multipliers-only seed and for an empty value).
    const Vec &z() const noexcept { return bound_lmults_; }

    /// @brief The declaration stamp the value was taken (or seeded) under.
    const DeclarationKey &key() const noexcept { return structure_key_; }

    /// @brief The opaque engine extensions the value carries.
    const std::vector<WarmExtension> &extensions() const noexcept { return extensions_; }

    /// @brief The row multipliers in @p problem's row space: one per Nlp row in
    ///        Ipopt's sign convention, or the pieces problem's [equality rows |
    ///        inequality rows] -- the space `SolveResult::lambda` reports.
    /// @throws std::invalid_argument if the stamp is not @p problem's
    ///         declaration or a multiplier block is not at the problem's row
    ///         counts.
    Vec lambda(const Problem &problem) const;

    /// @brief Builds a warm start from a point and its prices, in the problem's
    ///        row space.
    ///
    /// @param problem The problem the value is for; it supplies the declaration
    ///        stamp (the key both engines stamp on export, so either engine
    ///        accepts the value) and the row map. Its declaration is read as of
    ///        the last solve entry (or construction).
    /// @param x The primal point, declared width; EMPTY builds the
    ///        multipliers-only form (the solve starts from its own `x0`).
    /// @param lambda One multiplier per row of the problem's row space, in the
    ///        sign convention of `SolveResult::lambda`.
    /// @param z The bound multipliers, declared width when @p x is given and
    ///        empty when it is not.
    /// @throws std::invalid_argument naming the block, on a size that does not
    ///         match the problem.
    static WarmStart seed(const Problem &problem, ConstVecRef x, ConstVecRef lambda, ConstVecRef z);

    /// @brief Exact and fieldwise, so it compares payload doubles
    ///        bitwise-through-`==`: two payloads carrying NaN in the same slot
    ///        are NOT equal. A round-trip check over a payload that may carry
    ///        NaN compares bit patterns, not values.
    friend bool operator==(const WarmStart &, const WarmStart &) = default;

  private:
    friend struct detail::WarmStartAccess;

    Vec primal_;    ///< n.
    Vec eq_lmults_; ///< me.
    Vec iq_lmults_; ///< mi.
    /// n, signed z = zL - zU (the engine's convention; >= 0 at an active
    /// lower bound).
    Vec bound_lmults_;
    DeclarationKey structure_key_; ///< The stamp; `==` is the currency test.
    std::vector<WarmExtension> extensions_;
};

/// @brief The serialized-format version this build writes, and the only one
///        `deserialize` accepts.
///
/// v2 is the DECLARATION-IDENTITY STAMP's format: the key slot is two u64
/// conjuncts where v1 carried three (u64, i32, u64). v1 payloads are REFUSED
/// BY VERSION, naming both versions; there is no compatibility shim, so a v1
/// payload is re-exported from the engine that produced it, never converted.
inline constexpr std::uint32_t kWarmStartFormatVersion = 2;

/// @brief Encodes a warm-start value as a self-delimiting, versioned byte
///        sequence.
/// @param data The value to encode.
/// @return The encoding: a magic-and-version tag, then the four blocks, the
///         stamp, and the extensions. Payload doubles are carried as their
///         IEEE-754 bit patterns, so the round trip is bit-exact for every
///         double a caller can store -- NaN payload and sign of zero included.
///         The encoding depends on the value alone, never on this process.
std::vector<std::byte> serialize(const WarmStart &data);

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
WarmStart deserialize(std::span<const std::byte> bytes);

} // namespace hven::solvers
