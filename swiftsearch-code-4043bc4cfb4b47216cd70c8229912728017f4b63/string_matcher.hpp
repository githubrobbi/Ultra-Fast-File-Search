#pragma once

class string_matcher
{
	struct base_type;
	typedef string_matcher this_type;
	base_type *p;
public:
	enum pattern_kind
	{
		pattern_anything,
		pattern_verbatim,
		pattern_glob,
		pattern_globstar,
		pattern_regex
	};
	enum pattern_options
	{
		pattern_option_none = 0,
		pattern_option_case_insensitive = 1 << 0
	};
	~string_matcher();
	string_matcher();
	explicit string_matcher(pattern_kind const kind, pattern_options const option, wchar_t const pattern[], size_t const length = ~size_t());
	explicit string_matcher(pattern_kind const kind, pattern_options const option,  char   const pattern[], size_t const length = ~size_t());
	string_matcher(this_type const &other);
	this_type &operator =(this_type other);
	void swap(this_type &other);
	friend void swap(this_type &a, this_type &b) { return a.swap(b); }
	bool is_match(wchar_t const str[], size_t const length = ~size_t(), size_t *const corpus_high_water_mark = NULL) const;
	bool is_match(wchar_t const str[], size_t const length = ~size_t(), size_t *const corpus_high_water_mark = NULL);
	bool is_match( char   const str[], size_t const length = ~size_t(), size_t *const corpus_high_water_mark = NULL) const;
	bool is_match( char   const str[], size_t const length = ~size_t(), size_t *const corpus_high_water_mark = NULL);
};

template<class Char> Char totlower(Char const c);
template<>  char   totlower< char  >( char   const ch);
template<> wchar_t totlower<wchar_t>(wchar_t const ch);
template<class Char> Char totupper(Char const c);
template<> char   totupper< char  >( char   const ch);
template<> wchar_t totupper<wchar_t>(wchar_t const ch);

template<class Char, Char F(Char const)> struct char_transformer { Char operator()(Char const ch) const { return F(ch); } };
