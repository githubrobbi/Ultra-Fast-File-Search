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
// Note: This struct intentionally matches the original monolith behavior:
// - No explicit constructor (allows brace initialization)
// - Implicit copy/move (original didn't restrict these)
// ============================================================================
struct File
{
	typedef int handle_type;
	handle_type f;

	~File()
	{
		if (f)
		{
			_close(f);
		}
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

