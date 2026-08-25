// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The "hven.ipm.polish.v1" extension's byte form and its crossover bridge. The
// caller-facing contract -- what the extension carries, what each block means,
// and the layout itself -- is in hven/warmstart/ipm_polish_extension.h.
//
// WHY THIS FILE READS LIKE warm_start_data.cpp. It is the same job under a
// different frame, and the arguments that file makes hold verbatim here:
// fixed-width little-endian integers, doubles as explicit IEEE-754 bit
// patterns (so a payload round-trips bit-exactly and the encoding is a
// property of the value rather than of the host), every declared length
// checked against what remains BEFORE it is used, the check written as a
// division so a large count cannot wrap past it, and trailing bytes refused
// because the encoding is self-delimiting. Rather than restate those, this
// file states only what is DIFFERENT.
//
// DIFFERENT 1 -- NO VERSION FIELD. The currency's byte form leads with magic
// AND a version, because the currency is one shape that will grow. This
// payload's version is its TAG: "hven.ipm.polish.v1" names this layout and a
// later layout is "...v2", a tag a v1 reader skips silently by the currency's
// own unknown-tag rule. An internal version would be a second axis that can
// disagree with the first, and the disagreement has no defined resolution.
// The magic stays: it turns a corrupt payload into "this is not a polish
// payload" instead of "the length field at offset 0 declares 2^60 elements".
//
// DIFFERENT 2 -- THE REFUSALS NAME THE TAG. A malformed payload under a KNOWN
// tag is corruption, and corruption is not a capability downgrade: the
// currency's rule that a reader ignores an extension it does not know applies
// to FOREIGN tags. Naming the tag in every message is what keeps the two
// apart for whoever reads the exception.
//
// DIFFERENT 3 -- THIS FILE ALSO CARRIES THE BRIDGE, and deliberately: the
// bridge is the extension's only non-trivial consumer inside this library, and
// the mapping from payload blocks to from_interior_point's arguments is the
// other half of the same contract. Splitting them would put the layout and
// the one thing that reads it in different files for no benefit. The bridge
// itself is a validate-then-forward: every convention, every activity rule and
// every ingest semantic lives in detail/warmstart/warm_start.h, and nothing
// here re-derives any of them.

#include "hven/warmstart/ipm_polish_extension.h"

#include <array>
#include <bit>
#include <cstdint>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

#include <fmt/format.h>

namespace hven::solvers {

namespace {

static_assert(std::numeric_limits<double>::is_iec559,
              "the polish payload writes IEEE-754 double bit patterns");
static_assert(sizeof(double) == 8, "the polish payload writes 8-byte doubles");

constexpr std::size_t kMagicSize = 8;
constexpr std::array<unsigned char, kMagicSize> kMagic = {'H', 'V', 'E', 'N', 'I', 'P', 'P', 0x00};

constexpr std::size_t kDoubleSize = 8;

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

/// A one-pass cursor over the payload that refuses, rather than reads, past
/// the end of its span. Every accessor names the offset it is at, what it
/// wanted there, and the tag the bytes arrived under.
class Reader {
  public:
    explicit Reader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    std::size_t offset() const noexcept { return offset_; }
    std::size_t remaining() const noexcept { return bytes_.size() - offset_; }

    /// @throws std::invalid_argument if fewer than `count` bytes remain.
    std::span<const std::byte> take(std::size_t count, std::string_view what) {
        if (count > remaining()) {
            throw std::invalid_argument(
                fmt::format("deserialize_ipm_polish: the \"{0}\" payload ends at byte {1}, but {2} "
                            "byte(s) were expected at byte offset {3} for {4}",
                            kIpmPolishTag, bytes_.size(), count, offset_, what));
        }
        const std::span<const std::byte> taken = bytes_.subspan(offset_, count);
        offset_ += count;
        return taken;
    }

    std::uint64_t read_u64(std::string_view what) {
        const std::span<const std::byte> raw = take(8, what);
        std::uint64_t value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<std::uint64_t>(std::to_integer<unsigned char>(raw[i])) << (8 * i);
        }
        return value;
    }

    double read_double(std::string_view what) { return std::bit_cast<double>(read_u64(what)); }

    /// Reads a u64 count and refuses it here, at the length field's own offset,
    /// if the input cannot hold `count` doubles.
    Eigen::VectorXd read_vector(std::string_view what) {
        const std::size_t length_offset = offset_;
        const std::uint64_t count = read_u64(what);
        if (count > static_cast<std::uint64_t>(remaining() / kDoubleSize)) {
            throw std::invalid_argument(fmt::format(
                "deserialize_ipm_polish: the length field at byte offset {0} for {1} in the "
                "\"{2}\" payload declares {3} element(s) of {4} byte(s), but only {5} byte(s) "
                "remain in a {6}-byte payload",
                length_offset, what, kIpmPolishTag, count, kDoubleSize, remaining(),
                bytes_.size()));
        }
        Eigen::VectorXd vector(static_cast<Eigen::Index>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            vector[static_cast<Eigen::Index>(i)] = read_double(what);
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

} // namespace

std::vector<std::byte> serialize_ipm_polish(const IpmPolishData &polish) {
    std::vector<std::byte> out;
    out.reserve(kMagicSize + kDoubleSize + 3 * 8 +
                static_cast<std::size_t>(polish.z_lower_.size() + polish.z_upper_.size() +
                                         polish.iq_values_.size()) *
                    kDoubleSize);
    for (const unsigned char byte : kMagic) {
        out.push_back(static_cast<std::byte>(byte));
    }
    append_double(out, polish.mu_);
    append_vector(out, polish.z_lower_);
    append_vector(out, polish.z_upper_);
    append_vector(out, polish.iq_values_);
    return out;
}

IpmPolishData deserialize_ipm_polish(std::span<const std::byte> bytes) {
    Reader reader(bytes);

    const std::span<const std::byte> magic = reader.take(kMagicSize, "the payload magic");
    for (std::size_t i = 0; i < kMagicSize; ++i) {
        if (std::to_integer<unsigned char>(magic[i]) != kMagic[i]) {
            throw std::invalid_argument(fmt::format(
                "deserialize_ipm_polish: expected the 8-byte payload magic 48 56 45 4e 49 50 50 "
                "00 (\"HVENIPP\\0\") at byte offset 0 of the \"{0}\" payload, found {1}",
                kIpmPolishTag, hex_dump(magic)));
        }
    }

    IpmPolishData polish;
    polish.mu_ = reader.read_double("the barrier parameter");
    polish.z_lower_ = reader.read_vector("the lower-bound multiplier block");
    polish.z_upper_ = reader.read_vector("the upper-bound multiplier block");
    polish.iq_values_ = reader.read_vector("the inequality-value block");

    if (reader.remaining() != 0) {
        throw std::invalid_argument(
            fmt::format("deserialize_ipm_polish: the \"{0}\" payload ends at byte offset {1}, but "
                        "the input carries {2} trailing byte(s) after it",
                        kIpmPolishTag, reader.offset(), reader.remaining()));
    }
    return polish;
}

const WarmExtension *find_ipm_polish(const WarmStartData &data) {
    const WarmExtension *found = nullptr;
    for (const WarmExtension &extension : data.extensions_) {
        if (extension.tag_ != kIpmPolishTag) {
            continue;
        }
        if (found != nullptr) {
            // Not arbitrated silently. One tag naming two payloads is a value
            // no producer in this library emits, and picking either one would
            // be a guess a later wrong answer could be traced back to.
            throw std::invalid_argument(
                fmt::format("find_ipm_polish: the warm-start value carries the \"{0}\" extension "
                            "more than once; exactly one payload may travel under one tag",
                            kIpmPolishTag));
        }
        found = &extension;
    }
    return found;
}

WarmStart to_sqp_warm_start(const WarmStartData &data, const Vec &lower, const Vec &upper,
                            const DeclarationKey &structure_key, const IpCrossoverOptions &opts) {
    const WarmExtension *extension = find_ipm_polish(data);
    if (extension == nullptr) {
        // A CORE-ONLY VALUE CANNOT BE BRIDGED, and this is a refusal rather
        // than a degraded best effort: the core carries only the signed
        // z = zL - zU, and the two blocks from_interior_point judges each bound
        // side against are exactly what that difference does not contain. A
        // consumer wanting a core-only start has one -- it stages the value
        // into an engine -- and it does not come through here.
        throw std::invalid_argument(
            fmt::format("to_sqp_warm_start: the warm-start value carries no \"{0}\" extension, so "
                        "the (z_lower, z_upper) pair the crossover judges each bound side against "
                        "is not available; the core's signed z does not invert into it",
                        kIpmPolishTag));
    }

    // THE STAMP, before anything is decoded. `lower`/`upper` describe a model,
    // the value describes a solve, and if the two were not taken under one
    // declared structure then every index below names a different variable in
    // each of them.
    if (!(data.structure_key_ == structure_key)) {
        throw std::invalid_argument(fmt::format(
            "to_sqp_warm_start: the warm-start value was taken under declaration key {0:#x} but "
            "the bounds handed in belong to a model keying {1:#x} -- the value describes a "
            "different declared problem. The key covers the declared dimensions (with any "
            "fixed-variable treatment's own rows subtracted) and the declared bound STRUCTURE, so "
            "one of those moved",
            data.structure_key_.digest(), structure_key.digest()));
    }

    const IpmPolishData polish = deserialize_ipm_polish(extension->payload_);

    const Eigen::Index n = data.primal_.size();
    const Eigen::Index mi = data.iq_lmults_.size();
    const auto check = [&](const char *what, Eigen::Index held, Eigen::Index expected,
                           const char *expected_name) {
        if (held != expected) {
            throw std::invalid_argument(
                fmt::format("to_sqp_warm_start: {0} holds {1} entries but {2} is {3}; every block "
                            "of a warm-start value and of its \"{4}\" extension is stated over the "
                            "DECLARED problem, at exactly its dimensions",
                            what, held, expected_name, expected, kIpmPolishTag));
        }
    };
    check("the extension's lower-bound multiplier block", polish.z_lower_.size(), n,
          "the core's primal width n");
    check("the extension's upper-bound multiplier block", polish.z_upper_.size(), n,
          "the core's primal width n");
    check("the extension's inequality-value block", polish.iq_values_.size(), mi,
          "the core's inequality width mi");
    check("the lower-bound vector", lower.size(), n, "the core's primal width n");
    check("the upper-bound vector", upper.size(), n, "the core's primal width n");

    // FORWARD, and nothing else. `slack_i` is the cI(x) VALUES -- this
    // project's own convention, which is what the extension carries and what
    // from_interior_point's SIGN CONVENTION paragraph demands; the (z_lower,
    // z_upper) -> signed z conversion, the activity inference, the kFixed
    // choice and the kSeeded consequence all live there.
    return from_interior_point(data.primal_, data.eq_lmults_, data.iq_lmults_, polish.iq_values_,
                               polish.z_lower_, polish.z_upper_, lower, upper, opts);
}

} // namespace hven::solvers
