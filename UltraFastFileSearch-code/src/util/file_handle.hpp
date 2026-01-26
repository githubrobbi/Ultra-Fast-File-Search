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

	// Default and value constructor - NOT explicit to allow brace initialization
	// This matches original behavior: File output = { fd };
	File() noexcept : f(0) {}
	File(handle_type fd) noexcept : f(fd) {}

	// Non-copyable (file descriptors shouldn't be implicitly copied)
	File(const File&) = delete;
	File& operator=(const File&) = delete;

	// Movable
	File(File&& other) noexcept : f(other.f) { other.f = 0; }
	File& operator=(File&& other) noexcept
	{
		if (this != &other)
		{
			if (f) { _close(f); }
			f = other.f;
			other.f = 0;
		}
		return *this;
	}

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

