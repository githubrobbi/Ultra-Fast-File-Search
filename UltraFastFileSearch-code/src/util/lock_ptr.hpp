/**
 * @file lock_ptr.hpp
 * @brief RAII lock wrapper template for synchronized object access
 *
 * Provides lock_ptr - a smart pointer that holds a mutex lock while
 * providing access to the pointed-to object. Used with objects that
 * have a get_mutex() method.
 *
 * Extracted from UltraFastFileSearch.cpp during refactoring.
 */

#ifndef UFFS_LOCK_PTR_HPP
#define UFFS_LOCK_PTR_HPP

#include "atomic_compat.hpp"
#include "intrusive_ptr.hpp"

// Type trait to remove volatile qualifier
template <class T> struct remove_volatile
{
    typedef T type;
};

template <class T> struct remove_volatile<T volatile>
{
    typedef T type;
};

// Helper struct for mutable values (used by lock() functions)
template <class T>
struct mutable_
{
    mutable T value;
};

// RAII lock wrapper that provides locked access to objects with get_mutex()
template <class T>
class lock_ptr : atomic_namespace::unique_lock<atomic_namespace::recursive_mutex>
{
    typedef atomic_namespace::unique_lock<atomic_namespace::recursive_mutex> base_type;
    typedef lock_ptr this_type;
    T* me;
    lock_ptr(this_type const&);
    this_type& operator=(this_type const&);
public:
    ~lock_ptr() {}

    lock_ptr(T volatile* const me, bool const do_lock = true) : base_type(), me(const_cast<T*>(me))
    {
        if (do_lock && me)
        {
            base_type temp(me->get_mutex());
            using std::swap;
            swap(temp, static_cast<base_type&>(*this));
        }
    }

    lock_ptr(T* const me, bool const do_lock = true) : base_type(), me(me)
    {
        if (do_lock && me)
        {
            base_type temp(me->get_mutex());
            using std::swap;
            swap(temp, static_cast<base_type&>(*this));
        }
    }

    lock_ptr() : base_type(), me() {}

    T* operator->() const
    {
        return me;
    }

    T& operator*() const
    {
        return *me;
    }

    void swap(this_type& other)
    {
        using std::swap;
        swap(static_cast<base_type&>(*this), static_cast<base_type&>(other));
        swap(this->me, other.me);
    }

    this_type& init(T volatile* const me)
    {
        this_type(me).swap(*this);
        return *this;
    }

    this_type& init(T* const me)
    {
        this_type(me).swap(*this);
        return *this;
    }
};

// Helper function to create a lock_ptr from a raw pointer
template <class T>
lock_ptr<typename remove_volatile<T>::type>& lock(T* const value, mutable_<lock_ptr<typename remove_volatile<T>::type>>
    const& holder = mutable_<lock_ptr<typename remove_volatile<T>::type>>())
{
    return holder.value.init(value);
}

// Helper function to create a lock_ptr from an intrusive_ptr
template <class T>
lock_ptr<typename remove_volatile<T>::type>& lock(intrusive_ptr<T>
    const& value, mutable_<lock_ptr<typename remove_volatile<T>::type>>
    const& holder = mutable_<lock_ptr<typename remove_volatile<T>::type>>())
{
    return lock<T>(value.get(), holder);
}

#endif // UFFS_LOCK_PTR_HPP

