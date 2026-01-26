// ============================================================================
// sort_utils.hpp - Sorting utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================
#pragma once

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

namespace uffs {

// ============================================================================
// is_sorted_ex - Check if range is sorted (with optional reverse check)
// ============================================================================
template <class It, class Less>
[[nodiscard]] bool is_sorted_ex(It begin, It const end, Less less, bool const reversed = false)
{
	if (begin != end)
	{
		It i(begin);
		It const& left = reversed ? i : begin;
		It const& right = reversed ? begin : i;
		++i;
		while (i != end)
		{
			if (less(*right, *left))
			{
				return false;
			}
			begin = i;
			++i;
		}
	}
	return true;
}

// ============================================================================
// stable_sort_by_key_comparator - Comparator for stable sorting by key
// ============================================================================
template <class ValueType, class KeyComp>
struct stable_sort_by_key_comparator : KeyComp
{
	explicit stable_sort_by_key_comparator(KeyComp const& comp = KeyComp()) : KeyComp(comp) {}

	typedef ValueType value_type;
	bool operator()(value_type const& a, value_type const& b) const
	{
		return this->KeyComp::operator()(a.first, b.first) ||
			(!this->KeyComp::operator()(b.first, a.first) && (a.second < b.second));
	}
};

// ============================================================================
// stable_sort_by_key - Stable sort using a key extractor
// ============================================================================
template <class It, class Key, class Swapper>
void stable_sort_by_key(It begin, It end, Key key, Swapper swapper)
{
	typedef typename std::iterator_traits<It>::difference_type Diff;
	typedef std::less<typename Key::result_type> KeyComp;
	typedef std::vector<std::pair<typename Key::result_type, Diff>> Keys;
	Keys keys;
	Diff const n = std::distance(begin, end);
	keys.reserve(static_cast<typename Keys::size_type>(n));
	{
		Diff j = 0;
		for (It i = begin; i != end; ++i)
		{
			keys.push_back(typename Keys::value_type(key(*i), j++));
		}
	}

	std::stable_sort(keys.begin(), keys.end(),
		stable_sort_by_key_comparator<typename Keys::value_type, KeyComp>());

	for (Diff i = 0; i != n; ++i)
	{
		for (Diff j = i;;)
		{
			using std::swap;
			swap(j, keys[j].second);
			swapper(*(begin + j), *(begin + j));
			if (j == i)
			{
				break;
			}
			using std::iter_swap;
			swapper(*(begin + j), *(begin + keys[j].second));
		}
	}
}

// ============================================================================
// My_Stable_sort_unchecked1 - MSVC STL internal stable sort helper
// ============================================================================
// Only needed for older MSVC STL versions (610 <= _CPPLIB_VER < 650)
// ============================================================================
#if defined(_CPPLIB_VER) && 610 <= _CPPLIB_VER && _CPPLIB_VER < 650

template <class _BidIt, class _Diff, class _Ty, class _Pr>
inline void My_Stable_sort_unchecked1(_BidIt _First, _BidIt _Last, _Diff _Count,
	std::_Temp_iterator<_Ty>& _Tempbuf, _Pr& _Pred)
{
	// sort preserving order of equivalents, using _Pred
	{
		// sort halves and merge
		_Diff _Count2 = (_Count + 1) / 2;
		_BidIt _Mid = _First;
		_STD advance(_Mid, _Count2);

		if (_Count2 <= _Tempbuf._Maxlen())
		{
			// temp buffer big enough, sort each half using buffer
			std::_Buffered_merge_sort_unchecked(_First, _Mid, _Count2, _Tempbuf, _Pred);
			std::_Buffered_merge_sort_unchecked(_Mid, _Last, _Count - _Count2,
				_Tempbuf, _Pred);
		}
		else
		{
			// temp buffer not big enough, divide and conquer
			My_Stable_sort_unchecked1(_First, _Mid, _Count2, _Tempbuf, _Pred);
			My_Stable_sort_unchecked1(_Mid, _Last, _Count - _Count2, _Tempbuf, _Pred);
		}

		std::_Buffered_merge_unchecked(_First, _Mid, _Last,
			_Count2, _Count - _Count2, _Tempbuf, _Pred);	// merge halves
	}
}

#endif // _CPPLIB_VER

} // namespace uffs

