// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The warm-start currency's byte form: what the layout is, and why it is this
// one. The caller-facing contract is in hven/warmstart/warm_start_data.h.
//
// WHY A BYTE FORM AT ALL. The currency is the value a consumer may pickle,
// send across a process boundary, or store between runs -- that is the whole
// point of making it value-semantic rather than a handle into a live engine.
// A byte form that is not versioned is one that cannot be changed later
// without silently misreading old payloads, so the version leads.
//
// THE LAYOUT. All integers are fixed-width little-endian. All offsets below
// are byte offsets from the start of the encoding.
//
//   0   magic, 8 bytes: 'H' 'V' 'E' 'N' 'W' 'S' 'D' 0x00
//   8   version, u32          -- kWarmStartFormatVersion
//   12  primal_:       u64 count, then count doubles
//       eq_lmults_:    u64 count, then count doubles
//       iq_lmults_:    u64 count, then count doubles
//       bound_lmults_: u64 count, then count doubles
//       structure_key_: u64 declaration_digest_, u64 bound_digest_
//       extensions_:   u64 count, then per extension
//                        u64 tag length, tag bytes,
//                        u64 payload length, payload bytes
//
// WHY THE MAGIC AS WELL AS THE VERSION. A bare version number makes every
// short byte string a plausible payload -- an all-zero buffer reads as
// "version 0" and refuses only because 0 is not a version we write, which is
// luck rather than a check. The magic makes "these are not our bytes" a
// distinct, earlier, and more useful refusal than "these are our bytes at a
// version we do not read".
//
// WHY DOUBLES AS EXPLICIT LITTLE-ENDIAN BIT PATTERNS, rather than a memcpy of
// the native object representation. The currency's whole value is that it
// round-trips bit-exactly -- a payload is a solver iterate, and an iterate
// that came back rounded is a different start point, so NaN payload, infinity
// and the sign of zero all have to survive. Going through the bit pattern
// (std::bit_cast to u64, then an explicit byte order) gets that AND makes the
// encoding a property of the value alone rather than of the host that wrote
// it, which is what lets a frozen byte-layout pin mean anything. On a
// little-endian IEEE-754 host the emitted bytes are exactly the object
// representation, so this costs nothing measurable; the static_assert below
// is what stands between us and a host where "the bit pattern" is not a
// well-defined thing to write down.
//
// WHY EVERY LENGTH IS CHECKED BEFORE IT IS USED. A declared length is
// attacker-or-corruption-controlled input, and this decoder must never read
// past its span (CLAUDE.md section 4: a bounds check, never UB, is the guard).
// The check is written as a division rather than a multiplication -- `count >
// remaining / element_size` -- because `count * element_size` can wrap for a
// large declared count and wrap into a value that passes.
//
// WHY TRAILING BYTES REFUSE. The encoding is self-delimiting, so a decode
// that ends before the input does means the caller handed us something other
// than one payload -- a concatenation, a truncated-then-padded buffer, a
// framing bug. Accepting it would make the first such bug show up later, as a
// wrong answer somewhere else.

#include "hven/warmstart/warm_start_data.h"

#include <array>
#include <bit>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace hven::solvers {

namespace {

static_assert(std::numeric_limits<double>::is_iec559,
              "the warm-start byte form writes IEEE-754 double bit patterns");
static_assert(sizeof(double) == 8, "the warm-start byte form writes 8-byte doubles");
static_assert(sizeof(int) == 4, "the warm-start byte form writes the partition count as an i32");

constexpr std::size_t kMagicSize = 8;
constexpr std::array<unsigned char, kMagicSize> kMagic = {'H', 'V', 'E', 'N', 'W', 'S', 'D', 0x00};

/// The size in bytes of one element of each length-prefixed run, for the
/// bounds check on a declared length.
constexpr std::size_t kDoubleSize = 8;
constexpr std::size_t kByteSize = 1;

/// The fixed size of one extension's two length fields, used only to bound the
/// declared extension COUNT: an extension is at least its two u64 lengths, so a
/// count larger than remaining/16 cannot be honoured no matter what follows.
constexpr std::size_t kMinExtensionSize = 16;

void append_u32(std::vector<std::byte> &out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
    }
}

void append_u64(std::vector<std::byte> &out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
    }
}

void append_double(std::vector<std::byte> &out, double value) {
    append_u64(out, std::bit_cast<std::uint64_t>(value));
}

void append_vector(std::vector<std::byte> &out, const Eigen::VectorXd &vector) {
    append_u64(out, static_cast<std::uint64_t>(vector.size()));
    for (Eigen::Index i = 0; i < vector.size(); ++i) {
        append_double(out, vector[i]);
    }
}

/// A one-pass cursor over the encoding that refuses, rather than reads, past
/// the end of its span. Every accessor names the offset it is at and what it
/// wanted there, so a refusal locates itself in the byte stream.
class Reader {
  public:
    explicit Reader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    std::size_t offset() const noexcept { return offset_; }
    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

    /// @throws std::invalid_argument if fewer than `count` bytes remain.
    std::span<const std::byte> take(std::size_t count, std::string_view what) {
        if (count > remaining()) {
            throw std::invalid_argument(fmt::format(
                "deserialize(WarmStartData): the input ends at byte {0}, but {1} byte(s) were "
                "expected at byte offset {2} for {3}",
                bytes_.size(), count, offset_, what));
        }
        const std::span<const std::byte> taken = bytes_.subspan(offset_, count);
        offset_ += count;
        return taken;
    }

    std::uint32_t read_u32(std::string_view what) {
        const std::span<const std::byte> raw = take(4, what);
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value |= static_cast<std::uint32_t>(std::to_integer<unsigned char>(raw[i])) << (8 * i);
        }
        return value;
    }

    std::uint64_t read_u64(std::string_view what) {
        const std::span<const std::byte> raw = take(8, what);
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(raw[i])) << (8 * i);
        }
        return value;
    }

    /// Reads a u64 count and refuses it here, at the length field's own offset,
    /// if the input cannot hold `count` elements of `element_size` bytes.
    ///
    /// @throws std::invalid_argument on a short read of the length itself, or
    ///         on a length the remaining input cannot hold.
    std::uint64_t read_length(std::string_view what, std::size_t element_size) {
        const std::size_t length_offset = offset_;
        const std::uint64_t count = read_u64(what);
        if (count > static_cast<std::uint64_t>(remaining() / element_size)) {
            throw std::invalid_argument(fmt::format(
                "deserialize(WarmStartData): the length field at byte offset {0} for {1} declares "
                "{2} element(s) of {3} byte(s), but only {4} byte(s) remain in a {5}-byte input",
                length_offset, what, count, element_size, remaining(), bytes_.size()));
        }
        return count;
    }

    Eigen::VectorXd read_vector(std::string_view what) {
        const std::uint64_t count = read_length(what, kDoubleSize);
        Eigen::VectorXd vector(static_cast<Eigen::Index>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            vector[static_cast<Eigen::Index>(i)] = std::bit_cast<double>(read_u64(what));
        }
        return vector;
    }

  private:
    std::span<const std::byte> bytes_;
    std::size_t offset_ = 0;
};

std::string hex_dump(std::span<const std::byte> bytes) {
    std::string text;
    for (const std::byte byte : bytes) {
        fmt::format_to(std::back_inserter(text), "{}{:02x}", text.empty() ? "" : " ",
                       std::to_integer<unsigned char>(byte));
    }
    return text;
}

/// The encoding's exact length. A reserve hint only -- the writer below, not
/// this function, defines the layout, so a drift here costs a reallocation and
/// nothing else.
std::size_t encoded_size_hint(const WarmStartData &data) {
    std::size_t size = kMagicSize + 4; // magic + version
    size += 4 * 8;                     // four block counts
    size += static_cast<std::size_t>(data.primal_.size() + data.eq_lmults_.size() +
                                     data.iq_lmults_.size() + data.bound_lmults_.size()) *
            kDoubleSize;
    size += 8 + 8; // the stamp's two conjuncts
    size += 8;     // the extension count
    for (const WarmExtension &extension : data.extensions_) {
        size += kMinExtensionSize + extension.tag_.size() + extension.payload_.size();
    }
    return size;
}

} // namespace

std::vector<std::byte> serialize(const WarmStartData &data) {
    std::vector<std::byte> out;
    out.reserve(encoded_size_hint(data));
    for (const unsigned char byte : kMagic) {
        out.push_back(static_cast<std::byte>(byte));
    }
    append_u32(out, kWarmStartFormatVersion);

    append_vector(out, data.primal_);
    append_vector(out, data.eq_lmults_);
    append_vector(out, data.iq_lmults_);
    append_vector(out, data.bound_lmults_);

    append_u64(out, data.structure_key_.declaration_digest_);
    append_u64(out, data.structure_key_.bound_digest_);

    append_u64(out, static_cast<std::uint64_t>(data.extensions_.size()));
    for (const WarmExtension &extension : data.extensions_) {
        append_u64(out, static_cast<std::uint64_t>(extension.tag_.size()));
        for (const char character : extension.tag_) {
            out.push_back(static_cast<std::byte>(static_cast<unsigned char>(character)));
        }
        append_u64(out, static_cast<std::uint64_t>(extension.payload_.size()));
        out.insert(out.end(), extension.payload_.begin(), extension.payload_.end());
    }
    return out;
}

WarmStartData deserialize(std::span<const std::byte> bytes) {
    Reader reader(bytes);

    const std::span<const std::byte> magic = reader.take(kMagicSize, "the format magic");
    for (std::size_t i = 0; i < kMagicSize; ++i) {
        if (std::to_integer<unsigned char>(magic[i]) != kMagic[i]) {
            throw std::invalid_argument(fmt::format(
                "deserialize(WarmStartData): expected the 8-byte format magic 48 56 45 4e 57 53 "
                "44 00 (\"HVENWSD\\0\") at byte offset 0, found {0}",
                hex_dump(magic)));
        }
    }

    const std::uint32_t version = reader.read_u32("the format version");
    if (version != kWarmStartFormatVersion) {
        // NAMING BOTH VERSIONS, and refusing rather than adapting. Version 1
        // carried a three-conjunct LAYOUT key where this one carries a
        // two-conjunct DECLARATION key (warm_start_data.h's stamp note); a
        // shim would have to invent a declaration digest out of a claim
        // digest, which is not a thing that can be done, so a v1 payload is
        // re-exported from its producer rather than converted.
        throw std::invalid_argument(
            fmt::format("deserialize(WarmStartData): the format version at byte offset {0} is {1}, "
                        "which this build does not read; it writes and reads version {2}. A "
                        "version {1} payload carries the pre-ruling layout stamp and cannot be "
                        "converted -- re-export it from the engine that produced it.",
                        kMagicSize, version, kWarmStartFormatVersion));
    }

    WarmStartData data;
    data.primal_ = reader.read_vector("the primal block");
    data.eq_lmults_ = reader.read_vector("the equality-multiplier block");
    data.iq_lmults_ = reader.read_vector("the inequality-multiplier block");
    data.bound_lmults_ = reader.read_vector("the bound-multiplier block");

    data.structure_key_.declaration_digest_ = reader.read_u64("the stamp's declaration digest");
    data.structure_key_.bound_digest_ = reader.read_u64("the stamp's bound digest");

    const std::uint64_t extension_count =
        reader.read_length("the extension list", kMinExtensionSize);
    data.extensions_.reserve(static_cast<std::size_t>(extension_count));
    for (std::uint64_t i = 0; i < extension_count; ++i) {
        WarmExtension extension;
        const std::uint64_t tag_size = reader.read_length("an extension tag", kByteSize);
        const std::span<const std::byte> tag =
            reader.take(static_cast<std::size_t>(tag_size), "an extension tag");
        extension.tag_.resize(static_cast<std::size_t>(tag_size));
        if (tag_size > 0) {
            std::memcpy(extension.tag_.data(), tag.data(), static_cast<std::size_t>(tag_size));
        }
        const std::uint64_t payload_size = reader.read_length("an extension payload", kByteSize);
        const std::span<const std::byte> payload =
            reader.take(static_cast<std::size_t>(payload_size), "an extension payload");
        extension.payload_.assign(payload.begin(), payload.end());
        data.extensions_.push_back(std::move(extension));
    }

    if (reader.remaining() != 0) {
        throw std::invalid_argument(
            fmt::format("deserialize(WarmStartData): the payload ends at byte offset {0}, but the "
                        "input carries {1} trailing byte(s) after it",
                        reader.offset(), reader.remaining()));
    }
    return data;
}

} // namespace hven::solvers
