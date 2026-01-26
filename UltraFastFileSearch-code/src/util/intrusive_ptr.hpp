#pragma once

// ============================================================================
// Intrusive Reference Counting Smart Pointer and RefCounted Base Class
// ============================================================================
// Extracted from UltraFastFileSearch.cpp
// Contains:
//   - intrusive_ptr<T> - intrusive reference-counted smart pointer
//   - RefCounted<Derived> - CRTP base class for reference-counted objects
// ============================================================================

#include "atomic_compat.hpp"

template<class T>
struct intrusive_ptr
{
    typedef T value_type, element_type;
    typedef intrusive_ptr this_type;
    typedef value_type const* const_pointer;
    typedef value_type* pointer;
    pointer p;

    ~intrusive_ptr() { if (this->p) { intrusive_ptr_release(this->p); } }

    void ref() { if (this->p) { intrusive_ptr_add_ref(this->p); } }

    [[nodiscard]] pointer detach() noexcept { pointer p_ = this->p; this->p = nullptr; return p_; }

    intrusive_ptr(pointer const p = nullptr, bool const addref = true) : p(p)
    { if (addref) { this->ref(); } }

    template<class U>
    intrusive_ptr(intrusive_ptr<U> const& p, bool const addref = true) : p(p.get())
    { if (addref) { this->ref(); } }

    intrusive_ptr(this_type const& other) : p(other.p) { this->ref(); }

    // Move constructor
    intrusive_ptr(this_type&& other) noexcept : p(other.p) { other.p = nullptr; }

    this_type& operator=(this_type other) noexcept { return other.swap(*this), *this; }

    [[nodiscard]] pointer operator->() const noexcept { return this->p; }
    [[nodiscard]] pointer get() const noexcept { return this->p; }
    [[nodiscard]] explicit operator bool() const noexcept { return this->p != nullptr; }

    void reset(pointer const p = nullptr, bool const add_ref = true)
    { this_type(p, add_ref).swap(*this); }

    operator pointer() noexcept { return this->p; }
    operator const_pointer() const noexcept { return this->p; }

    void swap(this_type& other) noexcept { using std::swap; swap(this->p, other.p); }
    friend void swap(this_type& a, this_type& b) noexcept { return a.swap(b); }

    [[nodiscard]] bool operator==(const_pointer const p) const noexcept { return this->p == p; }
    [[nodiscard]] friend bool operator==(const_pointer const p, this_type const& me) noexcept { return p == me.get(); }

    [[nodiscard]] bool operator!=(const_pointer const p) const noexcept { return this->p != p; }
    [[nodiscard]] friend bool operator!=(const_pointer const p, this_type const& me) noexcept { return p != me.get(); }

    [[nodiscard]] bool operator<=(const_pointer const p) const noexcept { return this->p <= p; }
    [[nodiscard]] friend bool operator<=(const_pointer const p, this_type const& me) noexcept { return p <= me.get(); }

    [[nodiscard]] bool operator>=(const_pointer const p) const noexcept { return this->p >= p; }
    [[nodiscard]] friend bool operator>=(const_pointer const p, this_type const& me) noexcept { return p >= me.get(); }

    [[nodiscard]] bool operator<(const_pointer const p) const noexcept { return this->p < p; }
    [[nodiscard]] friend bool operator<(const_pointer const p, this_type const& me) noexcept { return p < me.get(); }

    [[nodiscard]] bool operator>(const_pointer const p) const noexcept { return this->p > p; }
    [[nodiscard]] friend bool operator>(const_pointer const p, this_type const& me) noexcept { return p > me.get(); }
};

// ============================================================================
// RefCounted - CRTP base class for intrusive reference counting
// ============================================================================
// Uses atomic operations for thread-safe reference counting.
// The intrusive_ptr_add_ref and intrusive_ptr_release friend functions
// are found via ADL when used with intrusive_ptr<T>.
// ============================================================================
template<class Derived>
class RefCounted
{
    mutable atomic_namespace::atomic<unsigned int> refs;

    friend void intrusive_ptr_add_ref(RefCounted const volatile* p)
    {
        p->refs.fetch_add(1, atomic_namespace::memory_order_acq_rel);
    }

    friend void intrusive_ptr_release(RefCounted const volatile* p)
    {
        if (p->refs.fetch_sub(1, atomic_namespace::memory_order_acq_rel) - 1 == 0)
        {
            delete static_cast<Derived const volatile*>(p);
        }
    }

protected:
    RefCounted() : refs(0) {}
    RefCounted(RefCounted const&) : refs(0) {}
    ~RefCounted() {}
    RefCounted& operator=(RefCounted const&) { return *this; }
    void swap(RefCounted&) {}
};

