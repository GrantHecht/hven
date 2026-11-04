// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// TypeStorage relocates inline Models by their own move constructor, so a
// Model need not be trivially relocatable. The Models here hold exactly the
// members that are not: a std::string (libstdc++ small-string optimisation
// points into the object itself) and a std::vector.

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "hven/detail/interior/fixed_variable_row.h"
#include "hven/core/type_storage.h"
#include "hven/detail/model/nlp_adapter.h"
#include "hven/model/solver_interface_specs.h"

namespace type_storage_test {

struct Base;
using Storage = hven::utils::TypeStorage<Base>;

struct Base {
    virtual ~Base() = default;
    virtual void clone_into(Storage &) const = 0;
    virtual std::string text() const = 0;
    virtual std::size_t count() const = 0;
};

inline int g_live = 0;

struct StringModel final : Base {
    std::string s_;
    std::vector<int> v_;
    explicit StringModel(std::pair<std::string, std::vector<int>> p)
        : s_(std::move(p.first)), v_(std::move(p.second)) {
        ++g_live;
    }
    StringModel(StringModel &&o) noexcept : s_(std::move(o.s_)), v_(std::move(o.v_)) { ++g_live; }
    StringModel(const StringModel &o) : s_(o.s_), v_(o.v_) { ++g_live; }
    ~StringModel() override { --g_live; }
    void clone_into(Storage &s) const override { s.emplace<StringModel>(std::make_pair(s_, v_)); }
    std::string text() const override { return s_; }
    std::size_t count() const override { return v_.size(); }
};

// Inline-sized Model whose move constructor may throw: stored on the heap, so
// the storage's own moves stay noexcept.
struct ThrowingMoveModel final : Base {
    std::string s_;
    explicit ThrowingMoveModel(std::string s) : s_(std::move(s)) { ++g_live; }
    ThrowingMoveModel(ThrowingMoveModel &&o) noexcept(false) : s_(std::move(o.s_)) { ++g_live; }
    ThrowingMoveModel(const ThrowingMoveModel &o) : s_(o.s_) { ++g_live; }
    ~ThrowingMoveModel() override { --g_live; }
    void clone_into(Storage &s) const override { s.emplace<ThrowingMoveModel>(s_); }
    std::string text() const override { return s_; }
    std::size_t count() const override { return 0; }
};

struct BigModel final : Base {
    std::string s_;
    char pad_[512] = {};
    explicit BigModel(std::string s) : s_(std::move(s)) { ++g_live; }
    BigModel(BigModel &&o) noexcept : s_(std::move(o.s_)) { ++g_live; }
    BigModel(const BigModel &o) : s_(o.s_) { ++g_live; }
    ~BigModel() override { --g_live; }
    void clone_into(Storage &s) const override { s.emplace<BigModel>(s_); }
    std::string text() const override { return s_; }
    std::size_t count() const override { return 0; }
};

// A Model that records its own address in every constructor. memcpy relocation
// leaves the recorded address stale; move-construction never does. Fails
// deterministically, with no sanitizer and no UB-dependent crash.
struct SelfPointerModel final : Base {
    const SelfPointerModel *self_;
    int v_;
    explicit SelfPointerModel(int v) : self_(this), v_(v) { ++g_live; }
    SelfPointerModel(SelfPointerModel &&o) noexcept : self_(this), v_(o.v_) { ++g_live; }
    SelfPointerModel(const SelfPointerModel &o) : self_(this), v_(o.v_) { ++g_live; }
    ~SelfPointerModel() override { --g_live; }
    void clone_into(Storage &s) const override { s.emplace<SelfPointerModel>(v_); }
    std::string text() const override { return self_ == this ? "ok" : "stale"; }
    std::size_t count() const override { return static_cast<std::size_t>(v_); }
};

static_assert(sizeof(StringModel) <= 128, "the string Model must land in the inline buffer");
static_assert(std::is_nothrow_move_constructible_v<Storage>);
static_assert(std::is_nothrow_move_assignable_v<Storage>);

// The production Models must stay in the inline buffer, so their moves are
// relocations through the Model move constructor, never heap allocations.
#define HVEN_PIN_INLINE(M)                                                                         \
    static_assert(std::is_nothrow_move_constructible_v<M>, "must stay in the inline buffer");      \
    static_assert(sizeof(M) <= 128, "must stay in the inline buffer");                             \
    static_assert(alignof(M) <= alignof(std::max_align_t), "must stay in the inline buffer")
HVEN_PIN_INLINE(hven::solvers::ObjectiveModel<hven::solvers::NLPObjectivePiece>);
HVEN_PIN_INLINE(hven::solvers::ConstraintModel<hven::solvers::NLPConstraintPiece>);
HVEN_PIN_INLINE(hven::solvers::ConstraintModel<hven::solvers::FixedVariableRow>);
#undef HVEN_PIN_INLINE

const std::string kShort = "short";
const std::string kLong = "a string comfortably longer than any small-string buffer";

Storage make(const std::string &s, std::vector<int> v) {
    Storage st;
    st.emplace<StringModel>(std::make_pair(s, std::move(v)));
    return st;
}

TEST(TypeStorage, InlineNonTriviallyRelocatableModelSurvivesMoves) {
    g_live = 0;
    {
        Storage a = make(kShort, {1, 2, 3});
        Storage b = make(kLong, {4, 5});
        for (int i = 0; i < 6; ++i) {
            Storage c(std::move(a));
            EXPECT_TRUE(a.empty());
            a = std::move(b);
            EXPECT_TRUE(b.empty());
            b = std::move(c);
            EXPECT_TRUE(c.empty());
            // after every move, not only after an even count of rotations
            ASSERT_FALSE(a.empty());
            ASSERT_FALSE(b.empty());
            EXPECT_EQ(a.get().text(), i % 2 == 0 ? kLong : kShort);
            EXPECT_EQ(b.get().text(), i % 2 == 0 ? kShort : kLong);
        }
        // six rotations of (a,b): back where it started after an even count
        EXPECT_EQ(a.get().text(), kShort);
        EXPECT_EQ(a.get().count(), 3u);
        EXPECT_EQ(b.get().text(), kLong);
        EXPECT_EQ(b.get().count(), 2u);
        EXPECT_EQ(g_live, 2);
    }
    EXPECT_EQ(g_live, 0);
}

TEST(TypeStorage, RelocationRunsTheModelMoveConstructor) {
    g_live = 0;
    {
        Storage a;
        a.emplace<SelfPointerModel>(7);
        Storage b(std::move(a));
        EXPECT_EQ(b.get().text(), "ok"); // move ctor
        Storage c;
        c = std::move(b);
        EXPECT_EQ(c.get().text(), "ok"); // move-assign into empty
        Storage d;
        d.emplace<SelfPointerModel>(1);
        d = std::move(c);
        EXPECT_EQ(d.get().text(), "ok"); // move-assign over live
        EXPECT_EQ(d.get().count(), 7u);
        Storage e(d);
        EXPECT_EQ(e.get().text(), "ok"); // copy via clone_into
        EXPECT_EQ(g_live, 2);
    }
    EXPECT_EQ(g_live, 0);
}

TEST(TypeStorage, MoveAssignFromEmptyOverLiveDestroysTargetAndEmptiesIt) {
    g_live = 0;
    {
        Storage a = make(kLong, {1, 2});
        Storage empty;
        EXPECT_EQ(g_live, 1);
        a = std::move(empty);
        EXPECT_TRUE(a.empty());
        EXPECT_EQ(g_live, 0);
    }
    EXPECT_EQ(g_live, 0);
}

TEST(TypeStorage, MoveAssignOverLiveStorageDestroysTheOldModel) {
    g_live = 0;
    {
        Storage a = make(kLong, {1});
        Storage b = make(kShort, {2, 2});
        a = std::move(b);
        EXPECT_EQ(g_live, 1);
        EXPECT_EQ(a.get().text(), kShort);
        EXPECT_EQ(a.get().count(), 2u);
        a = std::move(a); // NOLINT: self-move leaves the value alone
        EXPECT_EQ(a.get().text(), kShort);
    }
    EXPECT_EQ(g_live, 0);
}

TEST(TypeStorage, CopyThenMoveKeepsBothValues) {
    g_live = 0;
    {
        Storage a = make(kShort, {7});
        Storage b(a);
        Storage c;
        c = a;
        Storage d(std::move(b));
        EXPECT_TRUE(b.empty());
        EXPECT_EQ(a.get().text(), kShort);
        EXPECT_EQ(c.get().text(), kShort);
        EXPECT_EQ(d.get().text(), kShort);
        EXPECT_EQ(d.get().count(), 1u);
        EXPECT_EQ(g_live, 3);
    }
    EXPECT_EQ(g_live, 0);
}

TEST(TypeStorage, MovedIntoVectorGrowthKeepsValues) {
    g_live = 0;
    {
        std::vector<Storage> v;
        for (int i = 0; i < 40; ++i) {
            v.push_back(make(i % 2 ? kLong : kShort, std::vector<int>(i, i)));
        }
        for (int i = 0; i < 40; ++i) {
            EXPECT_EQ(v[i].get().text(), i % 2 ? kLong : kShort);
            EXPECT_EQ(v[i].get().count(), static_cast<std::size_t>(i));
        }
        EXPECT_EQ(g_live, 40);
    }
    EXPECT_EQ(g_live, 0);
}

TEST(TypeStorage, ThrowingMoveModelAndOversizeModelMoveToo) {
    g_live = 0;
    {
        Storage a, b;
        a.emplace<ThrowingMoveModel>(kShort);
        b.emplace<BigModel>(kShort);
        Storage c(std::move(a));
        Storage d;
        d = std::move(b);
        EXPECT_EQ(c.get().text(), kShort);
        EXPECT_EQ(d.get().text(), kShort);
        EXPECT_EQ(g_live, 2);
    }
    EXPECT_EQ(g_live, 0);
}

} // namespace type_storage_test
