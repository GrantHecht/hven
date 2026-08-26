// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The warm-start currency: fieldwise equality, the bit-exactness of its byte
// form's round trip, and every refusal that form owes a caller -- a bad magic,
// a version this build does not read, a truncation at any offset, a declared
// length the input cannot hold, and trailing bytes.
//
// The frozen byte-layout pin at the bottom is the one test here that is not
// written against the implementation: its expected bytes are stated by hand
// from the layout documented in src/warmstart/warm_start_data.cpp, so a
// silent format drift fails it even when serialize and deserialize still
// agree with each other.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include <gtest/gtest.h>

#include "hven/model/structure_identity.h"
#include "hven/warmstart/warm_start_data.h"

using hven::solvers::DeclarationKey;
using hven::solvers::deserialize;
using hven::solvers::kWarmStartFormatVersion;
using hven::solvers::serialize;
using hven::solvers::WarmExtension;
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

/// Bitwise comparison of two payload blocks. `operator==` compares doubles with
/// `==`, which is the right currency test but the wrong round-trip test: a NaN
/// that survived a round trip intact still compares unequal to itself.
bool bitwise_equal(const Eigen::VectorXd &left, const Eigen::VectorXd &right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (Eigen::Index i = 0; i < left.size(); ++i) {
        std::uint64_t left_bits = 0;
        std::uint64_t right_bits = 0;
        std::memcpy(&left_bits, &left[i], sizeof(double));
        std::memcpy(&right_bits, &right[i], sizeof(double));
        if (left_bits != right_bits) {
            return false;
        }
    }
    return true;
}

bool bitwise_equal(const WarmStartData &left, const WarmStartData &right) {
    return bitwise_equal(left.primal_, right.primal_) &&
           bitwise_equal(left.eq_lmults_, right.eq_lmults_) &&
           bitwise_equal(left.iq_lmults_, right.iq_lmults_) &&
           bitwise_equal(left.bound_lmults_, right.bound_lmults_) &&
           left.structure_key_ == right.structure_key_ && left.extensions_ == right.extensions_;
}

WarmStartData make_populated() {
    WarmStartData data;
    data.primal_ = vector_of({1.5, -2.25, 3.0});
    data.eq_lmults_ = vector_of({0.125, -0.5});
    data.iq_lmults_ = vector_of({4.0});
    data.bound_lmults_ = vector_of({-1.0, 0.0, 2.5});
    data.structure_key_ = DeclarationKey{0xA1B2C3D4E5F60718ULL, 0x0F1E2D3C4B5A6978ULL};
    data.extensions_ = {
        WarmExtension{"hven.ipm.polish.v1", bytes_of({0x00, 0x7F, 0xFF, 0x10})},
        WarmExtension{"foreign.tag", bytes_of({0x2A})},
    };
    return data;
}

/// Deserializes and expects a refusal, returning the refusal's message. An
/// accepted input is a failure with an empty message, so the caller's substring
/// checks fail rather than pass vacuously.
std::string refusal_message(std::span<const std::byte> bytes) {
    try {
        const WarmStartData decoded = deserialize(bytes);
        ADD_FAILURE() << "deserialize accepted an input it was expected to refuse (" << bytes.size()
                      << " bytes)";
        return {};
    } catch (const std::invalid_argument &error) {
        return error.what();
    }
}

bool mentions(const std::string &message, const std::string &fragment) {
    return message.find(fragment) != std::string::npos;
}

// ---------------------------------------------------------------------------
// The frozen byte-layout pin's fixture.
//
// The value is small and every field is exercised: a two-element primal block,
// a one-element equality block, an EMPTY inequality block, a bound block
// carrying +0.0 and -0.0 (the pair `operator==` cannot tell apart but the byte
// form must), both stamp conjuncts distinct and byte-asymmetric, and one
// extension with a two-character tag and a three-byte payload.
//
// RE-STATED FOR FORMAT v2: the key slot is two u64 conjuncts where v1 carried
// u64 + i32 + u64, so the encoding is FOUR BYTES SHORTER and every offset past
// the stamp moves down by four. The bytes below are re-derived by hand from the
// layout in src/warmstart/warm_start_data.cpp, not adjusted from the v1 pin.
// ---------------------------------------------------------------------------

WarmStartData frozen_value() {
    WarmStartData data;
    data.primal_ = vector_of({1.0, -2.0});
    data.eq_lmults_ = vector_of({0.5});
    data.iq_lmults_ = Eigen::VectorXd();
    data.bound_lmults_ = vector_of({0.0, -0.0});
    data.structure_key_ = DeclarationKey{0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL};
    data.extensions_ = {WarmExtension{"ab", bytes_of({0x01, 0x02, 0x03})}};
    return data;
}

/// The expected encoding of `frozen_value()`, stated by hand from the layout
/// documented in src/warmstart/warm_start_data.cpp -- all integers fixed-width
/// little-endian, doubles as their IEEE-754 bit patterns in the same byte
/// order. The double patterns are the standard ones: 1.0 is 0x3FF0000000000000,
/// -2.0 is 0xC000000000000000, 0.5 is 0x3FE0000000000000, +0.0 is all zero, and
/// -0.0 is 0x8000000000000000.
std::vector<std::byte> frozen_bytes() {
    return bytes_of({
        // [0] magic: 'H' 'V' 'E' 'N' 'W' 'S' 'D' 0x00
        0x48,
        0x56,
        0x45,
        0x4E,
        0x57,
        0x53,
        0x44,
        0x00,
        // [8] version = 2 (u32)
        0x02,
        0x00,
        0x00,
        0x00,
        // [12] primal_ count = 2 (u64)
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [20] primal_[0] = 1.0
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xF0,
        0x3F,
        // [28] primal_[1] = -2.0
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xC0,
        // [36] eq_lmults_ count = 1 (u64)
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [44] eq_lmults_[0] = 0.5
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0xE0,
        0x3F,
        // [52] iq_lmults_ count = 0 (u64), no elements
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [60] bound_lmults_ count = 2 (u64)
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [68] bound_lmults_[0] = +0.0
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [76] bound_lmults_[1] = -0.0
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x80,
        // [84] structure_key_.declaration_digest_ = 0x0123456789ABCDEF (u64)
        0xEF,
        0xCD,
        0xAB,
        0x89,
        0x67,
        0x45,
        0x23,
        0x01,
        // [92] structure_key_.bound_digest_ = 0xFEDCBA9876543210 (u64)
        0x10,
        0x32,
        0x54,
        0x76,
        0x98,
        0xBA,
        0xDC,
        0xFE,
        // [100] extension count = 1 (u64)
        0x01,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [108] extension[0] tag length = 2 (u64)
        0x02,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [116] extension[0] tag = "ab"
        0x61,
        0x62,
        // [118] extension[0] payload length = 3 (u64)
        0x03,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        0x00,
        // [126] extension[0] payload
        0x01,
        0x02,
        0x03,
        // ends at [129]
    });
}

/// The hand-stated encoding's length, and the number of proper prefixes the
/// truncation suite below sweeps.
constexpr std::size_t kFrozenSize = 129;

// The offsets the length-field refusal tests patch, read off the same
// hand-stated layout.
constexpr std::size_t kVersionOffset = 8;
constexpr std::size_t kPrimalLengthOffset = 12;
constexpr std::size_t kExtensionCountOffset = 100;
constexpr std::size_t kTagLengthOffset = 108;
constexpr std::size_t kPayloadLengthOffset = 118;

void poke_u64(std::vector<std::byte> &bytes, std::size_t offset, std::uint64_t value) {
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
    }
}

void poke_u32(std::vector<std::byte> &bytes, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 0xFFU);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Value semantics
// ---------------------------------------------------------------------------

TEST(WarmStartValue, DefaultIsEmptyAndEqualToAnotherDefault) {
    const WarmStartData left;
    const WarmStartData right;
    EXPECT_EQ(left.primal_.size(), 0);
    EXPECT_EQ(left.eq_lmults_.size(), 0);
    EXPECT_EQ(left.iq_lmults_.size(), 0);
    EXPECT_EQ(left.bound_lmults_.size(), 0);
    EXPECT_TRUE(left.extensions_.empty());
    EXPECT_EQ(left.structure_key_, DeclarationKey{});
    EXPECT_EQ(left, right);
}

TEST(WarmStartValue, ACopyEqualsItsOriginal) {
    const WarmStartData original = make_populated();
    const WarmStartData copy = original;
    EXPECT_EQ(original, copy);
}

TEST(WarmStartValue, EqualityIsFieldwiseOverEveryBlock) {
    const WarmStartData base = make_populated();

    WarmStartData primal = base;
    primal.primal_[1] += 1.0;
    EXPECT_NE(base, primal);

    WarmStartData primal_size = base;
    primal_size.primal_ = vector_of({1.5, -2.25});
    EXPECT_NE(base, primal_size);

    WarmStartData eq = base;
    eq.eq_lmults_[0] = -eq.eq_lmults_[0];
    EXPECT_NE(base, eq);

    WarmStartData iq = base;
    iq.iq_lmults_[0] = 0.0;
    EXPECT_NE(base, iq);

    WarmStartData bound = base;
    bound.bound_lmults_[2] = -bound.bound_lmults_[2];
    EXPECT_NE(base, bound);
}

TEST(WarmStartValue, EqualityIsFieldwiseOverBothStampConjuncts) {
    const WarmStartData base = make_populated();

    WarmStartData declaration = base;
    declaration.structure_key_.declaration_digest_ += 1;
    EXPECT_NE(base, declaration);

    WarmStartData bounds = base;
    bounds.structure_key_.bound_digest_ += 1;
    EXPECT_NE(base, bounds);
}

TEST(WarmStartValue, EqualityIsFieldwiseOverTheExtensionList) {
    const WarmStartData base = make_populated();

    WarmStartData dropped = base;
    dropped.extensions_.pop_back();
    EXPECT_NE(base, dropped);

    WarmStartData retagged = base;
    retagged.extensions_[0].tag_ += "x";
    EXPECT_NE(base, retagged);

    WarmStartData repaid = base;
    repaid.extensions_[1].payload_.push_back(std::byte{0});
    EXPECT_NE(base, repaid);

    WarmStartData reordered = base;
    std::swap(reordered.extensions_[0], reordered.extensions_[1]);
    EXPECT_NE(base, reordered);

    WarmExtension extension{"t", bytes_of({0x01})};
    EXPECT_EQ(extension, (WarmExtension{"t", bytes_of({0x01})}));
    EXPECT_NE(extension, (WarmExtension{"t", bytes_of({0x02})}));
    EXPECT_NE(extension, (WarmExtension{"u", bytes_of({0x01})}));
}

TEST(WarmStartValue, EqualityIsExactSoANaNPayloadIsUnequalToItself) {
    WarmStartData data = make_populated();
    data.primal_[0] = std::numeric_limits<double>::quiet_NaN();
    const WarmStartData copy = data;
    EXPECT_NE(data, copy);
    EXPECT_TRUE(bitwise_equal(data, copy));
}

// ---------------------------------------------------------------------------
// Round trips
// ---------------------------------------------------------------------------

TEST(WarmStartSerialization, RoundTripsAPopulatedValue) {
    const WarmStartData original = make_populated();
    const std::vector<std::byte> bytes = serialize(original);
    const WarmStartData decoded = deserialize(bytes);
    EXPECT_EQ(original, decoded);
}

TEST(WarmStartSerialization, RoundTripsADefaultConstructedValue) {
    const WarmStartData original;
    const std::vector<std::byte> bytes = serialize(original);
    const WarmStartData decoded = deserialize(bytes);
    EXPECT_EQ(original, decoded);
    EXPECT_EQ(decoded.primal_.size(), 0);
    EXPECT_TRUE(decoded.extensions_.empty());
}

TEST(WarmStartSerialization, RoundTripsEmptyBlocksWithAPopulatedStamp) {
    // BOTH CONJUNCTS AT THEIR EXTREMES, and one of them zero: a digest is an
    // FNV-1a value with no reserved bit patterns, so the top bit set and the
    // all-zero value are both real keys the byte form has to carry unchanged.
    WarmStartData original;
    original.structure_key_ = DeclarationKey{0xFFFFFFFFFFFFFFFFULL, 0ULL};
    WarmStartData decoded = deserialize(serialize(original));
    EXPECT_EQ(original, decoded);
    EXPECT_EQ(decoded.structure_key_.declaration_digest_, 0xFFFFFFFFFFFFFFFFULL);
    EXPECT_EQ(decoded.structure_key_.bound_digest_, 0ULL);

    original.structure_key_ = DeclarationKey{0ULL, 0x8000000000000000ULL};
    decoded = deserialize(serialize(original));
    EXPECT_EQ(original, decoded);
    EXPECT_EQ(decoded.structure_key_.bound_digest_, 0x8000000000000000ULL);
}

TEST(WarmStartSerialization, RoundTripsSpecialDoublesBitExactly) {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    WarmStartData original;
    original.primal_ = vector_of({std::numeric_limits<double>::quiet_NaN(),
                                  -std::numeric_limits<double>::quiet_NaN(), kInf, -kInf});
    original.eq_lmults_ = vector_of({0.0, -0.0});
    original.iq_lmults_ =
        vector_of({std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::min(),
                   std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()});
    // A value whose decimal form is not exact: the round trip must be over the
    // bit pattern, not over any decimal rendering of it.
    original.bound_lmults_ = vector_of({0.1, 1.0 / 3.0, 0x1.fffffffffffffp+1023});

    const WarmStartData decoded = deserialize(serialize(original));
    EXPECT_TRUE(bitwise_equal(original, decoded));
    EXPECT_TRUE(std::isnan(decoded.primal_[0]));
    EXPECT_TRUE(std::signbit(decoded.eq_lmults_[1]));
    EXPECT_FALSE(std::signbit(decoded.eq_lmults_[0]));
}

TEST(WarmStartSerialization, RoundTripsAwkwardExtensionsInOrder) {
    WarmStartData original;
    original.extensions_ = {
        WarmExtension{"", {}},
        WarmExtension{std::string("with\0nul", 8), bytes_of({0x00, 0x00})},
        WarmExtension{"hven.ipm.polish.v1", {}},
        WarmExtension{"", bytes_of({0xFF})},
    };
    const WarmStartData decoded = deserialize(serialize(original));
    EXPECT_EQ(original, decoded);
    ASSERT_EQ(decoded.extensions_.size(), 4U);
    EXPECT_EQ(decoded.extensions_[1].tag_.size(), 8U);
    EXPECT_EQ(decoded.extensions_[2].tag_, "hven.ipm.polish.v1");
}

TEST(WarmStartSerialization, TheEncodingIsAFunctionOfTheValueAlone) {
    const WarmStartData original = make_populated();
    const WarmStartData copy = original;
    EXPECT_EQ(serialize(original), serialize(original));
    EXPECT_EQ(serialize(original), serialize(copy));
    EXPECT_EQ(serialize(deserialize(serialize(original))), serialize(original));
}

// ---------------------------------------------------------------------------
// The frozen byte-layout pin
// ---------------------------------------------------------------------------

TEST(WarmStartSerialization, FrozenByteLayout) {
    const std::vector<std::byte> expected = frozen_bytes();
    ASSERT_EQ(expected.size(), kFrozenSize);

    const std::vector<std::byte> actual = serialize(frozen_value());
    ASSERT_EQ(actual.size(), expected.size())
        << "the encoding's length moved: the layout in "
           "src/warmstart/warm_start_data.cpp and this hand-stated pin disagree";
    for (std::size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(actual[i], expected[i]) << "at byte offset " << i;
    }
}

TEST(WarmStartSerialization, DecodesTheFrozenBytesBackToTheFrozenValue) {
    const std::vector<std::byte> bytes = frozen_bytes();
    const WarmStartData decoded = deserialize(bytes);
    EXPECT_EQ(decoded, frozen_value());
    // +0.0 and -0.0 compare equal, so the pin above is what proves the sign
    // survived; this asserts it here too rather than leaving it to `==`.
    ASSERT_EQ(decoded.bound_lmults_.size(), 2);
    EXPECT_FALSE(std::signbit(decoded.bound_lmults_[0]));
    EXPECT_TRUE(std::signbit(decoded.bound_lmults_[1]));
}

TEST(WarmStartSerialization, TheFormatVersionThisBuildWritesIsTwo) {
    EXPECT_EQ(kWarmStartFormatVersion, 2U);
}

// THE v1 REFUSAL, called out on its own rather than left to the sweep below:
// v1 is a format this repository actually wrote, not a hypothetical unknown
// version, so its refusal must name both versions and say re-export (the
// version note in src/warmstart/warm_start_data.cpp has the why).
TEST(WarmStartSerialization, RefusesAVersionOnePayloadNamingBothVersions) {
    std::vector<std::byte> bytes = frozen_bytes();
    poke_u32(bytes, kVersionOffset, 1U);
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "byte offset 8")) << message;
    EXPECT_TRUE(mentions(message, "is 1")) << message;
    EXPECT_TRUE(mentions(message, "version 2")) << message;
    EXPECT_TRUE(mentions(message, "re-export")) << message;
    EXPECT_TRUE(mentions(message, "pre-ruling layout stamp")) << message;
}

// AND THE v1 EXPLANATION IS ABOUT v1 ONLY. A payload from a FUTURE version is
// the opposite situation -- the payload is fine and this build is the old one
// -- so it must not be handed v1's story. Pinned before such a version exists.
TEST(WarmStartSerialization, AVersionAboveThisBuildsIsNotBlamedOnThePreRulingStamp) {
    std::vector<std::byte> bytes = frozen_bytes();
    poke_u32(bytes, kVersionOffset, 3U);
    const std::string message = refusal_message(bytes);
    // Both versions, always.
    EXPECT_TRUE(mentions(message, "byte offset 8")) << message;
    EXPECT_TRUE(mentions(message, "is 3")) << message;
    EXPECT_TRUE(mentions(message, "version 2")) << message;
    // But not v1's story.
    EXPECT_FALSE(mentions(message, "pre-ruling layout stamp")) << message;
    EXPECT_FALSE(mentions(message, "re-export")) << message;
    // The true one instead.
    EXPECT_TRUE(mentions(message, "NEWER format")) << message;
}

// A version this project simply never wrote gets the two numbers and no story:
// there is nothing true to add about it.
TEST(WarmStartSerialization, AVersionZeroPayloadGetsTheNumbersAndNoStory) {
    std::vector<std::byte> bytes = frozen_bytes();
    poke_u32(bytes, kVersionOffset, 0U);
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "is 0")) << message;
    EXPECT_TRUE(mentions(message, "version 2")) << message;
    EXPECT_FALSE(mentions(message, "pre-ruling layout stamp")) << message;
    EXPECT_FALSE(mentions(message, "NEWER format")) << message;
}

// ---------------------------------------------------------------------------
// Refusals
// ---------------------------------------------------------------------------

TEST(WarmStartSerialization, RefusesAnEmptyInput) {
    const std::vector<std::byte> empty;
    const std::string message = refusal_message(empty);
    EXPECT_TRUE(mentions(message, "byte offset 0")) << message;
    EXPECT_TRUE(mentions(message, "the format magic")) << message;
}

TEST(WarmStartSerialization, RefusesABadMagic) {
    std::vector<std::byte> bytes = frozen_bytes();
    bytes[3] = std::byte{0x00};
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "byte offset 0")) << message;
    EXPECT_TRUE(mentions(message, "magic")) << message;
}

TEST(WarmStartSerialization, RefusesAVersionThisBuildDoesNotRead) {
    for (const std::uint32_t version : {0U, 1U, 3U, 0xFFFFFFFFU}) {
        std::vector<std::byte> bytes = frozen_bytes();
        poke_u32(bytes, kVersionOffset, version);
        const std::string message = refusal_message(bytes);
        EXPECT_TRUE(mentions(message, "byte offset 8")) << message;
        EXPECT_TRUE(mentions(message, "version")) << message;
        EXPECT_TRUE(mentions(message, std::to_string(version))) << message;
    }
}

TEST(WarmStartSerialization, RefusesTrailingBytes) {
    std::vector<std::byte> bytes = frozen_bytes();
    bytes.push_back(std::byte{0x00});
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "byte offset 129")) << message;
    EXPECT_TRUE(mentions(message, "trailing")) << message;
}

TEST(WarmStartSerialization, RefusesAVectorLengthTheInputCannotHold) {
    for (const std::uint64_t declared :
         {std::uint64_t{20}, std::uint64_t{1} << 60, std::numeric_limits<std::uint64_t>::max()}) {
        std::vector<std::byte> bytes = frozen_bytes();
        poke_u64(bytes, kPrimalLengthOffset, declared);
        const std::string message = refusal_message(bytes);
        EXPECT_TRUE(mentions(message, "byte offset 12")) << message;
        EXPECT_TRUE(mentions(message, "the primal block")) << message;
        EXPECT_TRUE(mentions(message, std::to_string(declared))) << message;
    }
}

TEST(WarmStartSerialization, RefusesAnExtensionCountTheInputCannotHold) {
    std::vector<std::byte> bytes = frozen_bytes();
    poke_u64(bytes, kExtensionCountOffset, std::numeric_limits<std::uint64_t>::max());
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "byte offset 100")) << message;
    EXPECT_TRUE(mentions(message, "the extension list")) << message;
}

TEST(WarmStartSerialization, RefusesATagLengthTheInputCannotHold) {
    std::vector<std::byte> bytes = frozen_bytes();
    poke_u64(bytes, kTagLengthOffset, 1000);
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "byte offset 108")) << message;
    EXPECT_TRUE(mentions(message, "an extension tag")) << message;
}

TEST(WarmStartSerialization, RefusesAPayloadLengthTheInputCannotHold) {
    std::vector<std::byte> bytes = frozen_bytes();
    poke_u64(bytes, kPayloadLengthOffset, 1000);
    const std::string message = refusal_message(bytes);
    EXPECT_TRUE(mentions(message, "byte offset 118")) << message;
    EXPECT_TRUE(mentions(message, "an extension payload")) << message;
}

// ---------------------------------------------------------------------------
// Truncation, at every offset
//
// Every PROPER prefix of a valid encoding must refuse, and must say where it
// ran out. Sweeping all of them rather than the section boundaries alone is
// what makes "no length is used before it is checked" a claim about the whole
// decoder rather than about the offsets someone remembered to list.
// ---------------------------------------------------------------------------

class WarmStartTruncation : public ::testing::TestWithParam<std::size_t> {};

TEST_P(WarmStartTruncation, RefusesAndNamesWhereItRanOut) {
    const std::size_t prefix = GetParam();
    const std::vector<std::byte> bytes = frozen_bytes();
    ASSERT_LT(prefix, bytes.size());

    const std::string message = refusal_message(std::span<const std::byte>(bytes.data(), prefix));
    EXPECT_TRUE(mentions(message, "deserialize(WarmStartData)")) << message;
    EXPECT_TRUE(mentions(message, "byte offset")) << message;
    // Either the read ran off the end ("the input ends at byte N") or a length
    // field could not be honoured by what remained ("in an N-byte input"); both
    // name the truncated size.
    const bool names_the_size = mentions(message, "ends at byte " + std::to_string(prefix)) ||
                                mentions(message, "in a " + std::to_string(prefix) + "-byte input");
    EXPECT_TRUE(names_the_size) << message;
}

INSTANTIATE_TEST_SUITE_P(EveryProperPrefix, WarmStartTruncation,
                         ::testing::Range(std::size_t{0}, kFrozenSize));
