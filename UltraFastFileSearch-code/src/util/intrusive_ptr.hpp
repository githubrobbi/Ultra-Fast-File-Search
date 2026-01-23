#pragma once

// ============================================================================
// Intrusive Reference Counting Smart Pointer
// ============================================================================
// This file contains an intrusive reference-counted smart pointer and
// a base class for reference-counted objects.
// Extracted from UltraFastFileSearch.cpp for better organization.
// ============================================================================

#include <utility>

// Forward declarations for ADL-found functions
template<class T> void intrusive_ptr_add_ref(T const volatile* p);
template<class T> void intrusive_ptr_release(T const volatile* p);

namespace uffs {

// ============================================================================
// intrusive_ptr - Intrusive reference-counted smart pointer
// ============================================================================
// This smart pointer works with objects that have intrusive_ptr_add_ref
// and intrusive_ptr_release functions defined (via ADL).
// ============================================================================
template<class T>
struct intrusive_ptr
{
    typedef T value_type, element_type;
    typedef intrusive_ptr this_type;
    typedef value_type const* const_pointer;
    typedef value_type* pointer;

    pointer p;

    ~intrusive_ptr()
    {
        if (this->p)
        {
            intrusive_ptr_release(this->p);
        }
    }

    void ref()
    {
        if (this->p)
        {
            intrusive_ptr_add_ref(this->p);
        }
    }

    pointer detach()
    {
        pointer p_ = this->p;
        this->p = NULL;
        return p_;
    }

    intrusive_ptr(pointer const p = NULL, bool const addref = true) : p(p)
    {
        if (addref)
        {
            this->ref();
        }
    }

    template<class U>
    intrusive_ptr(intrusive_ptr<U> const& p, bool const addref = true) : p(p.get())
    {
        if (addref)
        {
            this->ref();
        }
    }

    intrusive_ptr(this_type const& other) : p(other.p)
    {
        this->ref();
    }

    this_type& operator=(this_type other)
    {
        return other.swap(*this), *this;
    }

    pointer operator->() const
    {
        return this->p;
    }

    pointer get() const
    {
        return this->p;
    }

    void reset(pointer const p = NULL, bool const add_ref = true)
    {
        this_type(p, add_ref).swap(*this);
    }

    operator pointer()
    {
        return this->p;
    }

    operator const_pointer() const
    {
        return this->p;
    }

    void swap(this_type& other)
    {
        using std::swap;
        swap(this->p, other.p);
    }

    friend void swap(this_type& a, this_type& b)
    {
        return a.swap(b);
    }

    bool operator==(const_pointer const p) const { return this->p == p; }
    friend bool operator==(const_pointer const p, this_type const& me) { return p == me.get(); }

    bool operator!=(const_pointer const p) const { return this->p != p; }
    friend bool operator!=(const_pointer const p, this_type const& me) { return p != me.get(); }

    bool operator<=(const_pointer const p) const { return this->p <= p; }
    friend bool operator<=(const_pointer const p, this_type const& me) { return p <= me.get(); }

    bool operator>=(const_pointer const p) const { return this->p >= p; }
    friend bool operator>=(const_pointer const p, this_type const& me) { return p >= me.get(); }

    bool operator<(const_pointer const p) const { return this->p < p; }
    friend bool operator<(const_pointer const p, this_type const& me) { return p < me.get(); }

    bool operator>(const_pointer const p) const { return this->p > p; }
    friend bool operator>(const_pointer const p, this_type const& me) { return p > me.get(); }
};

} // namespace uffs

