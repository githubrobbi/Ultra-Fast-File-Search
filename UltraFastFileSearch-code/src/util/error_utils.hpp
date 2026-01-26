#pragma once
/**
 * @file error_utils.hpp
 * @brief Error handling utilities extracted from UltraFastFileSearch.cpp
 * 
 * Contains:
 * - safe_stprintf: Safe string formatting
 * - CppRaiseException: Convert SEH to C++ exceptions
 * - CheckAndThrow: Check and throw on failure
 * - GetAnyErrorText: Get error text from error code
 * - GetLastErrorAsString: Get last Win32 error as string
 */

#ifndef UFFS_ERROR_UTILS_HPP
#define UFFS_ERROR_UTILS_HPP

// Windows and ATL headers (expected to be included via stdafx.h in the project)
#include <Windows.h>
#include <tchar.h>
#include <atlbase.h>
#include <atlexcept.h>

#include <cstdarg>
#include <string>

namespace uffs {
namespace error {

/**
 * @brief Safe sprintf for TCHAR arrays with automatic null termination
 * @tparam N Size of the destination buffer
 * @param s Destination buffer
 * @param format Format string
 * @return Number of characters written, or negative on error
 */
template <size_t N>
int safe_stprintf(TCHAR(&s)[N], TCHAR const* const format, ...)
{
	int result;
	va_list args;
	va_start(args, format);
	result = _vsntprintf(s, N - 1, format, args);
	va_end(args);
	if (result < 0)
	{
		s[0] = _T('\0');
	}
	else if (result == N - 1)
	{
		s[result] = _T('\0');
	}

	return result;
}

/**
 * @brief Raises a Windows error as a C++ structured exception
 * @param error The Windows error code to raise
 * @note Uses ATL's structured exception handling
 */
inline void CppRaiseException(unsigned long const error)
{
	struct _EXCEPTION_POINTERS* pExPtrs = nullptr;
	bool thrown = false;
	int exCode = 0;
	struct CppExceptionThrower
	{
		void operator()(int exCode, struct _EXCEPTION_POINTERS* pExPtrs)
		{
			throw CStructured_Exception(exCode, pExPtrs);
		}

		static bool assign(struct _EXCEPTION_POINTERS** to, struct _EXCEPTION_POINTERS* from)
		{
			*to = from;
			return true;
		}
	};

	__try
	{
		ATL::_AtlRaiseException(error, 0);
	}

	__except (CppExceptionThrower::assign(&pExPtrs, static_cast<struct _EXCEPTION_POINTERS*>(GetExceptionInformation())) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		exCode = GetExceptionCode();
		thrown = true;
	}

	if (thrown)
	{
		CppExceptionThrower()(exCode, pExPtrs);
	}
}

/**
 * @brief Checks a success condition and throws if failed
 * @param success Zero indicates failure, non-zero indicates success
 * @throws CStructured_Exception if success is zero
 */
inline void CheckAndThrow(int const success)
{
	if (!success)
	{
		CppRaiseException(GetLastError());
	}
}

/**
 * @brief Gets error text for any Windows error code
 * @param errorCode The Windows error code
 * @param pArgList Optional argument list for format inserts
 * @return Pointer to static buffer containing error text
 * @note Returns hex error code if no message found
 */
[[nodiscard]] inline LPTSTR GetAnyErrorText(DWORD errorCode, va_list* pArgList = nullptr)
{
	static TCHAR buffer[1 << 15];
	ZeroMemory(buffer, sizeof(buffer));
	if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | (pArgList == nullptr ? FORMAT_MESSAGE_IGNORE_INSERTS : 0), nullptr, errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), pArgList))
	{
		if (!FormatMessage(FORMAT_MESSAGE_FROM_HMODULE | (pArgList == nullptr ? FORMAT_MESSAGE_IGNORE_INSERTS : 0), GetModuleHandle(_T("NTDLL.dll")), errorCode, 0, buffer, sizeof(buffer) / sizeof(*buffer), pArgList))
		{
			safe_stprintf(buffer, _T("%#lx"), errorCode);
		}
	}

	return buffer;
}

/**
 * @brief Returns the last Win32 error as a string
 * @return Error message string, or empty string if no error
 */
[[nodiscard]] inline std::string GetLastErrorAsString()
{
	// Get the error message, if any.
	DWORD errorMessageID = ::GetLastError();
	if (errorMessageID == 0)
		return std::string();  // No error message has been recorded

	LPSTR messageBuffer = nullptr;
	size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, nullptr);

	std::string message(messageBuffer, size);

	// Free the buffer.
	LocalFree(messageBuffer);

	return message;
}

} // namespace error
} // namespace uffs

// Expose functions at global scope for backward compatibility
using uffs::error::safe_stprintf;
using uffs::error::CppRaiseException;
using uffs::error::CheckAndThrow;
using uffs::error::GetAnyErrorText;
using uffs::error::GetLastErrorAsString;

#endif // UFFS_ERROR_UTILS_HPP

