#include "string_matcher.hpp"

#include <string.h>

#include <algorithm>
#include <iterator>
#include <string>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcomment"
#endif
#pragma warning(push)
#pragma warning(disable: 4456)  // declaration of '...' hides previous local declaration
#pragma warning(disable: 4619)  // warning: there is no warning number '...'
#pragma warning(disable: 5031)  // #pragma warning(pop): likely mismatch, popping warning state pushed in different file
#if (!defined(_CPPLIB_VER) || _CPPLIB_VER < 600) && 1
#pragma warning(disable: 4061)  // enumerator in switch of enum is not explicitly handled by a case label
#pragma warning(disable: 4127)  // conditional expression is constant
#pragma warning(disable: 4571)  // Informational: catch(...) semantics changed since Visual C++ 7.1; structured exceptions (SEH) are no longer caught
#pragma warning(disable: 4640)  // '...': construction of local static object is not thread-safe
#pragma warning(disable: 4702)  // unreachable code
#pragma warning(disable: 4986)  // 'operator new': exception specification does not match previous declaration
#define BOOST_UNORDERED_TUPLE_ARGS 0
#include <boost/xpressive/regex_error.hpp>
#ifdef  BOOST_XPR_ENSURE_
#undef  BOOST_XPR_ENSURE_
#define BOOST_XPR_ENSURE_(pred, code, msg) ((pred) || ((void)(throw regex_error((code), (msg))), 1))
#else
#error  BOOST_XPR_ENSURE_ was not found -- you need to fix this to avoid inserting strings into the binary unnecessarily
#endif
#include <boost/xpressive/match_results.hpp>
#include <boost/xpressive/xpressive_dynamic.hpp>
#if defined(__clang__) && !defined(_CPPLIB_VER)
template<> int boost::xpressive::cpp_regex_traits<wchar_t>::value(wchar_t a, int b) const { return reinterpret_cast<boost::xpressive::cpp_regex_traits<unsigned short> const *>(this)->value(static_cast<unsigned short>(a), b); }
#endif

namespace regex_namespace = boost::xpressive;
template<class Char, class Traits = regex_namespace::regex_traits<Char>, class BidIt = Char const *> struct basic_regex_of { typedef Traits traits_type; typedef regex_namespace::basic_regex<BidIt> type; typedef regex_namespace::match_results<BidIt> match_results_type; template<class It> static type compile(It const &begin, It const &end, regex_namespace::regex_constants::syntax_option_type const flags) { return regex_namespace::regex_compiler<typename type::iterator_type, Traits>().compile(begin, end, flags); } };
#elif (!defined(_CPPLIB_VER) || _CPPLIB_VER < 600) && 1
#include <boost/regex.hpp>
namespace regex_namespace = boost;
template<class Char, class Traits = regex_namespace::regex_traits<Char>, class BidIt = Char const *> struct basic_regex_of { typedef Traits traits_type; typedef regex_namespace::basic_regex<Char        , Traits> type; typedef regex_namespace::match_results<BidIt> match_results_type; template<class It> static type compile(It const &begin, It const &end, regex_namespace::regex_constants::syntax_option_type const flags) { return type(begin, end, flags); } };
#else
#include <regex>
#ifndef _CPPLIB_VER
namespace std { _NO_RETURN(_Xregex_error(regex_constants::error_type code) { throw regex_error(code); }) }
#endif
namespace regex_namespace = std;
template<class Char, class Traits = regex_namespace::regex_traits<Char>, class BidIt = Char const *> struct basic_regex_of { typedef Traits traits_type; typedef regex_namespace::basic_regex<Char        , Traits> type; typedef regex_namespace::match_results<BidIt> match_results_type; template<class It> static type compile(It const &begin, It const &end, regex_namespace::regex_constants::syntax_option_type const flags) { return type(begin, end, flags); } };
#endif
#define _assert(A, B, C) _assert(const A, const B, C)
#include <boost/algorithm/searching/boyer_moore_horspool.hpp>
#undef  _assert
#pragma warning(pop)
#ifdef __clang__
#pragma clang diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wglobal-constructors"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wshadow-field-in-constructor"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif
extern "C"
{
#ifndef WINBASEAPI
#define WINBASEAPI __declspec(dllimport)
#endif
	typedef int BOOL;
	WINBASEAPI int WINAPI MultiByteToWideChar(unsigned int CodePage, unsigned long dwFlags, char const *lpMultiByteStr, int cbMultiByte, wchar_t *lpWideCharStr, int cchWideChar);
	WINBASEAPI int WINAPI WideCharToMultiByte(unsigned int CodePage, unsigned long dwFlags, wchar_t const *lpWideCharStr, int cchWideChar, char *lpMultiByteStr, int cbMultiByte, char const *lpDefaultChar, BOOL *lpUsedDefaultChar);
}

template<class Range>
class copyable : public Range
{
	copyable() : Range() { }
public:
	template<class It>
	explicit copyable(It const a, It const b) : Range(a, b) { }
	template<class It>
	void emplace(It const a, It const b)
	{
		Range &me = static_cast<Range &>(*this);
		me.~Range();
		try { new(&me) Range(a, b); }
#pragma warning(suppress: 4571)
		catch (...) { new(&me) Range(a, a); throw; }
	}
};

template<class It>
struct tracking_iterator
{
	typedef tracking_iterator this_type;
	typedef typename std::iterator_traits<It>::value_type value_type;
	struct watermark { It begin; typename std::iterator_traits<It>::difference_type end; watermark() : begin(), end() { } explicit watermark(It const &begin) : begin(begin), end() { } };
	It p;
	watermark *c;
	tracking_iterator(It const &p = It(), watermark *const c = NULL) : p(p), c(c) { }
	value_type operator *() const { return (*this)[0]; }
	value_type operator[](ptrdiff_t const i) const
	{
		if (this->c)
		{
			typename std::iterator_traits<It>::difference_type const d = this->p + i + 1 - this->c->begin;
			assert(d >= 0);
			if (this->c->end < d) { this->c->end = d; }
		}
		return this->p[i];
	}
	this_type &operator++() { return *this += 1; }
	this_type &operator--() { return *this -= 1; }
	this_type operator++(int) { this_type me(*this); ++*this; return me; }
	this_type operator--(int) { this_type me(*this); --*this; return me; }
	this_type operator +(ptrdiff_t const d) const { this_type me(*this); me += d; return me; }
	this_type operator -(ptrdiff_t const d) const { this_type me(*this); me -= d; return me; }
	ptrdiff_t operator -(this_type const other) const { return this->p - other.p; }
	this_type &operator+=(ptrdiff_t const d) { this->p += d; return *this; }
	this_type &operator-=(ptrdiff_t const d) { return *this += -d; }
	bool operator==(this_type const other) const { return this->p == other.p; }
	bool operator!=(this_type const other) const { return this->p != other.p; }
	bool operator<=(this_type const other) const { return this->p <= other.p; }
	bool operator>=(this_type const other) const { return this->p >= other.p; }
	bool operator< (this_type const other) const { return this->p < other.p; }
	bool operator> (this_type const other) const { return this->p > other.p; }
	It base() const { return this->p; }
};

namespace std
{
	template<class It> struct iterator_traits<tracking_iterator<It> > : iterator_traits<It>
	{
		// We want to force traversal of the string
		typedef std::bidirectional_iterator_tag iterator_category;
	};
}

template<>  char   totlower< char  >( char   const ch) { return ch <= SCHAR_MAX ?  'A' <= ch && ch <=  'Z' ? static_cast< char  >(ch ^ 0x20) : ch : static_cast< char  >(::tolower (ch)); }
template<> wchar_t totlower<wchar_t>(wchar_t const ch) { return ch <= SCHAR_MAX ? L'A' <= ch && ch <= L'Z' ? static_cast<wchar_t>(ch ^ 0x20) : ch : static_cast<wchar_t>(::towlower(ch)); }
template<>  char   totupper< char  >( char   const ch) { return ch <= SCHAR_MAX ?  'a' <= ch && ch <=  'z' ? static_cast< char  >(ch ^ 0x20) : ch : static_cast< char  >(::tolower (ch)); }
template<> wchar_t totupper<wchar_t>(wchar_t const ch) { return ch <= SCHAR_MAX ? L'a' <= ch && ch <= L'z' ? static_cast<wchar_t>(ch ^ 0x20) : ch : static_cast<wchar_t>(::towlower(ch)); }

template<class It>
struct case_insensitive_iterator
{
	typedef case_insensitive_iterator this_type;
	typedef typename std::iterator_traits<It>::value_type value_type;
	It p;
	case_insensitive_iterator(It const &p) : p(p) { }
	value_type operator *() const { return (*this)[0]; }
	value_type operator[](ptrdiff_t const i) const{ return totlower(this->p[i]); }
	this_type &operator++() { return *this += 1; }
	this_type &operator--() { return *this -= 1; }
	this_type operator++(int) { this_type me(*this); ++ *this; return me; }
	this_type operator--(int) { this_type me(*this); -- *this; return me; }
	this_type operator +(ptrdiff_t const d) const { this_type me(*this); me += d; return me; }
	this_type operator -(ptrdiff_t const d) const { this_type me(*this); me -= d; return me; }
	ptrdiff_t operator -(this_type const other) const { return this->p - other.p; }
	this_type &operator+=(ptrdiff_t const d) { this->p += d; return *this; }
	this_type &operator-=(ptrdiff_t const d) { return *this += -d; }
	bool operator==(this_type const other) const { return this->p == other.p; }
	bool operator!=(this_type const other) const { return this->p != other.p; }
	bool operator<=(this_type const other) const { return this->p <= other.p; }
	bool operator>=(this_type const other) const { return this->p >= other.p; }
	bool operator< (this_type const other) const { return this->p <  other.p; }
	bool operator> (this_type const other) const { return this->p >  other.p; }
	It base() const { return this->p; }
};

namespace std
{
	template<class Char> struct iterator_traits<case_insensitive_iterator<Char *> > : iterator_traits<Char *> { };
	template<class Char> struct iterator_traits<case_insensitive_iterator<Char const *> > : iterator_traits<Char const *> { };
}

struct string_matcher::base_type
{
	template<class Char>
	struct special_chars;

	template<class Base>
	struct regex_traits : public Base
	{
		// Could also use this if needed later:  LCMapString(LOCALE_USER_DEFAULT, LCMAP_LOWERCASE, , , , )
		typedef typename Base::char_type char_type;
		static bool broken_system_translator()
		{
			return
#ifndef _CPPLIB_VER
				true
#else
				false
#endif
				;
		}
		char_type translate(char_type ch, bool const ignore_case) const { return ignore_case ? this->translate_nocase(ch) : this->translate(ch); }
		char_type translate(char_type ch) const { return broken_system_translator() || ch <= SCHAR_MAX ? ch : this->Base::translate(ch); }
		char_type translate_nocase(char_type ch) const { return broken_system_translator() || ch <= SCHAR_MAX ? totlower<char_type>(ch) : this->Base::translate_nocase(ch); }
		bool in_range(char_type first, char_type last, char_type ch) const { return broken_system_translator() || ch <= SCHAR_MAX ? this->translate(first) <= this->translate(ch) && this->translate(ch) <= this->translate(last) : this->Base::in_range(first, last, ch); }
		bool in_range_nocase(char_type first, char_type last, char_type ch) const { return broken_system_translator() || ch <= SCHAR_MAX ? this->translate_nocase(first) <= this->translate_nocase(ch) && this->translate_nocase(ch) <= this->translate_nocase(last) : this->Base::in_range_nocase(first, last, ch); }
		char_type tolower(char_type ch) const { return broken_system_translator() || ch <= SCHAR_MAX ? totlower<char_type>(ch) : this->Base::tolower(ch); }
		char_type toupper(char_type ch) const { return broken_system_translator() || ch <= SCHAR_MAX ? totupper<char_type>(ch) : this->Base::toupper(ch); }
	};

	template<class Char, class Alloc = std::allocator<Char> >
	struct impl
	{
		typedef Char const *base_iterator;
		typedef tracking_iterator<base_iterator> iterator;
		typedef tracking_iterator<case_insensitive_iterator<base_iterator> > ci_iterator;
		typedef basic_regex_of<Char, regex_traits<typename basic_regex_of<Char>::traits_type>, iterator> basic_regex_of_;
		typedef typename basic_regex_of_::type regex_type;
		typedef typename basic_regex_of_::traits_type regex_traits_type;
		typedef typename basic_regex_of_::match_results_type match_results_type;
		typedef Char char_type;
		typedef std::vector<Char, Alloc> pattern_type;
		enum AnchorType { UNANCHORED_BEGIN = 1 << 0, UNANCHORED_END = 1 << 1 };
		pattern_kind kind;
		pattern_options option;
		pattern_type pattern;
		AnchorType unanchored;
		bool case_insensitive;
		copyable<boost::algorithm::boyer_moore_horspool<iterator> > string_search;
		copyable<boost::algorithm::boyer_moore_horspool<ci_iterator> > string_search_ci;
		match_results_type mr;
		regex_type re;
		explicit impl(pattern_kind const kind, pattern_options const option, pattern_type pattern) :
			kind(kind), option(option), pattern(), unanchored(), case_insensitive(),
			string_search(pattern.data(), pattern.data()), string_search_ci(ci_iterator(pattern.data()), ci_iterator(pattern.data()))
		{ pattern.swap(this->pattern); this->init(); }
		explicit impl(pattern_kind const kind, pattern_options const option, char_type const pattern[], size_t const length) :
			kind(kind), option(option), pattern(pattern, pattern + static_cast<ptrdiff_t>(length)), unanchored(), case_insensitive(),
			string_search(pattern, pattern), string_search_ci(ci_iterator(pattern), ci_iterator(pattern))
		{ this->init(); }
		template<class It>
		static regex_type compile(It const &begin, It const &end, regex_namespace::regex_constants::syntax_option_type const flags)
		{
			return basic_regex_of_::template compile<It>(begin, end, flags);
		}
		template<class Range>
		static regex_type compile(Range const &range, regex_namespace::regex_constants::syntax_option_type const flags)
		{
			return compile(range.begin(), range.end(), flags);
		}
		void init()
		{
			this->unanchored = AnchorType();
			this->case_insensitive = !!(this->option & pattern_option_case_insensitive);
			this->string_search.emplace(this->pattern.data(), this->pattern.data());
			this->string_search_ci.emplace(ci_iterator(this->pattern.data()), ci_iterator(this->pattern.data()));
			regex_type().swap(this->re);
			typedef special_chars<char_type> special_chars_type;
			if (this->kind == pattern_glob || this->kind == pattern_globstar)
			{
				size_t const minwild = 1 + (this->kind == pattern_globstar);
				// Strip leading and trailing full-wildcards first!
				size_t prefix_asterisk = 0;
				while (prefix_asterisk < this->pattern.size() && this->pattern[prefix_asterisk] == special_chars_type::asterisk()) { ++prefix_asterisk; }
				size_t suffix_asterisk = 0;
				while (suffix_asterisk < this->pattern.size() && this->pattern[this->pattern.size() - 1 - suffix_asterisk] == special_chars_type::asterisk()) { ++suffix_asterisk; }
				this->unanchored = static_cast<AnchorType>(this->unanchored | ((prefix_asterisk >= minwild ? UNANCHORED_BEGIN : 0) | (suffix_asterisk >= minwild ? UNANCHORED_END : 0)));
				if (suffix_asterisk >= minwild)
				{
					this->pattern.erase(this->pattern.end() - static_cast<ptrdiff_t>(suffix_asterisk), this->pattern.end());
					suffix_asterisk = 0;
					if (prefix_asterisk > this->pattern.size()) { prefix_asterisk = this->pattern.size(); }
				}
				if (prefix_asterisk >= minwild)
				{
					this->pattern.erase(this->pattern.begin(), this->pattern.begin() + static_cast<ptrdiff_t>(prefix_asterisk));
					prefix_asterisk = 0;
					if (suffix_asterisk > this->pattern.size()) { suffix_asterisk = this->pattern.size(); }
				}
				size_t const questions = static_cast<size_t>(std::count(this->pattern.begin(), this->pattern.end(), special_chars_type::question()));
				if (this->kind == pattern_glob && !questions)
				{
					// reduce to pattern_globstar if possible (question marks can make this impossible)
					pattern_type globstar_pattern;
					globstar_pattern.reserve(2 * this->pattern.size());
					for (size_t i = 0; i != this->pattern.size(); ++i)
					{
						typename pattern_type::value_type ch = this->pattern[i];
						if (ch == special_chars_type::asterisk())
						{ globstar_pattern.push_back(ch); }
						globstar_pattern.push_back(ch);
					}
					globstar_pattern.swap(this->pattern);
					this->kind = pattern_globstar;
				}
				if (this->kind == pattern_globstar && !questions)
				{
					size_t const middle_asterisk = static_cast<size_t>(std::count(this->pattern.begin() + static_cast<ptrdiff_t>(prefix_asterisk), this->pattern.end() - static_cast<ptrdiff_t>(this->pattern.size() - prefix_asterisk < suffix_asterisk ? this->pattern.size() - prefix_asterisk : suffix_asterisk), special_chars_type::asterisk()));
					if (!middle_asterisk &&
						(!prefix_asterisk || prefix_asterisk >= minwild) &&
						(!suffix_asterisk || suffix_asterisk >= minwild))
					{
						// NOTE: Prefix and suffix asterisks may overlap!
						this->pattern.erase(this->pattern.end() - static_cast<ptrdiff_t>(suffix_asterisk), this->pattern.end());
						this->pattern.erase(this->pattern.begin(), this->pattern.begin() + static_cast<ptrdiff_t>(prefix_asterisk < this->pattern.size() ? prefix_asterisk : this->pattern.size()));
						this->kind = pattern_verbatim;
					}
				}
			}
			if (this->kind == pattern_glob || this->kind == pattern_globstar)  // reduce to regex
			{
				pattern_type to_escape;
				to_escape.push_back(special_chars_type::backslash());
				to_escape.push_back(special_chars_type::period());
				to_escape.push_back(special_chars_type::dash());
				to_escape.push_back(special_chars_type::plus());
				to_escape.push_back(special_chars_type::asterisk());
				to_escape.push_back(special_chars_type::question());
				to_escape.push_back(special_chars_type::open_bracket());
				to_escape.push_back(special_chars_type::close_bracket());
				to_escape.push_back(special_chars_type::open_brace());
				to_escape.push_back(special_chars_type::close_brace());
				to_escape.push_back(special_chars_type::open_parenthesis());
				to_escape.push_back(special_chars_type::close_parenthesis());
				to_escape.push_back(special_chars_type::comma());
				to_escape.push_back(special_chars_type::caret());
				to_escape.push_back(special_chars_type::dollar());
				to_escape.push_back(special_chars_type::pipe());
				to_escape.push_back(special_chars_type::number());
				to_escape.push_back(special_chars_type::carriage_return());
				to_escape.push_back(special_chars_type::line_feed());
				std::sort(to_escape.begin(), to_escape.end());
				/*   \**\     should be replaced with     \\(?:[^\\]+\\)*   */
				pattern_type pattern;
				if (!(this->unanchored & UNANCHORED_BEGIN) && (this->unanchored & UNANCHORED_END /* this is just an optimization, because regex_match will take care of it */)) { pattern.push_back(special_chars_type::caret()); }
				for (size_t i = 0; i != this->pattern.size(); ++i)
				{
					typename pattern_type::value_type ch = this->pattern[i];
					if (ch == special_chars_type::question())
					{
						if (this->kind == pattern_glob)
						{
							pattern.push_back(special_chars_type::period());
						}
						else
						{
							pattern.push_back(special_chars_type::open_bracket());
							pattern.push_back(special_chars_type::caret());
							pattern.push_back(special_chars_type::backslash());  // escape
							pattern.push_back(special_chars_type::backslash());  // directory sep
							pattern.push_back(special_chars_type::backslash());  // escape
							pattern.push_back(special_chars_type::slash());  // directory sep
							pattern.push_back(special_chars_type::close_bracket());
						}
					}
					else
					{
						if (ch == special_chars_type::asterisk())  // TODO: Also check for double-astrisks that weren't handled previously...
						{
							if (this->kind == pattern_glob)
							{
								pattern.push_back(special_chars_type::period());
							}
							else if (i + 1 < this->pattern.size() && this->pattern[i + 1] == special_chars_type::asterisk())
							{
								if (i > 0 && this->pattern[i - 1] == special_chars_type::backslash() &&
									i + 2 < this->pattern.size() && this->pattern[i + 2] == special_chars_type::backslash())
								{
									pattern.push_back(special_chars_type::open_parenthesis());
									pattern.push_back(special_chars_type::question());
									pattern.push_back(special_chars_type::colon());
									pattern.push_back(special_chars_type::open_bracket());
									pattern.push_back(special_chars_type::caret());
									pattern.push_back(special_chars_type::backslash());  // escape
									pattern.push_back(special_chars_type::backslash());  // directory sep
									pattern.push_back(special_chars_type::backslash());  // escape
									pattern.push_back(special_chars_type::slash());  // directory sep
									pattern.push_back(special_chars_type::close_bracket());
									pattern.push_back(special_chars_type::plus());
									pattern.push_back(special_chars_type::backslash());  // escape
									pattern.push_back(this->pattern[i + 2]);  // directory sep
									pattern.push_back(special_chars_type::close_parenthesis());
									unsigned long min_quantity = 0;
									while (i + 6 <= this->pattern.size() &&
										this->pattern[i + 3] == ch &&
										this->pattern[i + 4] == ch &&
										this->pattern[i + 5] == special_chars_type::backslash())
									{
										++min_quantity;
										i += 3;
									}
									if (min_quantity == 0) { /* asterisk is fine */ }
									else if (min_quantity == 1) { ch = special_chars_type::plus(); }
									else
									{
										pattern.push_back(special_chars_type::open_brace());
										TCHAR buffer[32];
										buffer[0] = _T('\0');
										_ultot(min_quantity, buffer, 10);
										for (size_t j = 0; buffer[j]; ++j)
										{ pattern.push_back(static_cast<typename pattern_type::value_type>(buffer[j])); }
										pattern.push_back(special_chars_type::comma());
										ch = special_chars_type::close_brace();
									}
									// Right now 'i' is on the first asterisk
									++i;
								}
								else
								{
									pattern.push_back(special_chars_type::period());
								}
								++i;
							}
							else
							{
								pattern.push_back(special_chars_type::open_bracket());
								pattern.push_back(special_chars_type::caret());
								pattern.push_back(special_chars_type::backslash());  // escape
								pattern.push_back(special_chars_type::backslash());  // directory sep
								pattern.push_back(special_chars_type::backslash());  // escape
								pattern.push_back(special_chars_type::slash());  // directory sep
								pattern.push_back(special_chars_type::close_bracket());
							}
						}
						else if (std::binary_search(to_escape.begin(), to_escape.end(), ch))
						{
							pattern.push_back(special_chars_type::backslash());
						}
						pattern.push_back(ch);
					}
				}
				if (this->unanchored & UNANCHORED_END)
				{
					// regex_match is faster than regex_search when dealing with slack ends
					pattern.push_back(special_chars_type::period());
					pattern.push_back(special_chars_type::asterisk());
					this->unanchored = static_cast<AnchorType>(this->unanchored & ~UNANCHORED_END);
				}
				if (!(this->unanchored & UNANCHORED_END) && (this->unanchored & UNANCHORED_BEGIN /* this is just an optimization, because regex_match will take care of it */)) { pattern.push_back(special_chars_type::dollar()); }
				pattern.swap(this->pattern);
				this->kind = pattern_regex;
			}
			if (this->kind == pattern_regex)
			{
				regex_namespace::regex_constants::syntax_option_type const flags = regex_namespace::regex_constants::optimize
					| regex_namespace::regex_constants::nosubs
					| (this->case_insensitive ? regex_namespace::regex_constants::icase : regex_namespace::regex_constants::syntax_option_type())
					;
				try
				{
					compile(this->pattern.begin(), this->pattern.end(), flags).swap(this->re);
				}
				catch (regex_namespace::regex_error &ex)
				{
					throw std::invalid_argument(ex.what());
				}
			}
			if (this->kind == pattern_verbatim && this->unanchored == (this->unanchored | (UNANCHORED_BEGIN | UNANCHORED_END)))
			{
				this->string_search.emplace(this->pattern.data(), this->pattern.data() + static_cast<ptrdiff_t>(this->pattern.size()));
				this->string_search_ci.emplace(ci_iterator(this->pattern.data()), ci_iterator(this->pattern.data() + static_cast<ptrdiff_t>(this->pattern.size())));
				if (this->pattern.empty())
				{
					this->kind = pattern_anything;
				}
			}
		}
		bool is_match_verbatim(char_type const *const corpus_begin, char_type const *const corpus_end, size_t *const corpus_high_water_mark) const
		{
			bool result;
			typedef char_type const *corpus_iterator;
			bool const case_insensitive = this->case_insensitive,
				unanchored_begin = !!(this->unanchored & UNANCHORED_BEGIN),
				unanchored_end = !!(this->unanchored & UNANCHORED_END);
			typename pattern_type::const_iterator const pattern_begin = this->pattern.begin();
			size_t const
				corpus_length = static_cast<size_t>(std::distance(corpus_begin, corpus_end)),
				pattern_length = static_cast<size_t>(std::distance(pattern_begin, this->pattern.end()));
			if (corpus_high_water_mark) { *corpus_high_water_mark = corpus_length; }
			if (unanchored_end)
			{
				if (unanchored_begin)  // substring
				{
					if (pattern_length <= corpus_length)
					{
						result = (case_insensitive
							? this->string_search_ci(ci_iterator(corpus_begin), ci_iterator(corpus_end)).first.base()
							: this->string_search(iterator(corpus_begin), iterator(corpus_end)).first.base()) != corpus_end;
					}
					else { result = false; }
				}
				else  // prefix
				{
					typename iterator::watermark wm(corpus_begin), *pwm = corpus_high_water_mark ? &wm : NULL;
					typename ci_iterator::watermark wm_ci(corpus_begin), *pwm_ci = corpus_high_water_mark ? &wm_ci : NULL;
					if (pattern_length <= corpus_length)
					{
						corpus_iterator const corpus_match_end = corpus_begin + static_cast<ptrdiff_t>(pattern_length);
						assert(corpus_match_end <= corpus_end && "out-of-bounds access! Fix!");
						result = (case_insensitive
							? std::equal(ci_iterator(corpus_begin, pwm_ci), ci_iterator(corpus_match_end, pwm_ci), ci_iterator(pattern_length ? &*pattern_begin : NULL))
							: std::equal(iterator(corpus_begin, pwm), iterator(corpus_match_end, pwm), pattern_begin));
						if (corpus_high_water_mark) { *corpus_high_water_mark = static_cast<size_t>(case_insensitive ? pwm_ci->end : pwm->end); }
					}
					else { result = false; }
				}
			}
			else
			{
				if (unanchored_begin)  // suffix
				{
					if (pattern_length <= corpus_length)
					{
						corpus_iterator const corpus_match_begin = corpus_end - static_cast<ptrdiff_t>(pattern_length);
						assert(corpus_match_begin >= corpus_begin && "out-of-bounds access! Fix!");
						result = (case_insensitive
							? std::equal(ci_iterator(corpus_match_begin), ci_iterator(corpus_end), ci_iterator(pattern_length ? &*pattern_begin : NULL))
							: std::equal(iterator(corpus_match_begin), iterator(corpus_end), pattern_begin));
					}
					else { result = false; }
				}
				else  // full string
				{
					if (pattern_length == corpus_length)
					{
						result = (case_insensitive
							? std::equal(ci_iterator(corpus_begin), ci_iterator(corpus_end), ci_iterator(pattern_length ? &*pattern_begin : NULL))
							: std::equal(iterator(corpus_begin), iterator(corpus_end), pattern_begin));
					}
					else { result = false; }
				}
			}
			return result;
		}
		bool is_match_regex(char_type const *const corpus_begin, char_type const *const corpus_end, size_t *const corpus_high_water_mark, match_results_type *const mr) const
		{
			typename iterator::watermark wm(corpus_begin), *pwm = corpus_high_water_mark ? &wm : NULL;
			iterator cb(corpus_begin, pwm), ce(corpus_end, pwm);
			bool result;
			result = this->unanchored
				? (mr ? regex_namespace::regex_search(cb, ce, *mr, this->re) : regex_namespace::regex_search(cb, ce, this->re))
				: (mr ? regex_namespace::regex_match(cb, ce, *mr, this->re) : regex_namespace::regex_match(cb, ce, this->re));
			if (corpus_high_water_mark) { *corpus_high_water_mark = static_cast<size_t>(pwm->end); }
			return result;
		}
		bool is_match(char_type const corpus[], size_t const length, size_t *const corpus_high_water_mark)
		{
			char_type const *const corpus_end = corpus + static_cast<ptrdiff_t>(length);
			bool result;
			switch (this->kind)
			{
			case pattern_anything: result = true; break;
			case pattern_verbatim: result = this->is_match_verbatim(corpus, corpus_end, corpus_high_water_mark); break;
			case pattern_regex: result = this->is_match_regex(corpus, corpus_end, corpus_high_water_mark, &this->mr); break;
			default: __debugbreak(); result = false; break;
			}
			return result;
		}
		bool is_match(char_type const corpus[], size_t const length, size_t *const corpus_high_water_mark) const
		{
			char_type const *const corpus_end = corpus + static_cast<ptrdiff_t>(length);
			bool result;
			switch (this->kind)
			{
			case pattern_anything: result = true; break;
			case pattern_verbatim: result = this->is_match_verbatim(corpus, corpus_end, corpus_high_water_mark); break;
			case pattern_regex: result = this->is_match_regex(corpus, corpus_end, corpus_high_water_mark, NULL); break;
			default: __debugbreak(); result = false; break;
			}
			return result;
		}
	};
	impl< char  > narrow;
	impl<wchar_t> wide;

	explicit base_type(pattern_kind const kind, pattern_options const option, wchar_t const pattern[], size_t const length) : narrow(kind, option, wcstombs(pattern, length)), wide(kind, option, pattern, length) { }
	explicit base_type(pattern_kind const kind, pattern_options const option,  char   const pattern[], size_t const length) : narrow(kind, option, pattern, length), wide(kind, option, mbstowcs(pattern, length)) { }

	static size_t tcslen(wchar_t const *const s, size_t const length) { return ~length ? length : wcslen(s); }
	static size_t tcslen( char   const *const s, size_t const length) { return ~length ? length : strlen(s); }

	static std::vector<char> wcstombs(wchar_t const *const input, size_t const length)
	{
		if (~length && length > INT_MAX / (1 << 3)) { throw std::logic_error("length potentialy too long; not supported"); }
		std::vector<char> output;
		for (int i = 0; i < 2; ++i)
		{ output.resize(static_cast<size_t>(WideCharToMultiByte(CP_UTF8, 0, &input[0], ~length ? static_cast<int>(length) : -1, output.empty() ? NULL : &output[0], static_cast<int>(output.size()), NULL, NULL))); }
		if (!~length && !output.empty() && *(output.end() - 1) == '\0') { output.erase(output.end() - 1); }
		return output;
	}

	static std::vector<wchar_t> mbstowcs(char const *const input, size_t const length)
	{
		if (~length && length > INT_MAX / (1 << 3)) { throw std::logic_error("length potentialy too long; not supported"); }
		std::vector<wchar_t> output;
		for (int i = 0; i < 2; ++i)
		{ output.resize(static_cast<size_t>(MultiByteToWideChar(CP_UTF8, 0, &input[0], ~length ? static_cast<int>(length) : -1, output.empty() ? NULL : &output[0], static_cast<int>(output.size())))); }
		if (!~length && !output.empty() && *(output.end() - 1) == L'\0') { output.erase(output.end() - 1); }
		return output;
	}
};

string_matcher::~string_matcher() { delete this->p; }
string_matcher::string_matcher() : p() { }
string_matcher::string_matcher(pattern_kind const kind, pattern_options const option, wchar_t const pattern[], size_t const length) : p(new base_type(kind, option, pattern, base_type::tcslen(pattern, length))) { }
string_matcher::string_matcher(pattern_kind const kind, pattern_options const option,  char   const pattern[], size_t const length) : p(new base_type(kind, option, pattern, base_type::tcslen(pattern, length))) { }
string_matcher::string_matcher(this_type const &other) : p(other.p ? new base_type(*other.p) : NULL) { }
string_matcher::this_type &string_matcher::operator =(this_type other) { other.swap(*this); return *this; }
void string_matcher::swap(this_type &other) { base_type *const p = this->p; this->p = other.p; other.p = p; }

#define X_ASSERT(C) assert(!(C) ? (__debugbreak(), (C)) : true)
static struct string_matcher_tester
{
	string_matcher_tester()
	{
		for (size_t i = 0; i < 2; ++i)
		{
			string_matcher::pattern_kind const kind = i ? string_matcher::pattern_globstar : string_matcher::pattern_glob;
			(void)kind;
			X_ASSERT(!string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("")).is_match(_T("a")));
			X_ASSERT(!string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("?")).is_match(_T("")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("*")).is_match(_T("")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("?")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("*")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("*?")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("?*")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("*?*")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("*a*")).is_match(_T("a")));
			X_ASSERT(!string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("*b*")).is_match(_T("a")));
			X_ASSERT(!string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("ab")));
			X_ASSERT(!string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("a*b")).is_match(_T("a")));
			X_ASSERT(string_matcher(kind, string_matcher::pattern_option_case_insensitive, _T("a*b")).is_match(_T("ab")));
		}

		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("*?")).is_match(_T("a\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a\\")));
		X_ASSERT(!string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("**?")).is_match(_T("a\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("*\\*")).is_match(_T("a\\b")));
		X_ASSERT(!string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("*\\")).is_match(_T("a\\b")));
		X_ASSERT(!string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("\\*")).is_match(_T("a\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("**")).is_match(_T("a")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("**")).is_match(_T("a\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_glob, string_matcher::pattern_option_case_insensitive, _T("**")).is_match(_T("\\b")));

		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*?")).is_match(_T("a\\b")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a\\b")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a\\")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("?*?")).is_match(_T("a")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**?")).is_match(_T("a\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\*")).is_match(_T("a\\b")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\")).is_match(_T("a\\b")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("\\*")).is_match(_T("a\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**")).is_match(_T("a")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**")).is_match(_T("a\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**")).is_match(_T("\\b")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a**b")).is_match(_T("ab")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a**b")).is_match(_T("ac")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a**b")).is_match(_T("acb")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a**c**b")).is_match(_T("acb")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a**cd**b")).is_match(_T("acb")));

		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**\\**\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**\\**\\")).is_match(_T("a\\b\\c\\")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**\\**\\")).is_match(_T("a\\b\\c")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\**\\**\\")).is_match(_T("a\\b\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**")).is_match(_T("a\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("*\\**\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\*\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\*\\*\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("a?\\**")).is_match(_T("ab\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\*")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\*\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("\\*\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\**\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\**\\**")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\*\\*\\*\\")).is_match(_T("a\\b\\c\\d\\")));
		X_ASSERT(!string_matcher(string_matcher::pattern_globstar, string_matcher::pattern_option_case_insensitive, _T("**\\*\\*\\*\\*\\")).is_match(_T("a\\b\\c\\d\\")));

		X_ASSERT(!string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T(".")).is_match(_T("ab")));
		X_ASSERT(string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T("..")).is_match(_T("ab")));
		X_ASSERT(string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T(".*")).is_match(_T("ab")));
		X_ASSERT(!string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T("^.")).is_match(_T("ab")));
		X_ASSERT(string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T("^.*")).is_match(_T("ab")));
		X_ASSERT(string_matcher(string_matcher::pattern_regex, string_matcher::pattern_option_case_insensitive, _T("^.*$")).is_match(_T("ab")));

	}
} const string_matcher_test;
#undef X_ASSERT

/* NOTE:
Proper usage of the high-water mark for prefix matching requires having an extra character past the end of the string to see whether it is dereferenced.
Checking the last character of the string itself gives false positives, since we do not know whether a mismatch was due to the character or due to the string length.
Even then, this is only valid if the string is examined character-by-character. It doesn't work if the string length is inspected for matching before all characters are examined.
*/

bool string_matcher::is_match(wchar_t const str[], size_t const length, size_t *const corpus_high_water_mark) const { return this->p->wide  .is_match(str, base_type::tcslen(str, length), corpus_high_water_mark); }
bool string_matcher::is_match(wchar_t const str[], size_t const length, size_t *const corpus_high_water_mark)       { return this->p->wide  .is_match(str, base_type::tcslen(str, length), corpus_high_water_mark); }
bool string_matcher::is_match( char   const str[], size_t const length, size_t *const corpus_high_water_mark) const { return this->p->narrow.is_match(str, base_type::tcslen(str, length), corpus_high_water_mark); }
bool string_matcher::is_match( char   const str[], size_t const length, size_t *const corpus_high_water_mark)       { return this->p->narrow.is_match(str, base_type::tcslen(str, length), corpus_high_water_mark); }

template<> struct string_matcher::base_type::special_chars<char>
{
	typedef char char_type;
	static char_type slash() { return '/'; }
	static char_type backslash() { return '\\'; }
	static char_type period() { return '.'; }
	static char_type dash() { return '-'; }
	static char_type plus() { return '+'; }
	static char_type asterisk() { return '*'; }
	static char_type question() { return '?'; }
	static char_type open_bracket() { return '['; }
	static char_type close_bracket() { return ']'; }
	static char_type open_brace() { return '{'; }
	static char_type close_brace() { return '}'; }
	static char_type open_parenthesis() { return '('; }
	static char_type close_parenthesis() { return ')'; }
	static char_type comma() { return ','; }
	static char_type colon() { return ':'; }
	static char_type caret() { return '^'; }
	static char_type dollar() { return '$'; }
	static char_type pipe() { return '|'; }
	static char_type number() { return '#'; }
	static char_type carriage_return() { return '\r'; }
	static char_type line_feed() { return '\n'; }
};

template<> struct string_matcher::base_type::special_chars<wchar_t>
{
	typedef wchar_t char_type;
	static char_type slash() { return L'/'; }
	static char_type backslash() { return L'\\'; }
	static char_type period() { return L'.'; }
	static char_type dash() { return L'-'; }
	static char_type plus() { return L'+'; }
	static char_type asterisk() { return L'*'; }
	static char_type question() { return L'?'; }
	static char_type open_bracket() { return L'['; }
	static char_type close_bracket() { return L']'; }
	static char_type open_brace() { return L'{'; }
	static char_type close_brace() { return L'}'; }
	static char_type open_parenthesis() { return L'('; }
	static char_type close_parenthesis() { return L')'; }
	static char_type comma() { return L','; }
	static char_type colon() { return L':'; }
	static char_type caret() { return L'^'; }
	static char_type dollar() { return L'$'; }
	static char_type pipe() { return L'|'; }
	static char_type number() { return L'#'; }
	static char_type carriage_return() { return L'\r'; }
	static char_type line_feed() { return L'\n'; }
};

#ifdef __clang__
#pragma clang diagnostic pop
#endif
