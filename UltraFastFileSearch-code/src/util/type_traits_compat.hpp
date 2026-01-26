// ============================================================================
// type_traits_compat.hpp - Type traits compatibility for older compilers
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// Provides remove_const, remove_volatile, remove_cv for pre-C++11 compilers
// ============================================================================
#pragma once

namespace stdext {

template <class T>
struct remove_const
{
	typedef T type;
};

template <class T>
struct remove_const<const T>
{
	typedef T type;
};

template <class T>
struct remove_volatile
{
	typedef T type;
};

template <class T>
struct remove_volatile<volatile T>
{
	typedef T type;
};

template <class T>
struct remove_cv
{
	typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

} // namespace stdext

