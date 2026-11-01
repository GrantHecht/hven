// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// General-purpose SBO container with value semantics: the storage behind this
// library's erased function handles (ConstraintInterface / ObjectiveInterface,
// see detail/solvers/solver_interface_specs.h), available to any consumer
// handle built on the same clone_into convention.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace hven::utils {

/// @brief Type-erasing SBO (small-buffer-optimised) container with value semantics.
///
/// Stores any object derived from `C` either inline (if `sizeof(Model) <= SBO_CAP`
/// and alignment is compatible, and its move constructor is `noexcept`) or on the
/// heap. Moving inline storage move-constructs the Model into the destination through
/// a function captured at `emplace` time, so a Model need not be trivially relocatable
/// (`std::string`, `std::vector` and the like are fine). Moves of the container are
/// `noexcept`; `get()` is unaffected.
///
/// `C` must provide `virtual void clone_into(TypeStorage<C, SBO_CAP>&) const` for
/// copy construction and copy assignment. Each concrete Model implements this as
/// `s.emplace<Model>(data_)`.
///
/// @tparam C       Abstract base class of the stored polymorphic object.
/// @tparam SBO_CAP Inline buffer size in bytes (default 128). Objects larger than
///                 this are allocated on the heap.
template <typename C, std::size_t SBO_CAP = 128> class TypeStorage {
    static constexpr std::size_t SBO_ALIGN = alignof(std::max_align_t);
    enum class Kind : uint8_t { Empty, Inline, Heap };

    Kind kind_ = Kind::Empty;
    // Inline only: move-constructs the Model at `src` into raw `dst`, then destroys `src`.
    void (*relocate_)(void *dst, void *src) noexcept = nullptr;
    union {
        alignas(SBO_ALIGN) std::byte buf_[SBO_CAP];
        C *ptr_;
    };

    C *inline_ptr() noexcept { return reinterpret_cast<C *>(buf_); }
    const C *inline_ptr() const noexcept { return reinterpret_cast<const C *>(buf_); }

    void destroy() noexcept {
        if (kind_ == Kind::Inline) {
            inline_ptr()->~C();
        } else if (kind_ == Kind::Heap) {
            delete ptr_;
        }
        kind_ = Kind::Empty;
    }

    // Take over `o`'s contents; *this must be empty.
    void take(TypeStorage &o) noexcept {
        if (o.kind_ == Kind::Inline) {
            o.relocate_(buf_, o.buf_);
            relocate_ = o.relocate_;
        } else if (o.kind_ == Kind::Heap) {
            ptr_ = o.ptr_;
        }
        kind_ = o.kind_;
        o.kind_ = Kind::Empty;
    }

  public:
    /// @brief Construct an empty container.
    TypeStorage() noexcept = default;

    /// @brief Construct a `Model(obj)` in the SBO buffer (if it fits) or on the heap.
    ///
    /// `Model` must derive from `C` and be constructible from `T`. It is stored inline
    /// only if it fits and is nothrow-move-constructible, so the container's moves
    /// stay `noexcept`.
    ///
    /// @tparam Model Concrete derived type to construct.
    /// @tparam T     Type of the forwarded argument used to construct `Model`.
    /// @param obj    Value moved into the new `Model` instance.
    template <typename Model, typename T> void emplace(T obj) {
        destroy();
        if constexpr (sizeof(Model) <= SBO_CAP && alignof(Model) <= SBO_ALIGN &&
                      std::is_nothrow_move_constructible_v<Model>) {
            ::new (static_cast<void *>(buf_)) Model(std::move(obj));
            relocate_ = [](void *dst, void *src) noexcept {
                Model *m = static_cast<Model *>(src);
                ::new (dst) Model(std::move(*m));
                m->~Model();
            };
            kind_ = Kind::Inline;
        } else {
            ptr_ = new Model(std::move(obj));
            kind_ = Kind::Heap;
        }
    }

    /// @brief Deep-copy constructor — invokes `clone_into` on the stored object.
    TypeStorage(const TypeStorage &o) : kind_(Kind::Empty) {
        if (!o.empty()) {
            o.get().clone_into(*this);
        }
    }

    /// @brief Move constructor — relocates inline storage via the Model's move constructor,
    /// steals the pointer for heap storage.
    TypeStorage(TypeStorage &&o) noexcept : kind_(Kind::Empty) { take(o); }

    /// @brief Copy-assignment operator — destroys current contents, then deep-copies via
    /// `clone_into`.
    TypeStorage &operator=(const TypeStorage &o) {
        if (this != &o) {
            destroy();
            if (!o.empty()) {
                o.get().clone_into(*this);
            }
        }
        return *this;
    }

    /// @brief Move-assignment operator — destroys current contents, then steals from `o`.
    TypeStorage &operator=(TypeStorage &&o) noexcept {
        if (this != &o) {
            destroy();
            take(o);
        }
        return *this;
    }

    ~TypeStorage() { destroy(); }

    /// @brief Returns true if no object is currently stored.
    [[nodiscard]] bool empty() const noexcept { return kind_ == Kind::Empty; }

    /// @brief Return a const reference to the stored object.
    ///
    /// Behaviour is undefined if `empty()` is true.
    const C &get() const noexcept {
        if (kind_ == Kind::Inline) {
            return *inline_ptr();
        }
        return *ptr_;
    }

    /// @brief Return a mutable reference to the stored object.
    ///
    /// Behaviour is undefined if `empty()` is true.
    C &get() noexcept {
        if (kind_ == Kind::Inline) {
            return *inline_ptr();
        }
        return *ptr_;
    }

    /// @brief Return a const pointer to the stored object, or `nullptr` if empty.
    const C *get_ptr() const noexcept {
        if (kind_ == Kind::Inline) {
            return inline_ptr();
        }
        return ptr_;
    }

    /// @brief Return a mutable pointer to the stored object, or `nullptr` if empty.
    C *get_ptr() noexcept {
        if (kind_ == Kind::Inline) {
            return inline_ptr();
        }
        return ptr_;
    }
};

/// @internal
/// @brief Concept satisfied when `C` exposes `clone_into(TypeStorage<C,N>&) const`.
template <typename C, std::size_t N>
concept TypeStorageBase = requires(const C &c, TypeStorage<C, N> &s) { c.clone_into(s); };

} // namespace hven::utils
