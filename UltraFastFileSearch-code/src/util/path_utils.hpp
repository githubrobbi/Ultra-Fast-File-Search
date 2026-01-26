// ============================================================================
// path_utils.hpp - Path manipulation utilities
// ============================================================================
// Extracted from UltraFastFileSearch.cpp during Wave 2 refactoring
// ============================================================================
#pragma once

#include <Windows.h>
#include <ShlObj.h>
#include <tchar.h>

#include "core_types.hpp"
#include "path.hpp"

namespace uffs {

// ============================================================================
// remove_path_stream_and_trailing_sep - Clean up path by removing streams and trailing separators
// ============================================================================
inline void remove_path_stream_and_trailing_sep(std::tvstring& path)
{
	size_t ifirstsep = 0;
	while (ifirstsep < path.size())
	{
		if (isdirsep(path[ifirstsep]))
		{
			break;
		}
		++ifirstsep;
	}

	while (!path.empty() && isdirsep(*(path.end() - 1)))
	{
		if (path.size() <= ifirstsep + 1)
		{
			break;
		}
		path.erase(path.end() - 1);
	}

	for (size_t i = path.size(); i != 0 && ((void)--i, true);)
	{
		if (path[i] == _T(':'))
		{
			path.erase(path.begin() + static_cast<ptrdiff_t>(i), path.end());
		}
		else if (isdirsep(path[i]))
		{
			break;
		}
	}

	while (!path.empty() && isdirsep(*(path.end() - 1)))
	{
		if (path.size() <= ifirstsep + 1)
		{
			break;
		}
		path.erase(path.end() - 1);
	}

	if (!path.empty() && *(path.end() - 1) == _T('.') && (path.size() == 1 || isdirsep(*(path.end() - 2))))
	{
		path.erase(path.end() - 1);
	}
}

// ============================================================================
// NormalizePath - Normalize a path (collapse separators, make absolute)
// ============================================================================
inline std::tvstring NormalizePath(std::tvstring const& path)
{
	std::tvstring result;
	bool wasSep = false;
	bool isCurrentlyOnPrefix = true;
	for (TCHAR const& c : path)
	{
		isCurrentlyOnPrefix &= isdirsep(c);
		if (isCurrentlyOnPrefix || !wasSep || !isdirsep(c))
		{
			result.push_back(c);
		}
		wasSep = isdirsep(c);
	}

	if (!isrooted(result.begin(), result.end()))
	{
		std::tvstring currentDir(32 * 1024, _T('\0'));
		currentDir.resize(GetCurrentDirectory(static_cast<DWORD>(currentDir.size()), &currentDir[0]));
		adddirsep(currentDir);
		result = currentDir + result;
	}

	remove_path_stream_and_trailing_sep(result);
	return result;
}

// ============================================================================
// GetDisplayName - Get shell display name for a path
// ============================================================================
inline std::tstring GetDisplayName(HWND hWnd, const std::tstring& path, DWORD shgdn)
{
	ATL::CComPtr<IShellFolder> desktop;
	STRRET ret = {};
	LPITEMIDLIST shidl = nullptr;
	ATL::CComBSTR bstr;
	ULONG attrs = SFGAO_ISSLOW | SFGAO_HIDDEN;
	std::tstring result = (!path.empty() && SHGetDesktopFolder(&desktop) == S_OK &&
		desktop->ParseDisplayName(hWnd, nullptr, const_cast<LPWSTR>(path.c_str()), nullptr, &shidl, &attrs) == S_OK &&
		(attrs & SFGAO_ISSLOW) == 0 &&
		desktop->GetDisplayNameOf(shidl, shgdn, &ret) == S_OK &&
		StrRetToBSTR(&ret, shidl, &bstr) == S_OK
		) ? static_cast<LPCTSTR>(bstr) : std::tstring(basename(path.begin(), path.end()), path.end());
	return result;
}

} // namespace uffs

