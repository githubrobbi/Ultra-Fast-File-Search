// ============================================================================
// allocators.hpp - Dynamic allocator classes for UFFS
// ============================================================================
// Extracted from UltraFastFileSearch.cpp
// Contains:
//   - DynamicAllocator: Abstract base class for dynamic allocation
//   - dynamic_allocator<T>: Template allocator wrapping std::allocator<T>
//   - SingleMovableGlobalAllocator: Windows HGLOBAL-based allocator with recycling
// ============================================================================
#pragma once

#ifndef UFFS_ALLOCATORS_HPP
#define UFFS_ALLOCATORS_HPP

#include <memory>
#include <cstddef>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace uffs {

// ============================================================================
// DynamicAllocator - Abstract base class for dynamic allocation
// ============================================================================
class DynamicAllocator
{
protected:
    ~DynamicAllocator() { }
public:
    typedef void *pointer;
    typedef void const *const_pointer;
    virtual void deallocate(pointer const p, size_t const n) = 0;
    virtual pointer allocate(size_t const n, pointer const hint = nullptr) = 0;
    virtual pointer reallocate(pointer const p, size_t const n, bool const allow_movement) = 0;
};

// ============================================================================
// dynamic_allocator<T> - Template allocator wrapping std::allocator<T>
// ============================================================================
template <class T>
class dynamic_allocator : private std::allocator<T>
{
    typedef dynamic_allocator this_type;
    typedef std::allocator<T> base_type;
    typedef DynamicAllocator dynamic_allocator_type;
    dynamic_allocator_type* _alloc; /*Keep this IMMUTABLE, since otherwise we need to share state via heap allocation */
public:
    typedef typename base_type::value_type value_type;
    typedef typename base_type::const_pointer const_pointer;
    typedef typename base_type::const_reference const_reference;
    typedef typename base_type::difference_type difference_type;
    typedef typename base_type::pointer pointer;
    typedef typename base_type::reference reference;
    typedef typename base_type::size_type size_type;
    
    dynamic_allocator() : base_type(), _alloc() {}

    explicit dynamic_allocator(dynamic_allocator_type* const alloc) : base_type(), _alloc(alloc) {}

    template <class U>
    dynamic_allocator(dynamic_allocator<U> const& other) : base_type(other.base()), _alloc(other.dynamic_alloc()) {}

    template <class U>
    struct rebind
    {
        typedef dynamic_allocator<U> other;
    };

    dynamic_allocator_type* dynamic_alloc() const
    {
        return this->_alloc;
    }

    base_type& base()
    {
        return static_cast<base_type&>(*this);
    }

    base_type const& base() const
    {
        return static_cast<base_type const&>(*this);
    }

    pointer allocate(size_type const n, void* const p = nullptr)
    {
        pointer r;
        if (this->_alloc)
        {
            r = static_cast<pointer>(this->_alloc->allocate(n * sizeof(value_type), static_cast<typename dynamic_allocator_type::pointer>(p)));
        }
        else
        {
            r = this->base_type::allocate(n, p);
        }
        return r;
    }

    void deallocate(pointer const p, size_type const n)
    {
        if (this->_alloc)
        {
            this->_alloc->deallocate(p, n * sizeof(value_type));
        }
        else
        {
            this->base_type::deallocate(p, n);
        }
    }

    pointer reallocate(pointer const p, size_t const n, bool const allow_movement)
    {
        if (this->_alloc)
        {
            return static_cast<pointer>(this->_alloc->reallocate(p, n * sizeof(value_type), allow_movement));
        }
        else
        {
            return nullptr;
        }
    }

    bool operator==(this_type const& other) const
    {
        return static_cast<base_type const&>(*this) == static_cast<base_type const&>(other);
    }

    bool operator!=(this_type const& other) const
    {
        return static_cast<base_type const&>(*this) != static_cast<base_type const&>(other);
    }

    template <class U>
    this_type& operator=(dynamic_allocator<U> const& other)
    {
        return static_cast<this_type&>(this->base_type::operator=(other.base()));
    }

    using base_type::construct;
    using base_type::destroy;
    using base_type::address;
    using base_type::max_size;
};

// ============================================================================
// SingleMovableGlobalAllocator - Windows HGLOBAL-based allocator with recycling
// ============================================================================
class SingleMovableGlobalAllocator : public DynamicAllocator
{
    SingleMovableGlobalAllocator(SingleMovableGlobalAllocator const&);
    SingleMovableGlobalAllocator& operator=(SingleMovableGlobalAllocator const&);
    HGLOBAL _recycled;
public:
    ~SingleMovableGlobalAllocator()
    {
        if (this->_recycled)
        {
            GlobalFree(this->_recycled);
        }
    }

    SingleMovableGlobalAllocator() : _recycled() {}

    bool disown(HGLOBAL const h)
    {
        bool const same = h == this->_recycled;
        if (same)
        {
            this->_recycled = nullptr;
        }
        return same;
    }

    void deallocate(pointer const p, size_t const n)
    {
        if (n)
        {
            if (HGLOBAL const h = GlobalHandle(p))
            {
                GlobalUnlock(h);
                if (!this->_recycled)
                {
                    this->_recycled = h;
                }
                else
                {
                    GlobalFree(h);
                }
            }
        }
    }

    pointer allocate(size_t const n, pointer const hint = nullptr)
    {
        HGLOBAL mem = nullptr;
        if (n)
        {
            if (this->_recycled)
            {
                mem = GlobalReAlloc(this->_recycled, n, GMEM_MOVEABLE);
                if (mem)
                {
                    this->_recycled = nullptr;
                }
            }

            if (!mem)
            {
                mem = GlobalAlloc(GMEM_MOVEABLE, n);
            }
        }

        pointer p = nullptr;
        if (mem)
        {
            p = GlobalLock(mem);
        }

        return p;
    }

    pointer reallocate(pointer const p, size_t const n, bool const allow_movement)
    {
        pointer result = nullptr;
        if (HGLOBAL h = GlobalHandle(p))
        {
            if (allow_movement || GlobalUnlock(h))
            {
                if (HGLOBAL const r = GlobalReAlloc(h, n, GMEM_MOVEABLE))
                {
                    result = GlobalLock(r);
                }
            }
        }

        return result;
    }
};

} // namespace uffs

#endif // UFFS_ALLOCATORS_HPP

