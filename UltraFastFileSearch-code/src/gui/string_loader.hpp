// ============================================================================
// string_loader.hpp - GUI string utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================
#pragma once

#include <vector>
#include <stdexcept>
#include <cassert>

#include <Windows.h>
#include <tchar.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlmisc.h>

extern WTL::CAppModule _Module;
extern HMODULE mui_module;

namespace uffs {
namespace gui {

// ============================================================================
// RefCountedCString - Ref-counted string wrapper for WTL::CString
// ============================================================================
// Ensures that copying doesn't move the buffer (e.g. in case a containing
// vector resizes). This is important for maintaining stable pointers.
// ============================================================================
class RefCountedCString : public WTL::CString
{
	typedef RefCountedCString this_type;
	typedef WTL::CString base_type;

	void check_same_buffer(this_type const& other) const
	{
		// In newer ATL, GetData() is private. Use GetString() instead -
		// if strings share the same buffer, GetString() returns the same pointer.
		if (this->GetString() != other.GetString())
		{
			throw std::logic_error("expected the same buffer for both strings");
		}
	}

public:
	RefCountedCString() : base_type() {}

	RefCountedCString(this_type const& other) : base_type(other)
	{
		this->check_same_buffer(other);
	}

	this_type& operator=(this_type const& other)
	{
		this_type& result = static_cast<this_type&>(
			this->base_type::operator=(static_cast<base_type const&>(other)));
		result.check_same_buffer(other);
		return result;
	}

	LPTSTR data()
	{
		return this->base_type::GetBuffer(this->base_type::GetLength());
	}

	LPTSTR c_str()
	{
		int const n = this->base_type::GetLength();
		LPTSTR const result = this->base_type::GetBuffer(n + 1);
		if (result[n] != _T('\0'))
		{
			result[n] = _T('\0');
		}
		return result;
	}

	operator LPTSTR()
	{
		return this->c_str();
	}

	// Do NOT implement this. DDK's implementation doesn't handle embedded
	// null characters correctly. Just use a basic_string directly instead.
	friend std::basic_ostream<TCHAR>& operator<<(
		std::basic_ostream<TCHAR>& ss, this_type& me);
};

// ============================================================================
// StringLoader - Thread-safe string resource loader with MUI support
// ============================================================================
// Loads string resources from the MUI module if available, falling back
// to the main module. Caches loaded strings for efficiency.
// ============================================================================
class StringLoader
{
	class SwapModuleResource
	{
		SwapModuleResource(SwapModuleResource const&);
		SwapModuleResource& operator=(SwapModuleResource const&);
		HINSTANCE prev;

	public:
		~SwapModuleResource()
		{
			InterlockedExchangePointer(
				reinterpret_cast<void**>(&_Module.m_hInstResource), prev);
		}

		SwapModuleResource(HINSTANCE const instance) : prev()
		{
			InterlockedExchangePointer(
				reinterpret_cast<void**>(&_Module.m_hInstResource), mui_module);
		}
	};

	DWORD thread_id;
	std::vector<RefCountedCString> strings;

public:
	StringLoader() : thread_id(GetCurrentThreadId()) {}

	RefCountedCString& operator()(unsigned short const id)
	{
		if (id >= this->strings.size())
		{
			assert(GetCurrentThreadId() && this->thread_id &&
				"cannot expand string table from another thread");
			this->strings.resize(static_cast<size_t>(id) + 1);
		}

		if (this->strings[id].IsEmpty())
		{
			assert(GetCurrentThreadId() && this->thread_id &&
				"cannot modify string table from another thread");
			RefCountedCString& str = this->strings[id];
			bool success = mui_module &&
				(SwapModuleResource(mui_module), !!str.LoadString(id));
			if (!success)
			{
				(void)str.LoadString(id);
			}
		}

		return this->strings[id];
	}
};

} // namespace gui
} // namespace uffs

