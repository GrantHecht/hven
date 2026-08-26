// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipm_polish_extension.h — the "hven.ipm.polish.v1" warm-start extension: the
// interior-point state a hand-off carries beyond the currency's core, its byte
// contract, and the one bridge that turns it into an SQP WarmStart.
//
// The tag is the version. A different payload shape travels under a different
// tag ("...v2"), never these bytes rearranged, and there is no version field
// inside the payload; a reader that does not know a tag skips the whole
// extension.

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include <Eigen/Core>

#include "hven/core/types.h"
#include "hven/detail/warmstart/warm_start.h"
#include "hven/model/structure_identity.h"
#include "hven/warmstart/warm_start_data.h"

namespace hven::solvers {

/// @brief The tag naming this extension. Compared byte-for-byte, never parsed.
inline constexpr std::string_view kIpmPolishTag = "hven.ipm.polish.v1";

/// @brief The interior-point polish hand-off: the invertible bound-dual pair,
///        the inequality values, and the barrier level they were taken at.
///
/// Every block is stated in DECLARED space at declared widths -- `z_lower_` and
/// `z_upper_` at n, `iq_values_` at mi -- and on the caller's own objective
/// scale. Nothing in this component checks a block against a problem: that
/// check belongs to whichever engine stages or bridges the value. The `>= 0`
/// on the two price blocks is part of the contract, not a convention: both
/// engines' staging refuses a negative entry, naming this tag.
struct IpmPolishData {
    /// Barrier parameter at exit, on the caller's objective scale (so
    /// mu_ ~ z * distance holds against the blocks below). Neither consumer in
    /// this library reads it.
    double mu_ = 0.0;
    /// n, >= 0: prices lower(i) <= x(i). 0 where that side is not finite, where
    /// a fixed-variable treatment eliminated the variable, or where the side is
    /// unpriced.
    Eigen::VectorXd z_lower_;
    /// n, >= 0: prices x(i) <= upper(i); 0 in the same three cases.
    Eigen::VectorXd z_upper_;
    /// mi, cI(x) at the exported point in THIS PROJECT's sign convention
    /// (cI(x) <= 0 at a feasible point) -- the `slack_i` from_interior_point
    /// takes, not an interior-point method's own non-negative s.
    Eigen::VectorXd iq_values_;

    friend bool operator==(const IpmPolishData &, const IpmPolishData &) = default;
};

/// @brief Encodes a polish hand-off as this extension's payload bytes.
///
/// THE LAYOUT. All integers fixed-width little-endian; every double is written
/// as its IEEE-754 bit pattern, little-endian, so the round trip is bit-exact
/// for every value a caller can store (NaN payload, infinity and the sign of
/// zero included) and the encoding is a property of the value alone, never of
/// the host that wrote it. Offsets are byte offsets from the start of the
/// PAYLOAD (the currency's framing is outside it):
///
///   0   magic, 8 bytes: 'H' 'V' 'E' 'N' 'I' 'P' 'P' 0x00
///   8   mu_, f64
///   16  z_lower_:   u64 count, then count f64
///       z_upper_:   u64 count, then count f64
///       iq_values_: u64 count, then count f64
///
/// @param polish The value to encode.
/// @return The payload bytes, to be carried under `kIpmPolishTag`.
std::vector<std::byte> serialize_ipm_polish(const IpmPolishData &polish);

/// @brief Decodes payload bytes written by `serialize_ipm_polish`.
/// @param bytes The whole of one payload: trailing bytes are a refusal, not
///        slack.
/// @return The decoded value, bit-identical to the one encoded.
/// @throws std::invalid_argument if `bytes` does not open with the payload
///         magic, ends inside any field, declares a length the input cannot
///         hold, or carries trailing bytes. Every refusal names the byte offset
///         it refused at, what was expected there, and the tag. No length is
///         used before it is checked against what remains, so no input reads
///         past `bytes`.
IpmPolishData deserialize_ipm_polish(std::span<const std::byte> bytes);

/// @brief Finds the polish extension in a warm-start value.
/// @param data The value to search.
/// @return A pointer to the extension carrying `kIpmPolishTag`, or nullptr if
///         the value carries none -- the ordinary core-only case, which is a
///         capability statement and not an error.
/// @throws std::invalid_argument if the value carries the tag MORE THAN ONCE.
const WarmExtension *find_ipm_polish(const WarmStartData &data);

/// @brief Builds an SQP `WarmStart` from a warm-start value carrying the
///        polish extension -- the interior-point crossover, entered from the
///        currency.
///
/// A validate-then-forward: the core's `primal_`/`eq_lmults_`/`iq_lmults_` and
/// the extension's `iq_values_`/`z_lower_`/`z_upper_` are handed to
/// `from_interior_point` as `x`, `lambda_e`, `lambda_i`, `slack_i`, `z_lower`,
/// `z_upper`, with the caller's own `lower` and `upper`. Every sign convention,
/// activity rule and ingest semantic is that function's, in
/// detail/warmstart/warm_start.h. Read it before wiring a caller: the resulting
/// object carries `structure_hash == 0` (there is no model here to hash) and so
/// resolves StartLevel::kSeeded, never kWarm or kHot.
///
/// The core's own `bound_lmults_` is deliberately NOT read. A value whose core
/// and extension disagree is a corrupt value, not a case reconciled here.
///
/// @param data The warm-start value, in declared space, carrying the tag.
/// @param lower Declared lower bounds, size n (nlp_model.h's -1e20 convention
///        marks a side nothing can be active at).
/// @param upper Declared upper bounds, size n.
/// @param structure_key The DECLARATION key (model/structure_identity.h's
///        declaration_key) of the model `lower`/`upper` belong to. The value's
///        own stamp must equal it.
/// @param opts The crossover's activity-inference tolerances, forwarded
///        verbatim.
/// @return The crossover object, `valid == true`.
/// @throws std::invalid_argument if `data` carries no polish extension (naming
///         the tag), if the stamp does not match `structure_key` (naming both
///         digests), if the payload is malformed (the decode's own
///         offset-naming refusal), if the extension's block widths disagree
///         with the core's, or if `lower`/`upper` are not at the core's own n.
WarmStart to_sqp_warm_start(const WarmStartData &data, const Vec &lower, const Vec &upper,
                            const DeclarationKey &structure_key,
                            const IpCrossoverOptions &opts = {});

} // namespace hven::solvers
