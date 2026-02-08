#pragma once
/**
 * @file overlapped.hpp
 * @brief Overlapped I/O base class and related utilities for async Windows I/O
 *
 * @details
 * This file provides the foundation for asynchronous I/O operations using
 * Windows I/O Completion Ports (IOCP). It contains:
 *
 * - **Overlapped**: Base class for async I/O operations with reference counting
 * - **negative_one**: Helper for sentinel values (e.g., INVALID_HANDLE_VALUE)
 * - **value_initialized**: Template wrapper ensuring zero-initialization
 * - **constant**: Compile-time constant multiplication optimization
 *
 * ## Architecture Overview
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    Overlapped I/O Flow                          │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │  ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐  │
 * │  │  Application │───>│  ReadFile/   │───>│  I/O Completion  │  │
 * │  │  Code        │    │  WriteFile   │    │  Port (IOCP)     │  │
 * │  └──────────────┘    └──────────────┘    └────────┬─────────┘  │
 * │         │                                          │            │
 * │         │                                          v            │
 * │         │            ┌──────────────────────────────────────┐  │
 * │         │            │  Worker Thread Pool                   │  │
 * │         │            │  GetQueuedCompletionStatus()          │  │
 * │         │            └────────────────┬─────────────────────┘  │
 * │         │                             │                        │
 * │         │                             v                        │
 * │         │            ┌──────────────────────────────────────┐  │
 * │         └───────────>│  Overlapped::operator()              │  │
 * │                      │  (Completion Callback)               │  │
 * │                      └──────────────────────────────────────┘  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ## Thread Safety
 *
 * - Overlapped objects are reference-counted (thread-safe via RefCounted)
 * - The operator() callback is invoked on a worker thread
 * - Derived classes must ensure their state is thread-safe
 *
 * ## Usage Example
 *
 * ```cpp
 * class MyReadOperation : public Overlapped {
 *     std::vector<char> buffer_;
 * public:
 *     MyReadOperation(size_t size) : buffer_(size) {}
 *
 *     int operator()(size_t bytes_read, uintptr_t key) override {
 *         // Process the data in buffer_
 *         process_data(buffer_.data(), bytes_read);
 *         return -1;  // Request destruction
 *     }
 *
 *     void* buffer() { return buffer_.data(); }
 *     size_t size() { return buffer_.size(); }
 * };
 *
 * // Issue async read
 * intrusive_ptr<MyReadOperation> op(new MyReadOperation(4096));
 * op->offset(file_offset);
 * ReadFile(handle, op->buffer(), op->size(), nullptr, op.get());
 * op.detach();  // IOCP now owns the reference
 * ```
 *
 * @see IoCompletionPort - Manages the IOCP and worker threads
 * @see intrusive_ptr - Smart pointer for reference-counted objects
 */

#ifndef UFFS_OVERLAPPED_HPP
#define UFFS_OVERLAPPED_HPP

#include <Windows.h>
#include <climits>
#include "util/intrusive_ptr.hpp"

/**
 * @class Overlapped
 * @brief Base class for asynchronous I/O operations using IOCP
 *
 * @details
 * Inherits from both Windows OVERLAPPED structure and RefCounted for
 * automatic reference counting. Derived classes implement operator()
 * as the completion callback.
 *
 * ## Return Value Semantics
 *
 * The operator() return value controls post-completion behavior:
 *
 * | Return | Meaning                                      |
 * |--------|----------------------------------------------|
 * | > 0    | Re-queue this operation to the IOCP          |
 * | = 0    | Keep alive but don't re-queue                |
 * | < 0    | Release reference (may destroy object)       |
 *
 * ## Memory Layout
 *
 * ```
 * ┌─────────────────────────────────────────┐
 * │ Overlapped Object                        │
 * ├─────────────────────────────────────────┤
 * │ OVERLAPPED (Windows structure)           │
 * │   - Internal, InternalHigh               │
 * │   - Offset, OffsetHigh (file position)   │
 * │   - hEvent                               │
 * ├─────────────────────────────────────────┤
 * │ RefCounted<Overlapped>                   │
 * │   - reference_count_ (atomic)            │
 * ├─────────────────────────────────────────┤
 * │ Derived class data...                    │
 * └─────────────────────────────────────────┘
 * ```
 */
class Overlapped : public OVERLAPPED, public RefCounted<Overlapped>
{
	Overlapped(Overlapped const&) = delete;
	Overlapped& operator=(Overlapped const&) = delete;
public:
	virtual ~Overlapped() = default;

	Overlapped() noexcept : OVERLAPPED() {}

	virtual int /*> 0 if re-queue requested, = 0 if no re-queue but no destruction, < 0 if destruction requested */ operator()(size_t size, uintptr_t /*key*/) = 0;

	[[nodiscard]] long long offset() const noexcept
	{
		return (static_cast<long long>(this->OVERLAPPED::OffsetHigh) << (CHAR_BIT * sizeof(this->OVERLAPPED::Offset))) | this->OVERLAPPED::Offset;
	}

	void offset(long long value) noexcept
	{
		this->OVERLAPPED::Offset = static_cast<unsigned long>(value);
		this->OVERLAPPED::OffsetHigh = static_cast<unsigned long>(value >> (CHAR_BIT * sizeof(this->OVERLAPPED::Offset)));
	}
};

/**
 * @struct negative_one
 * @brief Helper for creating sentinel values like INVALID_HANDLE_VALUE
 *
 * @details
 * Converts to any type T as ~T() (bitwise NOT of zero), producing:
 * - For integers: -1 (all bits set)
 * - For pointers: INVALID_HANDLE_VALUE equivalent
 *
 * ## Usage Example
 * ```cpp
 * HANDLE h = negative_one;  // h == INVALID_HANDLE_VALUE
 * int i = negative_one;     // i == -1
 * ```
 */
static struct negative_one
{
	template <class T>
	operator T() const
	{
		return static_cast<T>(~T());
	}
}
const negative_one;

/**
 * @struct value_initialized
 * @brief Wrapper ensuring zero/value initialization for any type T
 *
 * @details
 * Guarantees that the wrapped value is zero-initialized even when
 * default-constructed. This is important for POD types that would
 * otherwise contain garbage values.
 *
 * ## Usage Example
 * ```cpp
 * struct MyStruct {
 *     value_initialized<int> count;      // Always 0
 *     value_initialized<bool> enabled;   // Always false
 *     value_initialized<void*> ptr;      // Always nullptr
 * };
 *
 * MyStruct s;  // All members are zero-initialized
 * ```
 *
 * @tparam T The type to wrap (must be default-constructible)
 */
template <class T>
struct value_initialized
{
	typedef value_initialized this_type, type;
	T value;
	value_initialized() noexcept : value() {}

	value_initialized(T const& value) noexcept : value(value) {}

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

/**
 * @class constant
 * @brief Compile-time constant multiplication optimization
 *
 * @details
 * Provides optimized multiplication by compile-time constants using
 * bit manipulation instead of the MUL instruction. The compiler can
 * often do this automatically, but this template ensures it.
 *
 * ## Algorithm
 *
 * Multiplication by a constant is decomposed into shifts and adds:
 * - m * 5 = (m << 2) + m
 * - m * 13 = (m << 4) - (m << 1) - m
 *
 * ## Specializations
 *
 * - `constant<0xD>` (13): Optimized as (m << 4) - (m << 1) - m
 *
 * ## Usage Example
 * ```cpp
 * size_t result = value * constant<13>();  // Optimized multiply by 13
 * ```
 *
 * @tparam V The constant multiplier value
 */
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

/// Specialization for multiply by 13: (m << 4) - (m << 1) - m
template <> class constant<0xD>
{
public:
	friend size_t operator*(size_t const m, constant<0xD> const)
	{
		return (m << 4) - (m << 1) - m;
	}
};

#endif // UFFS_OVERLAPPED_HPP
