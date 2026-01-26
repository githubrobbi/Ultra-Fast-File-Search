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
// - Aggregate-style initialization: File output = { fd };
// - Implicit conversion to handle_type for comparisons
// - Custom operator& for passing to functions expecting int*
// - Destructor closes the file descriptor
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

	[[nodiscard]] handle_type get() const noexcept { return f; }
	[[nodiscard]] bool valid() const noexcept { return f != 0; }

	operator handle_type&() noexcept { return this->f; }
	[[nodiscard]] operator handle_type() const noexcept { return this->f; }
	handle_type* operator&() noexcept { return &this->f; }
};

} // namespace uffs

