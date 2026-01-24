#pragma once

// ============================================================================
// Overlapped I/O Base Class and Related Utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp
// 
// Contains:
//   - Overlapped: Base class for async I/O operations using IOCP
//   - negative_one: Helper struct for sentinel values
//   - value_initialized: Template wrapper ensuring value initialization
//   - constant: Template class for compile-time constant multiplication
// ============================================================================

#include <Windows.h>
#include <climits>
#include "../util/intrusive_ptr.hpp"

// ============================================================================
// Overlapped Base Class
// ============================================================================
// Inherits from both Windows OVERLAPPED and RefCounted for reference counting.
// Pure virtual operator() serves as completion callback.
// ============================================================================

class Overlapped : public OVERLAPPED, public RefCounted<Overlapped>
{
	Overlapped(Overlapped const&);
	Overlapped& operator=(Overlapped const&);
public:
	virtual ~Overlapped() {}

	Overlapped() : OVERLAPPED() {}

	virtual int /*> 0 if re-queue requested, = 0 if no re-queue but no destruction, < 0 if destruction requested */ operator()(size_t const size, uintptr_t const /*key*/) = 0;

	long long offset() const
	{
		return (static_cast<long long>(this->OVERLAPPED::OffsetHigh) << (CHAR_BIT * sizeof(this->OVERLAPPED::Offset))) | this->OVERLAPPED::Offset;
	}

	void offset(long long const value)
	{
		this->OVERLAPPED::Offset = static_cast<unsigned long>(value);
		this->OVERLAPPED::OffsetHigh = static_cast<unsigned long>(value >> (CHAR_BIT * sizeof(this->OVERLAPPED::Offset)));
	}
};

// ============================================================================
// negative_one Helper
// ============================================================================
// Converts to any type T as ~T() - useful for sentinel values like
// INVALID_HANDLE_VALUE
// ============================================================================

static struct negative_one
{
	template <class T>
	operator T() const
	{
		return static_cast<T>(~T());
	}
}
const negative_one;

// ============================================================================
// value_initialized Template
// ============================================================================
// Wrapper ensuring value initialization for any type T
// ============================================================================

template <class T>
struct value_initialized
{
	typedef value_initialized this_type, type;
	T value;
	value_initialized() : value() {}

	value_initialized(T const& value) : value(value) {}

	operator T& ()
	{
		return this->value;
	}

	operator T const& () const
	{
		return this->value;
	}

	operator T volatile& () volatile
	{
		return this->value;
	}

	operator T const volatile& () const volatile
	{
		return this->value;
	}

	T* operator&()
	{
		return &this->value;
	}

	T const* operator&() const
	{
		return &this->value;
	}

	T volatile* operator&() volatile
	{
		return &this->value;
	}

	T const volatile* operator&() const volatile
	{
		return &this->value;
	}
};

// ============================================================================
// constant Template Class
// ============================================================================
// Compile-time constant multiplication optimization
// ============================================================================

template <size_t V>
class constant
{
	template <size_t I, size_t N>
	struct impl;
public:
	friend size_t operator*(size_t const m, constant<V> const)
	{
		return impl<0, sizeof(V) * CHAR_BIT>::multiply(m);
	}
};

template <size_t V> template <size_t I, size_t N> struct constant<V>::impl
{
	static size_t multiply(size_t const m)
	{
		return impl<I, N / 2>::multiply(m) + impl<I + N / 2, N - N / 2>::multiply(m);
	}
};

template <size_t V> template <size_t I> struct constant<V>::impl<I, 1>
{
	static size_t multiply(size_t const m)
	{
		return (V & static_cast<size_t>(static_cast<size_t>(1) << I)) ? static_cast<size_t>(m << I) : size_t();
	}
};

template <> class constant<0xD>
{
public:
	friend size_t operator*(size_t const m, constant<0xD> const)
	{
		return (m << 4) - (m << 1) - m;
	}
};

