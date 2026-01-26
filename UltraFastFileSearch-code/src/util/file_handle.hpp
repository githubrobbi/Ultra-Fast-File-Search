// ============================================================================
// file_handle.hpp - RAII wrapper for C file handles
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================
#pragma once

#include <io.h>

namespace uffs {

// ============================================================================
// File - RAII wrapper for C file descriptors
// ============================================================================
struct File
{
	typedef int handle_type;
	handle_type f;

	File() : f(0) {}
	explicit File(handle_type h) : f(h) {}

	~File()
	{
		if (f)
		{
			_close(f);
		}
	}

	// Non-copyable
	File(const File&) = delete;
	File& operator=(const File&) = delete;

	// Movable
	File(File&& other) noexcept : f(other.f) { other.f = 0; }
	File& operator=(File&& other) noexcept
	{
		if (this != &other)
		{
			if (f) _close(f);
			f = other.f;
			other.f = 0;
		}
		return *this;
	}

	operator handle_type&()
	{
		return this->f;
	}

	operator handle_type() const
	{
		return this->f;
	}

	handle_type* operator&()
	{
		return &this->f;
	}
};

} // namespace uffs

