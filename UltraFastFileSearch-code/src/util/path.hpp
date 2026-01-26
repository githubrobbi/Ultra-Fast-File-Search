#pragma once

#ifndef PATH_HPP
#define PATH_HPP

//#include <string>

template<class It>
struct reverse_iterator_impl
{
	typedef std::reverse_iterator<It
#if !defined(_CPPLIB_VER) || _CPPLIB_VER < 403
		, typename std::iterator_traits<It>::value_type
#endif
	> reverse_iterator;
	static reverse_iterator create(It const it) { return reverse_iterator(it); }
};
template<class It>
struct reverse_iterator_impl<std::reverse_iterator<It
#if !defined(_CPPLIB_VER) || _CPPLIB_VER < 403
	, typename std::iterator_traits<It>::value_type
#endif
> >
{
	typedef It reverse_iterator;
	static reverse_iterator create(std::reverse_iterator<It
#if !defined(_CPPLIB_VER) || _CPPLIB_VER < 403
		, typename std::iterator_traits<It>::value_type
#endif
	> const it) { return it.base(); }
};

template<class It> typename reverse_iterator_impl<It>::reverse_iterator make_reverse_iterator(It const it) { return reverse_iterator_impl<It>::create(it); }

template<class It1, class It2>
std::pair<typename reverse_iterator_impl<It2>::reverse_iterator, typename reverse_iterator_impl<It1>::reverse_iterator> make_pair_reverse_iterator(std::pair<It1, It2> const p)
{ return std::make_pair(make_reverse_iterator(p.second), make_reverse_iterator(p.first)); }

inline wchar_t getdirsep() { return L'\\'; }
inline wchar_t getaltdirsep() { return L'/'; }

inline bool isdirsep(const wchar_t c) { return c == getaltdirsep() || c == getdirsep(); }

template<class It>
bool isrooted(const It &begin, const It &end)
{
	return begin != end && (isdirsep(*begin) || (end - begin == 2 && *(begin + 1) == _T(':')) || (end - begin >= 3 && *(begin + 1) == _T(':') && isdirsep(*(begin + 2))));
}

template<class FwdIt>
FwdIt trimdirsep(FwdIt const &begin, FwdIt end)
{
	while (begin != end && isdirsep(*(end - 1)))
	{
		--end;
	}
	return end;
}

template<class Str>
bool hasdirsep(Str &str)
{
	return !str.empty() && isdirsep(str[str.size() - 1]);
}

template<class Str>
Str &deldirsep(Str &str)
{
	while (hasdirsep(str))
	{
		str.erase(str.size() - 1);
	}
	return str;
}

template<class Str>
Str &adddirsep(Str &str)
{
	if (str.begin() != str.end() && !isdirsep(*(str.end() - 1)))
	{
		typename Str::value_type ch = getdirsep();
		for (typename Str::const_iterator it = str.end(); it != str.begin(); --it)
		{
			if (isdirsep(*(it - 1)))
			{
				ch = *(it - 1);
				break;
			}
		}
		str.push_back(ch);
	}
	return str;
}

template<class FwdIt>
FwdIt basename(FwdIt const begin, FwdIt end)
{
	while (begin != end && !isdirsep(*(end - 1)))
	{
		--end;
	}
	return end;
}

template<class FwdIt>
FwdIt basename_rev(FwdIt begin, FwdIt const &end)
{
	while (begin != end && !isdirsep(*begin))
	{
		++begin;
	}
	return begin;
}

template<class BidIt>
BidIt dirname(BidIt const &begin, BidIt const &end)
{
	return trimdirsep(begin, basename(begin, end));
}

template<class It>
inline It fileext(It const begin, It const end)  // WARNING: Does NOT account for file streams! (i.e.  Foo.txt:Bar.bin would return .bin instead of .txt)
{
	for (It it = end; it != begin; --it)
	{
		if (*(it - 1) == _T('.'))
		{
			--it;
			return it;
		}
		else if (isdirsep(*(it - 1)))
		{
			break;
		}
	}
	return end;
}

#if 0

#include <boost/intrusive/intrusive_ptr.hpp>
#include <boost/range/algorithm/copy.hpp>
#include <boost/range/begin.hpp>
#include <boost/range/difference_type.hpp>
#include <boost/range/end.hpp>
#include <boost/range/iterator.hpp>
#include <boost/range/size.hpp>
#include <boost/range/value_type.hpp>
#include <boost/unordered/unordered_map.hpp>

template<class Range>
struct RangeRef;

template<class Range>
struct basic_path
{
	typedef Range value_type;
	typedef typename boost::range_value<value_type>::type char_type;
	typedef typename boost::range_difference<value_type>::type difference_type;
	typedef size_t size_type;
	typedef typename boost::range_iterator<value_type>::type value_iterator;
	typedef typename boost::range_const_iterator<value_type>::type value_const_iterator;
	typedef boost::intrusive_ptr<basic_path> pointer;
	typedef boost::intrusive_ptr<basic_path const> const_pointer;
	static char_type const sep = '\\';

	value_type value;
	const_pointer const parent;
	size_type const depth;

	basic_path(value_type const &value = value_type(), const_pointer const parent = NULL)
		: parent(parent), value(value), depth((parent ? parent->depth : 0) + 1) { }

	value_iterator value_begin() { return boost::begin(this->value); }
	value_const_iterator value_begin() const { return boost::begin(this->value); }
	value_iterator value_end() { return boost::end(this->value); }
	value_const_iterator value_end() const { return boost::end(this->value); }
	size_type size() const { return (this->parent ? this->parent->size() : 0) + 1; }
	size_type str_size() const { return (this->parent ? this->parent->str_size() : 0) + this->value_size(); }
	size_type value_size() const { return boost::size(this->value); }

	std::pair<basic_path const *, size_type> gets(size_type component_index) const
	{
		std::pair<basic_path const *, size_type> r(static_cast<basic_path const *>(NULL), 0);
		if (this->parent) { r = this->parent->gets(component_index); }
		if (r.second == component_index) { r.first = this; }
		++r.second;
		return r;
	}

	std::pair<basic_path const *, std::pair<size_type, value_const_iterator> > getc(size_type char_index) const
	{
		std::pair<basic_path const *, std::pair<size_type, value_const_iterator> > result
			= std::make_pair(this, std::make_pair(this->value_size(), this->value_begin()));
		if (this->parent)
		{
			result = this->parent->getc(char_index);
			if (result.second.first <= char_index)
			{
				result.second.first += this->value_size();
				result.second.second = this->value_begin();
				using std::min;
				std::advance(result.second.second, static_cast<difference_type>(
					min(char_index - result.second.first, this->value_size())));
			}
		}
		else
		{
			if (char_index < this->value_size())
			{ std::advance(result.second.second, static_cast<difference_type>(char_index)); }
			else { result.first = NULL; }
		}
		return result;
	}

	Range const &operator[](size_type const i) const { return this->gets(i).first->value; }

	template<class OutIt, class R>
	OutIt copy(R const &sep, OutIt const &it) const
	{
		return boost::copy(
			this->value,
			this->parent
			? boost::copy(sep, this->parent->copy(sep, it))
			: it);
	}

	template<class R>
	std::basic_string<char_type> str(R const &separator) const
	{
		std::basic_string<char_type> result;
		size_type const n =
			this->str_size() + (this->size() - 1) * boost::size(separator);
		result.reserve(n + boost::size(separator));
		result.resize(n);
		std::basic_string<char_type>::iterator const &out = this->copy(separator, result.begin());
		(void)out;
		assert(out == result.end());
		return result;
	}

	std::basic_string<char_type> str() const
	{
		return this->str(std::make_pair(&sep, &sep + 1));
	}

	operator std::basic_string<char_type>() const { return this->str(); }

	friend std::basic_ostream<char_type> &operator <<(std::basic_ostream<char_type> &stream, const_pointer const &p)
	{
		if (p)
		{
			p->copy(std::make_pair(&sep, &sep + 1), std::ostream_iterator<char_type, char_type>(stream));
		}
		return stream;
	}

	friend const_pointer operator +(const_pointer const &a, const_pointer const &b)
	{
		return b ? (a + b->parent) + b->value : a;
	}

	friend const_pointer operator +(const_pointer const &a, Range const &b)
	{
		return new basic_path(b, a);
	}

	template<class R>
	bool operator ==(R const &other) const
	{
		basic_path const *me = this;
		char_type const sep[] = { basic_path<Range>::sep };
		typename boost::range_iterator<R const>::type oi(boost::end(other)), obegin(boost::begin(other));
		for (;;)
		{
			if (!me) { return false; }
			if (!boost::ends_with(std::make_pair(obegin, oi), me->value))
			{ return false; }
			std::advance(oi, -static_cast<difference_type>(me->value_size()));
			me = me->parent.get();
			if (oi != obegin)
			{ std::advance(oi, -static_cast<difference_type>(boost::size(sep))); }
			else { break; }
		}
		return true;
	}

	bool operator !=(Range const &other) const { return !(*this == other); }

	friend struct RangeRef<Range>;
private:
	//mutable size_t hash;
};

template<class Ch>
typename basic_path<boost::iterator_range<Ch const *> const>::const_pointer make_path(Ch const *range)
{
	return make_path(boost::make_iterator_range(range, range + std::char_traits<Ch>::length(range)));
}

template<class Range>
struct RangeRef : private boost::iterator_range<typename basic_path<Range const>::value_const_iterator>
{
	typedef boost::iterator_range<typename basic_path<Range const>::value_const_iterator> Base;
	size_t hash_value;
	RangeRef(Base const p) : Base(p), hash_value(0)
	{
		for (typename basic_path<Range const>::value_const_iterator i = this->begin(); i != this->end(); ++i)
		{
			this->hash_value = (this->hash_value * 0x60000005) ^ *i;
		}
	}
	bool operator <(RangeRef const other) const
	{ return std::lexicographical_compare(this->first, this->second, other.first, other.second); }
	bool operator ==(RangeRef const other) const { return boost::equal<Base, Base>(*this, other); }
	friend size_t hash_value(RangeRef const r) { return r.hash_value; }
};

template<class Range>
typename basic_path<Range const>::const_pointer make_path(Range const &range)
{
	typedef basic_path<Range const> Path;
	typedef typename Path::value_const_iterator It;
	typedef std::pair<It, It> Pair;
	static typename boost::range_value<Range const>::type const sep[] = { Path::sep };

	// First, count the number of components
	size_t const n = 1 + boost::count(range, Path::sep);

	typedef std::pair<size_t, RangeRef<Range> > CacheKey;
	typedef boost::unordered_map<CacheKey, Path::const_pointer> Cache;
	static Cache cache;

	struct Recurser
	{
		Path::const_pointer operator()(Pair subrange, size_t const n) const
		{
			Path::const_pointer result;
			Path::value_const_iterator j = subrange.second;
			while (j != subrange.first && *(j - 1) != Path::sep) { --j; }  // faster than find_last
			std::pair<Cache::iterator, typename Cache::iterator> const r =
				cache.equal_range(CacheKey(n, Pair(j, subrange.second)));
			if (!boost::empty(r))
			{
				for (Cache::iterator i = r.first; i != r.second; ++i)
				{
					if (*i->second == subrange)
					{ result = i->second; }
				}
			}
			if (!result && !boost::empty(subrange))
			{
				Path::value_const_iterator i(j);
				if (i != subrange.first)
				{ std::advance(i, -static_cast<ptrdiff_t>(boost::size(sep))); }
				result = new Path(Range(j, subrange.second), (*this)(Pair(subrange.first, i), n - 1));
				cache[CacheKey(n, Pair(result->value.begin(), result->value.end()))] = result;
			}
			return result;
		}
	};
	return Recurser()(Pair(boost::begin(range), boost::end(range)), n);
}

template struct basic_path<std::basic_string<TCHAR> >;
typedef basic_path<std::basic_string<TCHAR> > tpath;
typedef basic_path<std::basic_string<TCHAR> const> ctpath;
typedef basic_path<boost::iterator_range<TCHAR const *> const> cltpath;
typedef basic_path<std::string> path;
typedef basic_path<std::string const> cpath;
typedef basic_path<boost::iterator_range<char const *> const> clpath;
typedef basic_path<std::wstring> wpath;
typedef basic_path<std::wstring const> cwpath;
typedef basic_path<boost::iterator_range<wchar_t const *> const> clwpath;
#endif

#endif