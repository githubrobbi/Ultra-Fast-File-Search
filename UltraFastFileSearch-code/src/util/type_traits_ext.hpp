#pragma once
/**
 * @file type_traits_ext.hpp
 * @brief Extended type traits utilities
 * 
 * Contains:
 * - propagate_const: Propagate const qualifier through pointer types
 * - fast_subscript: Optimized array subscripting for iterators
 * 
 * Extracted from ntfs_index.hpp for reusability.
 */

#ifndef UFFS_TYPE_TRAITS_EXT_HPP
#define UFFS_TYPE_TRAITS_EXT_HPP

#include <iterator>

namespace uffs {

/**
 * @brief Propagate const qualifier from one type to another
 * 
 * If From is const-qualified, the resulting type will be const To.
 * Handles references by stripping them before propagation.
 */
template <class From, class To>
struct propagate_const
{
    typedef To type;
};

template <class From, class To>
struct propagate_const<From const, To> : propagate_const<From, To const> {};

template <class From, class To>
struct propagate_const<From&, To> : propagate_const<From, To> {};

/**
 * @brief Optimized array subscripting for iterators
 * 
 * Returns a pointer to the element at index i, with const propagation
 * from the iterator's reference type.
 * 
 * @tparam It Iterator type
 * @param it Iterator to the beginning
 * @param i Index to access
 * @return Pointer to the element at index i
 */
template <class It>
[[nodiscard]] inline typename propagate_const<
    typename std::iterator_traits<It>::reference,
    typename std::iterator_traits<It>::value_type>::type*
fast_subscript(It const it, size_t const i) noexcept
{
    return &*(it + static_cast<ptrdiff_t>(i));
}

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::propagate_const;
using uffs::fast_subscript;

#endif // UFFS_TYPE_TRAITS_EXT_HPP

