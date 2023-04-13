#pragma once

#ifndef NFORMAT_HPP
#define NFORMAT_HPP

#include <tchar.h>

#include <locale>
#include <string>
#include <sstream>

template<class Char>
struct basic_conv;

template<> struct basic_conv< char  > { typedef  char   char_type; template<class OutIt> static OutIt plus_sign(OutIt i) { *i =  '+'; ++i; return i; } template<class OutIt> static OutIt ex(OutIt i) { *i =  'x'; ++i; return i; } template<class T> static void to_string(T const &value, char_type *const buffer, unsigned char const radix); };
template<> struct basic_conv<wchar_t> { typedef wchar_t char_type; template<class OutIt> static OutIt plus_sign(OutIt i) { *i = L'+'; ++i; return i; } template<class OutIt> static OutIt ex(OutIt i) { *i = L'x'; ++i; return i; } template<class T> static void to_string(T const &value, char_type *const output, unsigned char const radix); };
#define X(Char, Int, IToA, AToI) template<> inline void basic_conv<Char>::to_string<Int>(Int const &value, Char *const buffer, unsigned char const radix) { IToA(value, buffer, radix); } template<class> struct basic_conv;
X( char  ,   signed      char,    _itoa,     atoi);
X( char  ,   signed     short,    _itoa,     atoi);
X( char  ,   signed       int,    _itoa,     atoi);
X( char  ,   signed      long,    _ltoa,     atol);
X( char  , unsigned      long,   _ultoa,   _atoul);
X( char  ,   signed long long,  _i64toa,  _atoi64);
X( char  , unsigned long long, _ui64toa, _atoui64);
X(wchar_t,   signed      char,    _itow,    _wtoi);
X(wchar_t,   signed     short,    _itow,    _wtoi);
X(wchar_t,   signed       int,    _itow,    _wtoi);
X(wchar_t,   signed      long,    _ltow,    _wtol);
X(wchar_t, unsigned      long,   _ultow,   _wtoul);
X(wchar_t,   signed long long,  _i64tow,  _wtoi64);
X(wchar_t, unsigned long long, _ui64tow, _wtoui64);
#undef  X

template<class T> struct basic_char { typedef T type; };
#if defined(__clang__) && !defined(_CPPLIB_VER)
template<> struct basic_char<wchar_t> { typedef unsigned short type; };
#endif

template<class OutIt, class Traits = std::char_traits<typename basic_char<typename std::iterator_traits<OutIt>::value_type>::type> >
class basic_iterator_ios : public std::basic_ios<typename basic_char<typename Traits::char_type>::type, Traits>
{
	typedef basic_iterator_ios this_type;
	typedef std::basic_ios<typename basic_char<typename Traits::char_type>::type, Traits> base_type;
	typedef typename Traits::char_type char_type;
	typedef std::ctype<typename basic_char<char_type>::type> CType;
	typedef std::char_traits<char_type> traits_type;
	basic_iterator_ios(this_type const &);
	this_type &operator =(this_type const &);
	typedef std::numpunct<typename basic_char<char_type>::type> NumPunct;
	typedef std::num_put<typename basic_char<char_type>::type, OutIt> NumPut;
#if defined(_MSC_VER) && !defined(_WIN64) && (!defined(_CPPLIB_VER) || _CPPLIB_VER < 403)
	struct NumPutHacked : public NumPut
	{
		typedef std::ios_base ios_base;
		typedef TCHAR _E;
		typedef OutIt _OI;
		using NumPut::do_put;
		using NumPut::_Iput;
		static char *__cdecl _Ifmt(char *_Fmt, const char *_Spec, ios_base::fmtflags _Fl)
		{
			char *_S = _Fmt;
			*_S++ = '%';
			if (_Fl & ios_base::showpos)
			{ *_S++ = '+'; }
			if (_Fl & ios_base::showbase)
			{ *_S++ = '#'; }
			*_S++ = _Spec[0];
			*_S++ = _Spec[1];
			*_S++ = _Spec[2];
			*_S++ = _Spec[3];
			*_S++ = _Spec[4];
			ios_base::fmtflags _Bfl = _Fl & ios_base::basefield;
			*_S++ = _Bfl == ios_base::oct ? 'o'
				: _Bfl != ios_base::hex ? _Spec[5]      // 'd' or 'u'
				: _Fl & ios_base::uppercase ? 'X' : 'x';
			*_S = '\0';
			return (_Fmt);
		}
#pragma warning(push)
#pragma warning(disable: 4774)
		_OI do_put(_OI _F, ios_base &_X, _E _Fill, long _V) const
		{
			return this->NumPut::do_put(_F, _X, _Fill, _V);
		}
		_OI do_put(_OI _F, ios_base &_X, _E _Fill, unsigned long _V) const
		{
			return this->NumPut::do_put(_F, _X, _Fill, _V);
		}
		_OI do_put(_OI _F, ios_base& _X, _E _Fill, long long _V) const
		{
			char _Buf[2 * _MAX_INT_DIG], _Fmt[12];
			return (_Iput(_F, _X, _Fill, _Buf, sprintf(_Buf, _Ifmt(_Fmt, "I64lld", _X.flags()), _V)));
		}
		_OI do_put(_OI _F, ios_base& _X, _E _Fill, unsigned long long _V) const
		{
			char _Buf[2 * _MAX_INT_DIG], _Fmt[12];
			return (_Iput(_F, _X, _Fill, _Buf, sprintf(_Buf, _Ifmt(_Fmt, "I64llu", _X.flags()), _V)));
		}
#pragma warning(pop)
		template<class T>
		_OI put(_OI _F, ios_base &_X, _E _Fill, T const &value) const { return this->do_put(_F, _X, _Fill, value); }
	};
#else
	typedef NumPut NumPutHacked;
#endif
	template<class T>
	static T const *static_instance() { T const *p = NULL; if (!p) { p = new T(); } return p; }
	std::ios_base *me;
	NumPunct const *numpunct;
	std::string numpunct_grouping;
	NumPut const *num_put;
	void event_callback(std::ios_base::event const type)
	{
		if (type == std::ios_base::imbue_event)
		{
			std::locale loc = this->getloc();
			bool has_num_put;
#ifdef _ADDFAC
			has_num_put = std::_HAS(loc, NumPut);
#else
			has_num_put = std::has_facet<NumPut>(loc);
#endif
			if (has_num_put)
			{
				this->num_put = &
#ifdef _ADDFAC
					std::_USE(loc, NumPut)
#else
					std::use_facet<NumPut>(loc)
#endif
					;
			}
			else
			{
				this->num_put = static_instance<NumPutHacked>();
			}
			bool has_num_punct;
#ifdef _ADDFAC
			has_num_punct = std::_HAS(loc, NumPunct);
#else
			has_num_punct = std::has_facet<NumPunct>(loc);
#endif
			if (has_num_punct)
			{
				this->numpunct = &
#ifdef _ADDFAC
					std::_USE(loc, NumPunct)
#else
					std::use_facet<NumPunct>(loc)
#endif
					;
			}
			else
			{
				this->numpunct = static_instance<NumPunct>();
			}
			this->numpunct_grouping = this->numpunct->grouping();
		}
	}
	static void event_callback(std::ios_base::event type, std::ios_base &base, int)
	{
		return static_cast<this_type &>(base).event_callback(type);
	}
	template<class T>
	OutIt do_put(OutIt i, T const &value) const
	{
		bool unsupported = false;
		std::ios_base::fmtflags const flags = this->flags();
		if (flags & std::ios_base::right) { unsupported = true; }
		if (flags & std::ios_base::internal) { unsupported = true; }
		if (flags & std::ios_base::left) { unsupported = true;}
		if (flags & std::ios_base::boolalpha) { unsupported = true; }
		if (flags & std::ios_base::uppercase) { unsupported = true; /* this is because the itoa() family doesn't allow control over capitalization */  }
		if (flags & std::ios_base::fixed) { unsupported = true;}
		if (flags & std::ios_base::scientific) { unsupported = true; }
		if (unsupported)
		{
			i = static_cast<NumPutHacked const *>(this->num_put)->put(i, *this->me, this->base_type::fill(), value);
		}
		else
		{
			unsigned char radix = 10;
			if (flags & std::ios_base::oct) { radix =  010; }
			if (flags & std::ios_base::dec) { radix =   10; }
			if (flags & std::ios_base::hex) { radix = 0x10; }
			if (flags & std::ios_base::showbase)
			{
				if (radix !=   10) { char_type zbuf[16]; basic_conv<char_type>::to_string(T(), zbuf, radix); i = std::copy(zbuf, zbuf + static_cast<ptrdiff_t>(traits_type::length(zbuf)), i); }
				if (radix == 0x10) { i = basic_conv<char_type>::ex(i); }
			}
			char_type buf[2 * _MAX_INT_DIG];
			buf[0] = char_type();
			basic_conv<char_type>::to_string(value, buf, radix);
			size_t n = traits_type::length(buf);
			if (flags & std::ios_base::showpos) { T zero = T(); if (value >= zero) { i = basic_conv<char_type>::plus_sign(i); } }
			size_t const ngroups = this->numpunct_grouping.size();
			if (ngroups > 0)
			{
				char_type const sep = this->numpunct->thousands_sep();
				size_t nseps = 0;
				size_t igroup;
				for (int pass = 0; pass < 2; ++pass)
				{
					size_t o = n + nseps;
					igroup = 0;
					size_t ngrouprem = this->numpunct_grouping[igroup];
					for (size_t j = n; j != 0 && ((void)--j, true); )
					{
						if (!ngrouprem)
						{
							ngrouprem = this->numpunct_grouping[igroup];
							if (!pass) { ++nseps; }
							else {buf[--o] = sep; }
							igroup += igroup + 1 < ngroups;
						}
						if (pass) { buf[--o] = buf[j]; }
						ngrouprem -= !!ngrouprem;
					}
				}
				n += nseps;
			}
			i = std::copy(buf, buf + static_cast<ptrdiff_t>(n), i);
			if (flags & std::ios_base::showpoint) { *i = this->numpunct->decimal_point(); ++i; }
		}
		return i;
	}
	void init()
	{
		this->base_type::init();
		this->me = static_cast<std::ios_base *>(this);
		this->register_callback(event_callback, -1);
		this->event_callback(std::ios_base::imbue_event);
	}
public:
	template<class T>
	struct lazy
	{
		this_type const *me;
		T const *value;
		explicit lazy(this_type const *const me, T const &value) : me(me), value(&value) { }
		operator std::basic_string<char_type>() const
		{
			std::basic_string<char_type> result;
			me->put(std::back_inserter(result), *value);
			return result;
		}
	};
	basic_iterator_ios()                                : base_type(), me(), numpunct(), num_put() { this->init(); }
	explicit basic_iterator_ios(std::locale const &loc) : base_type(), me(), numpunct(), num_put() { this->init(); this->imbue(loc); }
	OutIt put(OutIt const &i,                 bool const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i,                 char const value) const { return this->do_put(i, value); }
#ifdef _NATIVE_WCHAR_T_DEFINED
	OutIt put(OutIt const &i,            __wchar_t const value) const { return this->do_put(i, value); }
#endif
	OutIt put(OutIt const &i,   signed        char const value) const { return this->do_put(i, static_cast<long>(value)); }
	OutIt put(OutIt const &i,   signed       short const value) const { return this->do_put(i, static_cast<long>(value)); }
	OutIt put(OutIt const &i,   signed         int const value) const { return this->do_put(i, static_cast<long>(value)); }
	OutIt put(OutIt const &i,   signed        long const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i,   signed long   long const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i, unsigned        char const value) const { return this->do_put(i, static_cast<unsigned long>(value)); }
	OutIt put(OutIt const &i, unsigned       short const value) const { return this->do_put(i, static_cast<unsigned long>(value)); }
	OutIt put(OutIt const &i, unsigned         int const value) const { return this->do_put(i, static_cast<unsigned long>(value)); }
	OutIt put(OutIt const &i, unsigned        long const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i, unsigned long   long const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i,               double const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i,          long double const value) const { return this->do_put(i, value); }
	OutIt put(OutIt const &i,          void const *const value) const { return this->do_put(i, value); }
	template<class T>
	lazy<T> operator()(T const &value) const { return lazy<T>(this, value); }
};

template<class Char, class Traits = std::char_traits<Char>, class Alloc = std::allocator<Char> >
class basic_fast_ostringstream : public std::basic_string<Char, Traits, Alloc>
{
	typedef basic_fast_ostringstream this_type;
	typedef std::basic_string<Char, Traits, Alloc> base_type;
public:
	template<class T>
	this_type &operator<<(T const &value) { static_cast<base_type &>(*this) += value; return *this; }
	this_type const &str() const { return *this; }
	std::back_insert_iterator<base_type> back_inserter() { return std::back_insert_iterator<base_type>(*this); }
};

#endif
