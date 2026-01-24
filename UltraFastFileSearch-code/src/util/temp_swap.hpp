/**
 * @file temp_swap.hpp
 * @brief RAII class for temporarily swapping values
 *
 * TempSwap provides a RAII wrapper that temporarily replaces a value
 * with a new one and restores the original value upon destruction.
 *
 * @note Part of the UltraFastFileSearch modular refactoring
 */

#ifndef UFFS_UTIL_TEMP_SWAP_HPP
#define UFFS_UTIL_TEMP_SWAP_HPP

#include <utility>  // std::swap

/**
 * @brief RAII class for temporarily swapping a value
 * @tparam T The type of value to swap
 *
 * Usage:
 * @code
 * int value = 10;
 * {
 *     TempSwap<int> guard(value, 20);  // value is now 20
 *     // ... do work with value == 20 ...
 * }  // value is restored to 10
 * @endcode
 */
template <class T>
class TempSwap
{
	TempSwap(TempSwap const&);
	TempSwap& operator=(TempSwap const&);
	T* target, old_value;
public:
	~TempSwap()
	{
		if (this->target)
		{
			using std::swap;
			swap(*this->target, this->old_value);
		}
	}

	TempSwap() : target(), old_value() {}

	explicit TempSwap(T& target, T const& new_value) : target(&target), old_value(new_value)
	{
		using std::swap;
		swap(*this->target, this->old_value);
	}

	void reset()
	{
		TempSwap().swap(*this);
	}

	void reset(T& target, T const& new_value)
	{
		TempSwap(target, new_value).swap(*this);
	}

	void swap(TempSwap& other)
	{
		using std::swap;
		swap(this->target, other.target);
		swap(this->old_value, other.old_value);
	}
};

#endif // UFFS_UTIL_TEMP_SWAP_HPP

