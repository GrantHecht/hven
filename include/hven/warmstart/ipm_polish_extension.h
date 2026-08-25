// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipm_polish_extension.h — the "hven.ipm.polish.v1" warm-start extension: the
// value it carries, its byte contract, and the one bridge that turns it into
// an SQP WarmStart.
//
// WHY IT EXISTS. The currency (warmstart/warm_start_data.h) carries a single
// SIGNED bound multiplier per variable, z = zL - zU. That form is what a
// stationarity residual wants and it is engine-neutral, but it is NOT
// invertible at a two-sided bound: z = 0.4 could be (zL, zU) = (0.4, 0) or
// (1.4, 1.0), and an interior-point method's barrier state needs the pair, not
// the difference. So does the interior-point crossover
// (detail/warmstart/warm_start.h's from_interior_point), which judges each
// bound side against its own dual population. The pair travels here, as an
// opaque extension, rather than in the currency: it is one engine family's
// state, and the currency's core is the part every consumer understands.
//
// WHAT IT CARRIES, and why each entry is in rather than out. The set is
// exactly what the two consumers need BEYOND the core blocks:
//
//   * `mu_`  -- the barrier parameter the exporting solve ended at, on the
//     CALLER's objective scale (so mu_ ~ z * distance holds against the
//     bound multipliers below, which are on that same scale). It is the
//     hand-off's own statement of how loose it is, which
//     from_interior_point's own THREE DELIBERATE LIMITS note tells a caller
//     to read before trusting an activity verdict. NOT consumed by either
//     consumer today -- from_interior_point estimates its own mu_hat from
//     the data it is handed, and the IPM's staging deliberately does not
//     override the barrier schedule a caller set through Settings::init_mu_
//     (a payload silently rewriting a setting is the wrong shape). It is
//     carried because a hand-off that cannot say what barrier level it was
//     taken at cannot be judged, and because §3 of the M5 brief names it.
//
//   * `z_lower_` / `z_upper_` -- the invertible pair, both NON-NEGATIVE, in
//     DECLARED space at declared width n: `z_lower_(i)` prices the constraint
//     lower(i) <= x(i) and `z_upper_(i)` prices x(i) <= upper(i), and an entry
//     is 0 where that side is not finite, where the variable is eliminated by
//     a fixed-variable treatment, or where the side is simply unpriced.
//     DECLARED space, not the solver's reduced space, for the reason the
//     currency's own SPACE CONVENTION gives: inside one stamp the declared
//     space is the one space both engines share, and it is also the space
//     from_interior_point's `lower`/`upper` arguments live in.
//
//   * `iq_values_` -- cI(x) at the exported point, declared width mi, in THIS
//     PROJECT's sign convention (cI(x) <= 0 at a feasible point). This is the
//     `slack_i` argument from_interior_point takes -- read that function's
//     SIGN CONVENTION paragraph: it wants the cI VALUES, not an
//     interior-point method's own non-negative s. It is carried rather than
//     recomputed because the payload is a CROSS-PROCESS value: a consumer
//     that had to evaluate the model to use the extension would need the
//     model, which is exactly the dependency the currency exists to remove.
//
// WHAT IT DOES NOT CARRY: the variable bounds themselves. Those are a
// property of the MODEL, not of the solve, and a consumer of this extension
// has the model in hand by construction (it is about to solve it). They are
// the bridge's own arguments below.
//
// THE TAG IS THE VERSION. "hven.ipm.polish.v1" names one shape. A different
// shape is a DIFFERENT TAG ("...v2"), never these bytes rearranged: a reader
// that does not know a tag skips the whole extension, which is precisely the
// capability downgrade R3 asks for, and which a silently-changed layout under
// a stable tag would turn into a misread. That is also why there is no
// version FIELD inside the payload, unlike the currency's own byte form: two
// version axes that can disagree are worse than one that cannot.

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
/// See this header's own note for what each member means, which space it is
/// stated in, and why it is carried. All three vectors are at DECLARED widths
/// -- `z_lower_` and `z_upper_` at n, `iq_values_` at mi -- and nothing in this
/// component checks them against a problem: that check belongs to whichever
/// engine stages or bridges the value, which is the same division the currency
/// itself draws.
struct IpmPolishData {
    double mu_ = 0.0;           ///< Barrier parameter at exit, caller's scale.
    Eigen::VectorXd z_lower_;   ///< n, >= 0, prices lower(i) <= x(i).
    Eigen::VectorXd z_upper_;   ///< n, >= 0, prices x(i) <= upper(i).
    Eigen::VectorXd iq_values_; ///< mi, cI(x) at the exported point (<= 0 feasible).

    friend bool operator==(const IpmPolishData &, const IpmPolishData &) = default;
};

/// @brief Encodes a polish hand-off as this extension's payload bytes.
///
/// THE LAYOUT. All integers fixed-width little-endian; every double is written
/// as its IEEE-754 bit pattern, little-endian, so the round trip is bit-exact
/// for every value a caller can store (NaN payload, infinity and the sign of
/// zero included) and the encoding is a property of the value alone, never of
/// the host that wrote it. Offsets are byte offsets from the start of the
/// PAYLOAD (the extension's own bytes; the currency's framing is outside it):
///
///   0   magic, 8 bytes: 'H' 'V' 'E' 'N' 'I' 'P' 'P' 0x00
///   8   mu_, f64
///   16  z_lower_:   u64 count, then count f64
///       z_upper_:   u64 count, then count f64
///       iq_values_: u64 count, then count f64
///
/// The magic carries the whole "these are not our bytes" verdict -- there is
/// no version field, because the TAG is the version (this header's note). It
/// earns its eight bytes anyway: without it a corrupt payload under the known
/// tag would be read as a length field first, and "declared 2^60 elements" is
/// a worse diagnostic than "this is not a polish payload".
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
///         hold, or carries trailing bytes. Every refusal names the byte
///         offset it refused at, what was expected there, and the tag -- a
///         malformed payload under a KNOWN tag is corruption, and corruption
///         is refused loudly rather than skipped the way a foreign tag is.
///         No length is used before it is checked against what remains, so no
///         input reads past `bytes`.
IpmPolishData deserialize_ipm_polish(std::span<const std::byte> bytes);

/// @brief Finds the polish extension in a warm-start value.
/// @param data The value to search.
/// @return A pointer to the extension carrying `kIpmPolishTag`, or nullptr if
///         the value carries none -- the ordinary core-only case, which is a
///         capability statement and not an error.
/// @throws std::invalid_argument if the value carries the tag MORE THAN ONCE.
///         Two payloads under one tag is a value no producer here emits and a
///         question this function will not arbitrate silently.
const WarmExtension *find_ipm_polish(const WarmStartData &data);

/// @brief Builds an SQP `WarmStart` from a warm-start value carrying the
///        polish extension -- the interior-point crossover, entered from the
///        currency.
///
/// THE ONE PLACE the currency meets `from_interior_point`. Every sign
/// convention, activity rule and ingest semantic lives there, in
/// detail/warmstart/warm_start.h, and this function only maps: the core's
/// `primal_`/`eq_lmults_`/`iq_lmults_` and the extension's
/// `iq_values_`/`z_lower_`/`z_upper_` are handed over as `x`, `lambda_e`,
/// `lambda_i`, `slack_i`, `z_lower`, `z_upper`, with the caller's own `lower`
/// and `upper`. Read that header before wiring a caller: the resulting object
/// carries `structure_hash == 0` (there is no model here to hash) and so
/// resolves StartLevel::kSeeded, never kWarm or kHot.
///
/// The core's own `bound_lmults_` is deliberately NOT read: it is the signed
/// difference of the two blocks this extension carries, and re-deriving the
/// pair from it is the inversion that does not exist. A value whose core and
/// extension disagree is a corrupt value, not a case to reconcile here.
///
/// @param data The warm-start value, in declared space, carrying the tag.
/// @param lower Declared lower bounds, size n (nlp_model.h's -1e20 convention
///        marks a side nothing can be active at).
/// @param upper Declared upper bounds, size n.
/// @param structure_key The key of the model `lower`/`upper` belong to. The
///        value's own stamp must equal it: a hand-off taken under a different
///        declared structure names different variables and different rows.
/// @param opts The crossover's activity-inference tolerances, forwarded
///        verbatim.
/// @return The crossover object, `valid == true`.
/// @throws std::invalid_argument if `data` carries no polish extension (naming
///         the tag -- a core-only value cannot be bridged, because the pair it
///         would need is exactly what it does not carry), if the stamp does
///         not match `structure_key` (naming both digests), if the payload is
///         malformed (the decode's own offset-naming refusal), if the
///         extension's block widths disagree with the core's, or if
///         `lower`/`upper` are not at the core's own n.
WarmStart to_sqp_warm_start(const WarmStartData &data, const Vec &lower, const Vec &upper,
                            const ModelStructureKey &structure_key,
                            const IpCrossoverOptions &opts = {});

} // namespace hven::solvers
