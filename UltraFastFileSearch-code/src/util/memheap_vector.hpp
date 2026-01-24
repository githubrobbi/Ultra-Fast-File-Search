#ifndef UFFS_MEMHEAP_VECTOR_HPP
#define UFFS_MEMHEAP_VECTOR_HPP

#include <vector>

namespace uffs {

/**
 * @brief A vector wrapper that attempts in-place memory expansion when possible.
 * 
 * This template wraps std::vector and provides custom reserve behavior that
 * attempts to expand memory in-place using MSVC-specific allocator features
 * when the custom memory heap allocator is available.
 * 
 * @tparam T The element type
 * @tparam Alloc The allocator type (defaults to memheap::MemoryHeapAllocator if available)
 */
template <class T, class Alloc = 
#if defined(MEMORY_HEAP_HPP)
    memheap::MemoryHeapAllocator
#else
    std::allocator
#endif
    <T>
>
class memheap_vector : std::vector<T, Alloc>
{
    typedef std::vector<T, Alloc> base_type;
    typedef memheap_vector this_type;

protected:
    typename base_type::size_type _reserve(typename base_type::size_type extra_capacity)
    {
        typename base_type::size_type extra_reserved = 0;
#if defined(MEMORY_HEAP_HPP) && defined(_CPPLIB_VER) && 600 <= _CPPLIB_VER && _CPPLIB_VER <= 699
        typename base_type::size_type const current_capacity = this->base_type::capacity();
        if (current_capacity > 0 && extra_capacity > current_capacity - this->base_type::size())
        {
            extra_capacity = this->base_type::_Grow_to(current_capacity + extra_capacity) - current_capacity;
            if (typename base_type::pointer const ptr = 
                this->base_type::get_allocator().allocate(extra_capacity, this->base_type::_Myend(), true))
            {
                if (ptr == this->base_type::_Myend())
                {
                    this->base_type::_Myend() = ptr + static_cast<typename base_type::difference_type>(extra_capacity);
                    extra_reserved = extra_capacity;
                }
                else
                {
                    this->base_type::get_allocator().deallocate(ptr, extra_capacity);
                }
            }
        }
#else
        (void)extra_capacity;
#endif
        return extra_reserved;
    }

public:
    typedef typename base_type::allocator_type allocator_type;
    typedef typename base_type::value_type value_type;
    typedef typename base_type::reference reference;
    typedef typename base_type::const_reference const_reference;
    typedef typename base_type::iterator iterator;
    typedef typename base_type::const_iterator const_iterator;
    typedef typename base_type::reverse_iterator reverse_iterator;
    typedef typename base_type::const_reverse_iterator const_reverse_iterator;
    typedef typename base_type::size_type size_type;
    typedef typename base_type::difference_type difference_type;

    memheap_vector() : base_type() {}

    memheap_vector(base_type const& other) : base_type(other) {}

    explicit memheap_vector(allocator_type const& alloc) : base_type(alloc) {}

    using base_type::begin;
    using base_type::end;
    using base_type::rbegin;
    using base_type::rend;
    using base_type::size;
    using base_type::empty;
    using base_type::capacity;
    using base_type::clear;

    void swap(this_type& other)
    {
        this->base_type::swap(static_cast<this_type&>(other));
    }

    friend void swap(this_type& me, this_type& other)
    {
        return me.swap(other);
    }

    void reserve(size_type const size)
    {
        typename base_type::size_type const current_size = this->base_type::size(),
            size_difference = size > current_size ? size - current_size : 0;
        if (size_difference && this->_reserve(size_difference) < size_difference)
        {
            this->base_type::reserve(size);
        }
    }

    void push_back(value_type const& value)
    {
        this->_reserve(1);
        return this->base_type::push_back(value);
    }

    void resize(size_type const size, value_type const& fill)
    {
        typename base_type::size_type const current_size = this->base_type::size();
        if (size > current_size)
        {
            this->_reserve(size - current_size);
        }
        return this->base_type::resize(size, fill);
    }

    void resize(size_type const size)
    {
        typename base_type::size_type const current_size = this->base_type::size();
        if (size > current_size)
        {
            this->_reserve(size - current_size);
        }
        return this->base_type::resize(size);
    }
};

// Default allocator type alias for memheap_vector
// Uses MemoryHeapAllocator when available, otherwise std::allocator
template<class T>
using default_memheap_alloc =
#if defined(MEMORY_HEAP_HPP)
    memheap::MemoryHeapAllocator<T>;
#else
    std::allocator<T>;
#endif

} // namespace uffs

#endif // UFFS_MEMHEAP_VECTOR_HPP

