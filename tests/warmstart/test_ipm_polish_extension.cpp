// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The "hven.ipm.polish.v1" extension, engine-free: the byte form's round trip
// and every refusal it owes a caller (a bad magic, a truncation at any offset,
// a declared length the input cannot hold, trailing bytes), the tag lookup's
// three answers (absent, present, duplicated), and the crossover bridge's
// validation and forwarding.
//
// The frozen byte-layout pin is stated BY HAND from the layout documented in
// hven/warmstart/ipm_polish_extension.h, so a silent format drift fails it
// even when the encoder and decoder still agree with each other. That is what
// makes the tag's "v1" mean something.
//
// The bridge's equivalence to the privileged hand-off is pinned twice: here,
// against hand-built blocks, and in tests/interior/test_ipm_warm_start.cpp
// against a real solve's own export. This one fixes the MAPPING (which payload
// block reaches which from_interior_point argument); that one fixes the
// PRODUCER.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <fmt/format.h>

#include <gtest/gtest.h>

#include "hven/detail/warmstart/warm_start.h"
#include "hven/model/structure_identity.h"
#include "hven/warmstart/ipm_polish_extension.h"
#include "hven/warmstart/warm_start_data.h"

using hven::solvers::DeclarationKey;
using hven::solvers::deserialize_ipm_polish;
using hven::solvers::find_ipm_polish;
using hven::solvers::from_interior_point;
using hven::solvers::IpmPolishData;
using hven::solvers::kIpmPolishTag;
using hven::solvers::serialize_ipm_polish;
using hven::solvers::to_sqp_warm_start;
using hven::solvers::WarmExtension;
using hven::solvers::WarmStart;
using hven::solvers::WarmStartData;

namespace {

Eigen::VectorXd vector_of(std::initializer_list<double> values) {
    Eigen::VectorXd vector(static_cast<Eigen::Index>(values.size()));
    Eigen::Index i = 0;
    for (const double value : values) {
        vector[i++] = value;
    }
    return vector;
}

std::vector<std::byte> bytes_of(std::initializer_list<unsigned char> values) {
    std::vector<std::byte> bytes;
    bytes.reserve(values.size());
    for (const unsigned char value : values) {
        bytes.push_back(static_cast<std::byte>(value));
    }
    return bytes;
}

bool mentions(const std::string &message, const std::string &fragment) {
    return message.find(fragment) != std::string::npos;
}

std::string refusal_message(std::span<const std::byte> bytes) {
    try {
        deserialize_ipm_polish(bytes);
    } catch (const std::invalid_argument &error) {
        return error.what();
    }
    ADD_FAILURE() << "expected a refusal, got a decoded value";
    return {};
}

template <typename Callable> std::string refusal_from(Callable &&callable) {
    try {
        callable();
    } catch (const std::invalid_argument &error) {
        return error.what();
    }
    ADD_FAILURE() << "expected a refusal, got a value";
    return {};
}

/// The value the frozen byte pin below encodes: two variables (one priced at
/// its lower bound, one at its upper), one inequality row, a barrier level.
IpmPolishData frozen_value() {
    IpmPolishData polish;
    polish.mu_ = 0.5;
    polish.z_lower_ = vector_of({1.0, 0.0});
    polish.z_upper_ = vector_of({0.0, 2.0});
    polish.iq_values_ = vector_of({-0.25});
    return polish;
}

/// The same value's bytes, stated by hand from the documented layout:
///   0   magic "HVENIPP\0"
///   8   mu_ = 0.5              (0x3FE0000000000000)
///   16  z_lower_: count 2, then 1.0 (0x3FF0...), 0.0
///   40  z_upper_: count 2, then 0.0, 2.0 (0x4000...)
///   64  iq_values_: count 1, then -0.25 (0xBFD0...)
std::vector<std::byte> frozen_bytes() {
    return bytes_of({
        0x48, 0x56, 0x45, 0x4e, 0x49, 0x50, 0x50, 0x00, // magic
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x3f, // mu_ = 0.5
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // z_lower_ count = 2
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, // 1.0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0.0
        0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // z_upper_ count = 2
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 0.0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, // 2.0
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // iq_values_ count = 1
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf, // -0.25
    });
}

constexpr std::size_t kFrozenSize = 80;
constexpr std::size_t kLowerLengthOffset = 16;

void poke_u64(std::vector<std::byte> &bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
    }
}

DeclarationKey key_of(std::uint64_t declaration) {
    DeclarationKey key;
    key.declaration_digest_ = declaration;
    key.bound_digest_ = 0xABCDEF;
    return key;
}

/// A two-variable, one-equality, two-inequality hand-off, bounded on both
/// sides, whose blocks are all distinguishable from one another -- so a
/// mapping that swapped two of them could not pass.
WarmStartData bridgeable_value() {
    WarmStartData data;
    data.primal_ = vector_of({0.0, 3.0});
    data.eq_lmults_ = vector_of({0.75});
    data.iq_lmults_ = vector_of({2.0, 1.0e-12});
    data.bound_lmults_ = vector_of({4.0, -1.5});
    data.structure_key_ = key_of(0x11);

    IpmPolishData polish;
    polish.mu_ = 1.0e-9;
    polish.z_lower_ = vector_of({4.0, 1.0e-12});
    polish.z_upper_ = vector_of({1.0e-12, 1.5});
    polish.iq_values_ = vector_of({-1.0e-10, -2.0});
    data.extensions_.push_back(
        WarmExtension{std::string(kIpmPolishTag), serialize_ipm_polish(polish)});
    return data;
}

Eigen::VectorXd bridge_lower() { return vector_of({0.0, -10.0}); }
Eigen::VectorXd bridge_upper() { return vector_of({10.0, 3.0}); }

void expect_same_warm_start(const WarmStart &actual, const WarmStart &expected) {
    ASSERT_EQ(actual.x.size(), expected.x.size());
    for (Eigen::Index i = 0; i < expected.x.size(); ++i) {
        EXPECT_EQ(actual.x[i], expected.x[i]) << "x[" << i << "]";
    }
    ASSERT_EQ(actual.lambda_e.size(), expected.lambda_e.size());
    for (Eigen::Index i = 0; i < expected.lambda_e.size(); ++i) {
        EXPECT_EQ(actual.lambda_e[i], expected.lambda_e[i]) << "lambda_e[" << i << "]";
    }
    ASSERT_EQ(actual.lambda_i.size(), expected.lambda_i.size());
    for (Eigen::Index i = 0; i < expected.lambda_i.size(); ++i) {
        EXPECT_EQ(actual.lambda_i[i], expected.lambda_i[i]) << "lambda_i[" << i << "]";
    }
    ASSERT_EQ(actual.z.size(), expected.z.size());
    for (Eigen::Index i = 0; i < expected.z.size(); ++i) {
        EXPECT_EQ(actual.z[i], expected.z[i]) << "z[" << i << "]";
    }
    EXPECT_EQ(actual.ineq_active, expected.ineq_active);
    EXPECT_EQ(actual.bound_active, expected.bound_active);
    EXPECT_EQ(actual.qp_working_set.bound_state(), expected.qp_working_set.bound_state());
    EXPECT_EQ(actual.qp_working_set.active_ineq(), expected.qp_working_set.active_ineq());
    EXPECT_EQ(actual.funnel_width, expected.funnel_width);
    EXPECT_EQ(actual.tr_radius, expected.tr_radius);
    EXPECT_EQ(actual.primal_delta, expected.primal_delta);
    EXPECT_EQ(actual.dual_mu, expected.dual_mu);
    EXPECT_EQ(actual.structure_hash, expected.structure_hash);
    EXPECT_EQ(actual.hot, expected.hot);
    EXPECT_EQ(actual.valid, expected.valid);
    EXPECT_EQ(actual.prox_sigma, expected.prox_sigma);
    EXPECT_EQ(actual.has_prox_center, expected.has_prox_center);
}

} // namespace

// ---------------------------------------------------------------------------
// The byte form
// ---------------------------------------------------------------------------

TEST(IpmPolishSerialization, RoundTripsAPopulatedPayload) {
    const IpmPolishData original = frozen_value();
    const IpmPolishData decoded = deserialize_ipm_polish(serialize_ipm_polish(original));
    EXPECT_EQ(decoded, original);
}

TEST(IpmPolishSerialization, RoundTripsADefaultConstructedPayload) {
    const IpmPolishData original;
    const IpmPolishData decoded = deserialize_ipm_polish(serialize_ipm_polish(original));
    EXPECT_EQ(decoded, original);
    EXPECT_EQ(decoded.z_lower_.size(), 0);
    EXPECT_EQ(decoded.iq_values_.size(), 0);
}

TEST(IpmPolishSerialization, RoundTripsSpecialDoublesBitExactly) {
    IpmPolishData original;
    original.mu_ = -0.0;
    original.z_lower_ = vector_of({std::numeric_limits<double>::infinity(),
                                   -std::numeric_limits<double>::infinity(),
                                   std::numeric_limits<double>::denorm_min()});
    original.z_upper_ = vector_of({std::numeric_limits<double>::quiet_NaN(), -0.0, 0.0});
    original.iq_values_ = vector_of({std::numeric_limits<double>::max()});

    const IpmPolishData decoded = deserialize_ipm_polish(serialize_ipm_polish(original));
    // Bit patterns, not values: NaN is not == itself, and the sign of zero is
    // invisible to ==. The currency's own round-trip pin makes the same move.
    const auto bits = [](double value) { return std::bit_cast<std::uint64_t>(value); };
    EXPECT_EQ(bits(decoded.mu_), bits(original.mu_));
    for (Eigen::Index i = 0; i < original.z_lower_.size(); ++i) {
        EXPECT_EQ(bits(decoded.z_lower_[i]), bits(original.z_lower_[i])) << i;
        EXPECT_EQ(bits(decoded.z_upper_[i]), bits(original.z_upper_[i])) << i;
    }
    EXPECT_EQ(bits(decoded.iq_values_[0]), bits(original.iq_values_[0]));
}

TEST(IpmPolishSerialization, FrozenByteLayout) {
    const std::vector<std::byte> encoded = serialize_ipm_polish(frozen_value());
    const std::vector<std::byte> expected = frozen_bytes();
    ASSERT_EQ(encoded.size(), kFrozenSize);
    EXPECT_EQ(encoded, expected);
}

TEST(IpmPolishSerialization, DecodesTheFrozenBytesBackToTheFrozenValue) {
    EXPECT_EQ(deserialize_ipm_polish(frozen_bytes()), frozen_value());
}

TEST(IpmPolishSerialization, RefusesAnEmptyInput) {
    const std::string message = refusal_message({});
    EXPECT_TRUE(mentions(message, std::string(kIpmPolishTag))) << message;
    EXPECT_TRUE(mentions(message, "the payload magic")) << message;
}

TEST(IpmPolishSerialization, RefusesABadMagic) {
    std::vector<std::byte> bytes = frozen_bytes();
    bytes[4] = std::byte{0x00};
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "payload magic")) << message;
    EXPECT_TRUE(mentions(message, "byte offset 0")) << message;
    EXPECT_TRUE(mentions(message, std::string(kIpmPolishTag))) << message;
}

TEST(IpmPolishSerialization, RefusesTrailingBytes) {
    std::vector<std::byte> bytes = frozen_bytes();
    bytes.push_back(std::byte{0x00});
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "trailing byte")) << message;
    EXPECT_TRUE(mentions(message, "byte offset " + std::to_string(kFrozenSize))) << message;
}

TEST(IpmPolishSerialization, RefusesALengthTheInputCannotHold) {
    std::vector<std::byte> bytes = frozen_bytes();
    // The multiplication `count * 8` would wrap on a 64-bit size_t for this
    // count; the decoder's division form is what refuses it.
    poke_u64(bytes, kLowerLengthOffset, 0x2000000000000001ULL);
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(
        mentions(message, "the length field at byte offset " + std::to_string(kLowerLengthOffset)))
        << message;
    EXPECT_TRUE(mentions(message, "the lower-bound multiplier block")) << message;
}

// Every PROPER prefix of a valid payload must refuse, and must say where it
// ran out -- the claim that no declared length is used before it is checked,
// made about the whole decoder rather than about the offsets someone
// remembered to list.
class IpmPolishTruncation : public ::testing::TestWithParam<std::size_t> {};

TEST_P(IpmPolishTruncation, RefusesAndNamesWhereItRanOut) {
    const std::size_t prefix = GetParam();
    const std::vector<std::byte> bytes = frozen_bytes();
    ASSERT_LT(prefix, bytes.size());

    const std::string message = refusal_message(std::span<const std::byte>(bytes.data(), prefix));
    EXPECT_TRUE(mentions(message, "deserialize_ipm_polish")) << message;
    EXPECT_TRUE(mentions(message, std::string(kIpmPolishTag))) << message;
    EXPECT_TRUE(mentions(message, "byte offset")) << message;
    const bool names_the_size =
        mentions(message, "ends at byte " + std::to_string(prefix)) ||
        mentions(message, "in a " + std::to_string(prefix) + "-byte payload");
    EXPECT_TRUE(names_the_size) << message;
}

INSTANTIATE_TEST_SUITE_P(EveryProperPrefix, IpmPolishTruncation,
                         ::testing::Range(std::size_t{0}, kFrozenSize));

// ---------------------------------------------------------------------------
// The tag lookup
// ---------------------------------------------------------------------------

TEST(IpmPolishLookup, ReturnsNullOnACoreOnlyValue) {
    const WarmStartData data;
    EXPECT_EQ(find_ipm_polish(data), nullptr);
}

TEST(IpmPolishLookup, IgnoresForeignTagsIncludingANearMiss) {
    WarmStartData data;
    data.extensions_.push_back(WarmExtension{"some.other.producer", bytes_of({0x01, 0x02})});
    // A near miss: the same producer at a DIFFERENT version. The tag is the
    // version, so this one is foreign to a v1 reader.
    data.extensions_.push_back(WarmExtension{"hven.ipm.polish.v2", frozen_bytes()});
    EXPECT_EQ(find_ipm_polish(data), nullptr);
}

TEST(IpmPolishLookup, FindsTheTagAmongOthersAndReturnsItsBytesVerbatim) {
    WarmStartData data;
    data.extensions_.push_back(WarmExtension{"some.other.producer", bytes_of({0x01})});
    data.extensions_.push_back(WarmExtension{std::string(kIpmPolishTag), frozen_bytes()});
    data.extensions_.push_back(WarmExtension{"another.producer", bytes_of({0x02})});

    const WarmExtension *found = find_ipm_polish(data);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->payload_, frozen_bytes());
}

TEST(IpmPolishLookup, RefusesTheTagCarriedTwice) {
    WarmStartData data;
    data.extensions_.push_back(WarmExtension{std::string(kIpmPolishTag), frozen_bytes()});
    data.extensions_.push_back(WarmExtension{std::string(kIpmPolishTag), frozen_bytes()});
    const std::string message = refusal_from([&] { find_ipm_polish(data); });
    EXPECT_TRUE(mentions(message, "more than once")) << message;
    EXPECT_TRUE(mentions(message, std::string(kIpmPolishTag))) << message;
}

// ---------------------------------------------------------------------------
// The crossover bridge
// ---------------------------------------------------------------------------

TEST(IpmPolishBridge, MatchesFromInteriorPointHandedTheSameBlocks) {
    const WarmStartData data = bridgeable_value();
    const IpmPolishData polish = deserialize_ipm_polish(find_ipm_polish(data)->payload_);

    const WarmStart bridged =
        to_sqp_warm_start(data, bridge_lower(), bridge_upper(), data.structure_key_);
    const WarmStart direct =
        from_interior_point(data.primal_, data.eq_lmults_, data.iq_lmults_, polish.iq_values_,
                            polish.z_lower_, polish.z_upper_, bridge_lower(), bridge_upper());

    // Field for field, and EXACTLY: the bridge only forwards, so anything
    // weaker than bit equality here would be hiding an arithmetic step that
    // should not exist.
    expect_same_warm_start(bridged, direct);
}

TEST(IpmPolishBridge, ForwardsTheCrossoverOptions) {
    const WarmStartData data = bridgeable_value();
    hven::solvers::IpCrossoverOptions strict;
    // A dual_tol above every price in the fixture leaves every row and every
    // bound side FREE -- a verdict only the forwarded options can produce.
    strict.dual_tol = 1.0e3;
    const WarmStart bridged =
        to_sqp_warm_start(data, bridge_lower(), bridge_upper(), data.structure_key_, strict);
    EXPECT_TRUE(bridged.qp_working_set.active_ineq().empty());
    for (const std::int8_t state : bridged.bound_active) {
        EXPECT_EQ(state, 0);
    }
}

TEST(IpmPolishBridge, RefusesACoreOnlyValueNamingTheTag) {
    WarmStartData data = bridgeable_value();
    data.extensions_.clear();
    const std::string message = refusal_from(
        [&] { to_sqp_warm_start(data, bridge_lower(), bridge_upper(), data.structure_key_); });
    EXPECT_TRUE(mentions(message, std::string(kIpmPolishTag))) << message;
    EXPECT_TRUE(mentions(message, "does not invert")) << message;
}

TEST(IpmPolishBridge, RefusesAStampThatDoesNotMatchTheModel) {
    const WarmStartData data = bridgeable_value();
    const DeclarationKey other = key_of(0x22);
    const std::string message =
        refusal_from([&] { to_sqp_warm_start(data, bridge_lower(), bridge_upper(), other); });
    EXPECT_TRUE(mentions(message, fmt::format("{:#x}", data.structure_key_.digest()))) << message;
    EXPECT_TRUE(mentions(message, fmt::format("{:#x}", other.digest()))) << message;
}

TEST(IpmPolishBridge, RefusesAMalformedPayloadWithTheDecodesOwnMessage) {
    WarmStartData data = bridgeable_value();
    data.extensions_[0].payload_.resize(20);
    const std::string message = refusal_from(
        [&] { to_sqp_warm_start(data, bridge_lower(), bridge_upper(), data.structure_key_); });
    EXPECT_TRUE(mentions(message, "deserialize_ipm_polish")) << message;
    EXPECT_TRUE(mentions(message, "byte offset")) << message;
}

TEST(IpmPolishBridge, RefusesAnExtensionWhoseWidthsDisagreeWithTheCore) {
    WarmStartData data = bridgeable_value();
    IpmPolishData polish = deserialize_ipm_polish(data.extensions_[0].payload_);
    polish.z_lower_ = vector_of({1.0, 2.0, 3.0});
    data.extensions_[0].payload_ = serialize_ipm_polish(polish);

    const std::string message = refusal_from(
        [&] { to_sqp_warm_start(data, bridge_lower(), bridge_upper(), data.structure_key_); });
    EXPECT_TRUE(mentions(message, "lower-bound multiplier block")) << message;
    EXPECT_TRUE(mentions(message, "holds 3 entries")) << message;
    EXPECT_TRUE(mentions(message, "is 2")) << message;
}

TEST(IpmPolishBridge, RefusesBoundVectorsThatAreNotAtTheCoresWidth) {
    const WarmStartData data = bridgeable_value();
    const std::string upper_message = refusal_from(
        [&] { to_sqp_warm_start(data, bridge_lower(), vector_of({1.0}), data.structure_key_); });
    EXPECT_TRUE(mentions(upper_message, "the upper-bound vector")) << upper_message;

    const std::string lower_message = refusal_from([&] {
        to_sqp_warm_start(data, vector_of({1.0, 2.0, 3.0}), bridge_upper(), data.structure_key_);
    });
    EXPECT_TRUE(mentions(lower_message, "the lower-bound vector")) << lower_message;
}
