// ============================================================================
// NFormat Extension - Number formatting for tstring/tvstring
// ============================================================================
// Extracted from UltraFastFileSearch.cpp for proper modular architecture
// 
// This extends the base nformat.hpp with NFormat class that supports
// both std::tstring and std::tvstring output types.
// ============================================================================

#pragma once

#ifndef UFFS_NFORMAT_EXT_HPP
#define UFFS_NFORMAT_EXT_HPP

#include "nformat.hpp"           // For basic_iterator_ios
#include "core_types.hpp"        // For std::tstring, std::tvstring

namespace uffs {

/**
 * @brief Base template for NFormat that creates the appropriate ios type
 */
template <class Container>
struct NFormatBase
{
    typedef basic_iterator_ios<std::back_insert_iterator<Container>, typename Container::traits_type> type;
};

/**
 * @brief Number formatter supporting both tstring and tvstring output
 * 
 * Usage:
 * @code
 * NFormat nf(std::locale::classic());
 * std::tstring s = nf(12345);  // "12,345" with grouping
 * @endcode
 */
class NFormat : public NFormatBase<std::tstring>::type, public NFormatBase<std::tvstring>::type
{
    typedef NFormat this_type;
public:
    explicit NFormat(std::locale const& loc)
        : NFormatBase<std::tstring>::type(loc)
        , NFormatBase<std::tvstring>::type(loc)
    {}

    template <class T>
    struct lazy
    {
        this_type const* me;
        T const* value;

        explicit lazy(this_type const* const me, T const& value)
            : me(me), value(&value)
        {}

        operator std::tstring() const
        {
            std::tstring result;
            me->NFormatBase<std::tstring>::type::put(std::back_inserter(result), *value);
            return result;
        }

        operator std::tvstring() const
        {
            std::tvstring result;
            me->NFormatBase<std::tvstring>::type::put(std::back_inserter(result), *value);
            return result;
        }

        template <class String>
        friend String& operator+=(String& out, lazy const& this_)
        {
            this_.me->NFormatBase<String>::type::put(std::back_inserter(out), *this_.value);
            return out;
        }
    };

    template <class T>
    lazy<T> operator()(T const& value) const
    {
        return lazy<T>(this, value);
    }
};

} // namespace uffs

// Expose at global scope for backward compatibility
using uffs::NFormatBase;
using uffs::NFormat;

#endif // UFFS_NFORMAT_EXT_HPP

