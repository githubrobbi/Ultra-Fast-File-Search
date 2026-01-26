#pragma once
/**
 * @file core_types.hpp
 * @brief Core type definitions for UFFS
 *
 * Contains:
 * - basic_vector_based_string<T> - Performance-optimized string class
 * - std::tstring - TCHAR-based standard string
 * - std::tvstring - TCHAR-based vector string with custom allocator
 */

#ifndef UFFS_CORE_TYPES_HPP
#define UFFS_CORE_TYPES_HPP

#include <tchar.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <utility>

#include "allocators.hpp"

/**
 * @brief A string class that uses std::vector as the underlying container in Release mode
 *        for performance, but std::basic_string in Debug mode for easier debugging.
 *
 * In Release mode, this provides better performance by avoiding SSO overhead.
 * In Debug mode, we get std::basic_string's debugging facilities.
 */
template<class T, class Traits = std::char_traits<T>, class Ax = std::allocator<T>>
class basic_vector_based_string : public

#ifdef _DEBUG
	std::basic_string<T, Traits, Ax>
#else
	std::vector<T, Ax>
#endif

{
	typedef basic_vector_based_string this_type;
	typedef std::basic_string<T, Traits, Ax> string_type;
	typedef
#ifdef _DEBUG
	string_type
#else
	std::vector<T, Ax>
#endif
	base_type;

public:

	typedef typename base_type::size_type size_type;
	typedef typename base_type::allocator_type allocator_type;

	typedef typename base_type::value_type
		const* const_pointer;

	typedef typename base_type::value_type value_type;

	static size_type
		const npos = ~size_type();

	basic_vector_based_string() : base_type() {}

	explicit basic_vector_based_string(const_pointer
		const value, size_t
		const n = npos) : base_type(value, value + static_cast<ptrdiff_t> (n == npos ? Traits::length(value) : n)) {}

	explicit basic_vector_based_string(size_type
		const n) : base_type(n) {}

	explicit basic_vector_based_string(size_type
		const n, value_type
		const& value) : base_type(n, value) {}

	explicit basic_vector_based_string(allocator_type
		const& ax) : base_type(ax) {}

	explicit basic_vector_based_string(size_type
		const n, allocator_type
		const& ax) : base_type(n, ax) {}

	explicit basic_vector_based_string(size_type
		const n, value_type
		const& value, allocator_type
		const& ax) : base_type(n, value, ax) {}

	using base_type::insert;
	using base_type::operator=;

#ifndef _DEBUG
	typedef Traits traits_type;

	typedef typename base_type::difference_type difference_type;
	typedef typename base_type::iterator iterator;
	typedef typename base_type::const_iterator const_iterator;

	using base_type::erase;

	void append(size_t
		const n, value_type
		const& value)
	{
		if (!n)
		{
			return;
		}

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)

		if (this->_End - this->_Last < static_cast<ptrdiff_t> (n))
		{
			this->reserve(this->size() + n);
			for (size_t i = 0; i != n; ++i)
			{
				this->push_back(value);
			}
		}
		else
		{
			std::uninitialized_fill(this->_Last, this->_Last + static_cast<ptrdiff_t> (n), value);
			this->_Last += n;
		}

#else

		this->reserve(this->size() + n);
		for (size_t i = 0; i != n; ++i)
		{
			this->push_back(value);
		}

#endif

	}

	void append(const_pointer
		const begin, const_pointer
		const end)

	{

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)

		ptrdiff_t
		const n = end - begin;

		if (this->_End - this->_Last < n)
		{
			this->insert(this->end(), begin, end);
		}
		else
		{
			std::uninitialized_copy(begin, end, this->_Last);
			this->_Last += n;
		}
#else

			this->insert(this->end(), begin, end);

#endif

	}



	void append(const_pointer
		const value, size_type n = npos)
	{
		return this->append(value, value + static_cast<ptrdiff_t> (n == npos ? Traits::length(value) : n));
	}

	size_type find(value_type
		const& value, size_type
		const offset = 0) const
	{
		const_iterator begin = this->begin() + static_cast<difference_type> (offset), end = this->end();
		size_type result = static_cast<size_type> (std::find(begin, end, value) - begin);
		if (result >= static_cast<size_type> (end - begin))
		{
			result = npos;
		}
		else
		{
			result += offset;
		}

		return result;
	}

	const_pointer c_str()
	{
		const_pointer p;
		size_t
			const n = this->size();
		if (n == 0 || this->capacity() <= n || *(&*this->begin() + static_cast<ptrdiff_t> (n)) != value_type())
		{
			this->push_back(value_type());
			p = &*this->begin();
			this->pop_back();
		}
		else
		{
			p = &*this->begin();
		}

		return p;
	}

	const_pointer c_str() const
	{
		// Delegate to non-const version via const_cast
		// This is safe because the modification (push_back/pop_back) is temporary
		// and leaves the object in the same logical state
		return const_cast<this_type*>(this)->c_str();
	}

	const_pointer data() const
	{
		return this->empty() ? nullptr : &*this->begin();
	}

	iterator erase(size_t
		const pos, size_type
		const n = npos)
	{
		return this->erase(this->begin() + static_cast<difference_type> (pos), this->begin() + static_cast<difference_type> (pos) + (n == npos ? this->size() - pos : n));
	}

	iterator insert(size_t
		const pos, const_pointer
		const value, size_type
		const n = npos)
	{
		return this->insert(this->begin() + static_cast<difference_type> (pos), value, n);
	}

	this_type operator+(base_type
		const& other) const
	{
		this_type result;
		result.reserve(this->size() + other.size());
		result += *this;
		result += other;
		return result;
	}

#if defined(_MSC_VER) && !defined(_CPPLIB_VER)

	void push_back(value_type
		const& value)
	{
		if (this->_Last != this->_End)
		{
			this->allocator.construct(this->_Last, value);
			++this->_Last;
		}
		else
		{
			this->base_type::push_back(value);
		}
	}

#endif

	friend this_type operator+(const_pointer
		const left, base_type
		const& right)
	{
		size_t
				const nleft = Traits::length(left);
			this_type result;
			result.reserve(nleft + right.size());
			result.append(left, nleft);
			result += right;
			return result;
	}

	this_type& operator+=(base_type
		const& value)
	{
		if (!value.empty())
		{
			this->append(&*value.begin(), &*(value.end() - 1) + 1);
		}

		return *this;
	}

#else

	using base_type::operator+=;
	this_type& operator+=(this_type
		const& value)
	{
		if (!value.empty())
		{
			this->append(&*value.begin(), &*(value.end() - 1) + 1);
		}

		return *this;
	}

#endif

	template < class Ax2>
	friend std::basic_string<T, Traits, Ax2>& operator+=(std::basic_string<T, Traits, Ax2> &out, this_type
		const& me)
	{
		out.append(me.begin(), me.end());
		return out;
	}

	iterator insert(iterator
		const i, const_pointer
		const value, size_type
		const n = npos)
	{
		size_type
			const pos = static_cast<size_type> (i - this->begin());
		this->insert(i, value, value + static_cast<ptrdiff_t> (n == npos ? Traits::length(value) : n));
		return this->begin() + static_cast<difference_type> (pos);
	}

	this_type& operator=(const_pointer
		const value)
	{
		this->clear();
		this->append(value);
		return *this;
	}

};

namespace std
{
	typedef basic_string<TCHAR> tstring;
	typedef basic_vector_based_string<TCHAR, std::char_traits<TCHAR>, dynamic_allocator<TCHAR>> tvstring;
}

#endif // UFFS_CORE_TYPES_HPP