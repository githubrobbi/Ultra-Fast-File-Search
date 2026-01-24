#pragma once
/**
 * @file containers.hpp
 * @brief Custom container utilities for UFFS
 *
 * Contains:
 * - vector_with_fast_size: Vector wrapper with cached O(1) size access
 * - Speed: Performance measurement struct (bytes + clock ticks)
 */

#include <vector>
#include <memory>
#include <utility>
#include <ctime>

/**
 * @brief Vector with cached size for O(1) size() access
 *
 * std::vector::size() can be O(n) on some implementations.
 * This wrapper caches the size for guaranteed O(1) access.
 */
template <class T, class Ax = std::allocator<T>>
class vector_with_fast_size : std::vector<T, Ax>
{
	typedef std::vector<T, Ax> base_type;
	typename base_type::size_type _size;
public: vector_with_fast_size() : _size() {}

	  typedef typename base_type::const_iterator const_iterator;
	  typedef typename base_type::iterator iterator;
	  typedef typename base_type::size_type size_type;
	  typedef typename base_type::value_type value_type;
	  using base_type::at;
	  using base_type::back;
	  using base_type::begin;
	  using base_type::capacity;
	  using base_type::empty;
	  using base_type::end;
	  using base_type::front;
	  using base_type::reserve;
	  using base_type::operator[];
	  size_type size() const
	  {
		  return this->_size;
	  }

	  void resize(size_type
		  const size)
	  {
		  this->base_type::resize(size);
		  this->_size = size;
	  }

	  void resize(size_type
		  const size, value_type
		  const& default_value)
	  {
		  this->base_type::resize(size, default_value);
		  this->_size = size;
	  }

	  void push_back(value_type
		  const& value)
	  {
		  this->base_type::push_back(value);
		  ++this->_size;
	  }
};

/**
 * @brief Performance measurement struct
 *
 * Pairs bytes processed with clock ticks elapsed.
 * Used for throughput calculations.
 */
struct Speed : std::pair<unsigned long long, clock_t>
{
	Speed() {}

	Speed(first_type
		const& first, second_type
		const& second) : std::pair<first_type, second_type>(first, second) {}

	Speed& operator+=(Speed
		const& other)
	{
		this->first += other.first;
		this->second += other.second;
		return *this;
	}
};

